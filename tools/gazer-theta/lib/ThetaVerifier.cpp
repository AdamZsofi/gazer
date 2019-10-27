#include "ThetaVerifier.h"
#include "ThetaCfaGenerator.h"

#include "gazer/Automaton/Cfa.h"
#include "gazer/Support/SExpr.h"
#include "gazer/Core/LiteralExpr.h"

#include <llvm/ADT/Twine.h>
#include <llvm/ADT/APInt.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/CommandLine.h>

using namespace gazer;
using namespace gazer::theta;

using llvm::cast;
using llvm::dyn_cast;

namespace
{

llvm::cl::opt<bool> PrintRawCex("print-raw-cex", llvm::cl::desc("Print the raw counterexample from theta."));

class ThetaVerifierImpl
{
public:
    explicit ThetaVerifierImpl(AutomataSystem& system, ThetaSettings settings, CfaTraceBuilder& traceBuilder)
        : mSystem(system), mSettings(settings), mTraceBuilder(traceBuilder)
    {}

    void writeSystem(llvm::raw_ostream& os);

    /// Runs the theta model checker on the input file.
    std::unique_ptr<VerificationResult> execute(llvm::StringRef input);

private:
    AutomataSystem& mSystem;
    ThetaSettings mSettings;
    CfaTraceBuilder& mTraceBuilder;
    ThetaNameMapping mNameMapping;

    std::unique_ptr<Trace> parseCex(llvm::StringRef cex, unsigned* errorCode);
};

} // end anonymous namespace


auto ThetaVerifierImpl::execute(llvm::StringRef input) -> std::unique_ptr<VerificationResult>
{
    llvm::outs() << "  Building theta configuration.\n";
    // First, find Java.
    auto java = llvm::sys::findProgramByName("java");
    if (std::error_code javaEc = java.getError()) {
        return VerificationResult::CreateInternalError("Could not find java. " + javaEc.message());
    }

    // Create the temp file which we will use to dump theta outputs into.
    llvm::SmallString<128> outputsFile;
    auto outputsFileEc = llvm::sys::fs::createTemporaryFile("gazer_theta", "", outputsFile);
    
    if (outputsFileEc) {
        llvm::errs() << outputsFileEc.message() << "\n";
        return VerificationResult::CreateInternalError("Could not create temporary file. " + outputsFileEc.message());
    }

    // Make sure that we have the theta jar and the Z3 library.
    llvm::SmallString<128> thetaPath("theta/theta-cfa-cli.jar");
    llvm::SmallString<128> z3Path("theta/lib");

    llvm::sys::fs::make_absolute(thetaPath);
    llvm::sys::fs::make_absolute(z3Path);

    if (!llvm::sys::fs::exists(thetaPath)) {
        return VerificationResult::CreateInternalError(
            "Could not execute theta verifier. Tool was not found in path '" + thetaPath + "'."
        );
    }

    if (!llvm::sys::fs::exists(z3Path)) {
        return VerificationResult::CreateInternalError(
            "Could not execute theta verifier."
            "Libraries required by theta (libz3.so) were not found in path '" + z3Path + "'."
        );
    }

    std::string javaLibPath = ("-Djava.library.path=" + z3Path).str();

    llvm::StringRef args[] = {
        "java",
        javaLibPath,
        "-jar",
        thetaPath,
        "--model", input,
        "--domain",  mSettings.domain,
        "--encoding", mSettings.encoding,
        "--initprec", mSettings.initPrec,
        "--precgranularity", mSettings.precGranularity,
        "--predsplit", mSettings.predSplit,
        "--refinement", mSettings.refinement,
        "--search", mSettings.search,
        "--maxenum", mSettings.maxEnum,
        "--cex",
        "--loglevel", "RESULT"
    };

    std::string ldLibPathEnv = ("LD_LIBRARY_PATH=" + z3Path).str();
    llvm::ArrayRef<llvm::StringRef> env = {
        llvm::StringRef(ldLibPathEnv)
    };

    llvm::Optional<llvm::StringRef> redirects[] = {
        llvm::None,         // stdin
        outputsFile.str(),  // stdout
        llvm::None          // stderr
    };

    llvm::outs() << "  Running theta...\n";
    std::string thetaErrors;
    int returnCode = llvm::sys::ExecuteAndWait(
        *java,
        args,
        env,
        redirects,
        mSettings.timeout,
        /*memoryLimit=*/0,
        &thetaErrors
    );

    if (returnCode == -1) {
        return VerificationResult::CreateInternalError("Theta execution failed. " + thetaErrors);
    }

    if (returnCode == -2) {
        return VerificationResult::CreateTimeout();
    }

    if (returnCode != 0) {
        return VerificationResult::CreateInternalError("Theta returned a non-zero exit code. " + thetaErrors);
    }

    // Grab the output file's contents.
    auto buffer = llvm::MemoryBuffer::getFile(outputsFile);
    if (auto errorCode = buffer.getError()) {
        return VerificationResult::CreateInternalError("Theta execution failed. " + errorCode.message());
    }

    llvm::StringRef thetaOutput = (*buffer)->getBuffer();

    // We have the output from theta, it is now time to parse
    // the result and the possible counterexample.
    if (thetaOutput.startswith("(SafetyResult Safe)")) {
        return VerificationResult::CreateSuccess();
    } else if (thetaOutput.startswith("(SafetyResult Unsafe")) {
        // Parse the counterexample
        auto cexPos = thetaOutput.find("(Trace");
        if (cexPos == llvm::StringRef::npos) {
            llvm::errs() << "Theta returned no parseable counterexample.\n";
            return VerificationResult::CreateFail(VerificationResult::GeneralFailureCode, nullptr);
        }

        auto cex = thetaOutput.substr(cexPos).trim();

        if (PrintRawCex) {
            llvm::outs() << cex << "\n";
        }

        unsigned ec = VerificationResult::GeneralFailureCode;
        auto trace = this->parseCex(cex, &ec);

        return VerificationResult::CreateFail(ec, std::move(trace));
    }

    return VerificationResult::CreateUnknown();
}

void ThetaVerifierImpl::writeSystem(llvm::raw_ostream& os)
{
    theta::ThetaCfaGenerator generator{mSystem};
    generator.write(os, mNameMapping);
}

static void reportInvalidCex(llvm::StringRef message, llvm::StringRef cex, sexpr::Value* value = nullptr)
{
    llvm::errs() << "Could not parse theta counterexample: " <<  message << "\n";
    if (value != nullptr) {
        llvm::errs() << "Value is: ";
        value->print(llvm::errs());
        llvm::errs() << "\n";
    }
    llvm::errs() << "Raw counterexample is: " << cex << "\n";
}

std::unique_ptr<Trace> ThetaVerifierImpl::parseCex(llvm::StringRef cex, unsigned* errorCode)
{
    // TODO: The whole parsing process is very fragile. We should introduce some proper error cheks.
    auto trace = sexpr::parse(cex);
    assert(trace->isList());

    std::vector<Location*> states;
    std::vector<std::vector<VariableAssignment>> actions;
    llvm::StringRef initLocName = trace->asList()[1]->asList()[1]->asAtom();

    states.push_back(mNameMapping.locations[initLocName]);

    for (size_t i = 3; i < trace->asList().size(); i += 2) {
        auto& stateList = trace->asList()[i]->asList();
        if (stateList[0]->asAtom() != "CfaState") {
            reportInvalidCex("expected 'CfaState' atom in list", cex, &*(trace->asList()[i]));
            return nullptr;
        }

        // Theta may insert unnamed locations, thus the format is either
        // (CfaState locName (ExplState ...)) or (CfaState (ExplState ...)).
        // In the latter case, we must skip this location as it is not present.
        // anywhere within gazer.
        if (stateList[1]->isList()) {
            continue;
        }

        auto& actionList = stateList[2]->asList();

        if (actionList.size() < 2) {
            // `(CfaState (ExplState))`, nothing to do here.
            actions.push_back({});
        } else {
            std::vector<VariableAssignment> assigns;
            for (size_t j = 1; j < actionList.size(); ++j) {
                llvm::StringRef varName = actionList[j]->asList()[0]->asAtom();
                llvm::StringRef value = actionList[j]->asList()[1]->asAtom();

                Variable* variable = mNameMapping.variables.lookup(varName);
                assert(variable != nullptr && "Each variable must be present in the theta name mapping!");

                Variable* origVariable = mNameMapping.inlinedVariables.lookup(variable);
                if (origVariable == nullptr) {
                    origVariable = variable;
                }

                ExprPtr rhs;
                Type& varTy = origVariable->getType();

                switch (varTy.getTypeID()) {
                    case Type::IntTypeID: {
                        long long int intVal;
                        if (!value.getAsInteger(10, intVal)) {
                            rhs = IntLiteralExpr::Get(cast<IntType>(varTy), intVal);
                        }
                        break;
                    }
                    case Type::BvTypeID: {
                        llvm::APInt intVal;
                        if (!value.getAsInteger(10, intVal)) {
                            auto& bvTy = cast<BvType>(varTy);
                            rhs = BvLiteralExpr::Get(bvTy, intVal.zextOrTrunc(bvTy.getWidth()));
                        }
                        break;
                    }
                    case Type::BoolTypeID: {
                        if (value.equals_lower("true")) {
                            rhs = BoolLiteralExpr::True(varTy.getContext());
                        } else if (value.equals_lower("false")) {
                            rhs = BoolLiteralExpr::False(varTy.getContext());
                        }
                        break;
                    }
                    default:
                        break;
                }

                if (rhs == nullptr) {
                    reportInvalidCex("expected a valid integer or boolean value", cex, &*(actionList[j]->asList()[1]));
                    return nullptr;
                }

                assigns.push_back({origVariable, rhs});
            }

            actions.push_back(assigns);
        }

        llvm::StringRef locName = stateList[1]->asAtom();

        Location* loc = mNameMapping.locations.lookup(locName);
        Location* origLoc = mNameMapping.inlinedLocations.lookup(loc);

        if (origLoc == nullptr) {
            origLoc = loc;
        }

        states.push_back(origLoc);
    }

    // Find the value of the error field variable and extract the error code.
    auto& lastAction = actions.back();
    auto errAssign = std::find_if(lastAction.begin(), lastAction.end(), [this](VariableAssignment& assignment) {
        return assignment.getVariable() == mNameMapping.errorFieldVariable;
    });

    assert(errAssign != lastAction.end()
        && "The error field variable must be present in the last element of the trace!");

    auto errorExpr = llvm::dyn_cast_or_null<LiteralExpr>(errAssign->getValue());
    assert(errorExpr != nullptr && "The error field must be present in the trace as a literal expression!");

    unsigned ec = 0;
    if (auto bvLit = llvm::dyn_cast<BvLiteralExpr>(errorExpr)) {
        ec = bvLit->getValue().getLimitedValue();
    } else if (auto intLit = llvm::dyn_cast<IntLiteralExpr>(errorExpr)) {
        ec = intLit->getValue();
    } else {
        llvm_unreachable("Invalid error field type!");
    }

    *errorCode = ec;

    return mTraceBuilder.build(states, actions);
}

auto ThetaVerifier::check(AutomataSystem& system, CfaTraceBuilder& traceBuilder)
    -> std::unique_ptr<VerificationResult>
{
    llvm::outs() << "Running theta verification backend.\n";

    std::error_code errors;
    ThetaVerifierImpl impl(system, mSettings, traceBuilder);

    // Create a temporary file to write into.
    llvm::SmallString<128> outputFile;
    errors = llvm::sys::fs::createTemporaryFile("gazer_theta_cfa", "theta", outputFile);

    if (errors) {
        return VerificationResult::CreateInternalError(
            "Failed to execute theta verifier. Could not create temporary file. " + errors.message()
        );
    }

    llvm::outs() << "  Writing theta CFA into '" << outputFile << "'.\n";
    llvm::raw_fd_ostream thetaOutput(outputFile, errors);

    if (errors) {
        return VerificationResult::CreateInternalError(
            "Failed to execute theta verifier. Could not open output file. " + errors.message()
        );
    }

    impl.writeSystem(thetaOutput);

    return impl.execute(outputFile);
}
