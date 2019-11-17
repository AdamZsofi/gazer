//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2019 Contributors to the Gazer project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Automaton/Cfa.h"

#include <llvm/ADT/Twine.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/ADT/DepthFirstIterator.h>

#include <boost/iterator/indirect_iterator.hpp>
#include <llvm/Support/raw_ostream.h>

using namespace gazer;

Cfa::Cfa(GazerContext &context, std::string name, AutomataSystem& parent)
    : mName(std::move(name)), mContext(context), mParent(parent)
{
    // Create an entry and exit location
    Location* entry = this->createLocation();
    Location* exit = this->createLocation();

    mEntry = entry;
    mExit = exit;
}

// Location and edge construction
//-------------------------------------------------------------------------
Location* Cfa::createLocation()
{
    auto loc = new Location(mLocationIdx++, this);
    mLocations.emplace_back(loc);
    mLocationNumbers[loc->getId()] = loc;

    return loc;
}

Location* Cfa::createErrorLocation()
{
    auto loc = new Location(mLocationIdx++, this, Location::Error);
    mLocations.emplace_back(loc);
    mErrorLocations.emplace_back(loc);
    mLocationNumbers[loc->getId()] = loc;

    return loc;
}

Location* Cfa::findLocationById(unsigned id)
{
    return mLocationNumbers.lookup(id);
}

AssignTransition *Cfa::createAssignTransition(
    Location *source, Location *target,
    ExprPtr guard,
    std::vector<VariableAssignment> assignments
) {
    if (guard == nullptr) {
        guard = BoolLiteralExpr::True(mContext);
    }

    auto edge = new AssignTransition(source, target, std::move(guard), std::move(assignments));
    mTransitions.emplace_back(edge);
    source->addOutgoing(edge);
    target->addIncoming(edge);

    return edge;
}

CallTransition *Cfa::createCallTransition(Location *source, Location *target, const ExprPtr& guard,
    Cfa *callee, std::vector<VariableAssignment> inputArgs, std::vector<VariableAssignment> outputArgs)
{
    assert(source != nullptr);
    assert(target != nullptr);

    auto call = new CallTransition(source, target, guard, callee, std::move(inputArgs), std::move(outputArgs));
    mTransitions.emplace_back(call);
    source->addOutgoing(call);
    target->addIncoming(call);

    return call;
}

CallTransition *Cfa::createCallTransition(
    Location *source,
    Location *target,
    Cfa *callee,
    std::vector<VariableAssignment> inputArgs,
    std::vector<VariableAssignment> outputArgs)
{
    return createCallTransition(
        source,
        target,
        BoolLiteralExpr::True(mContext),
        callee,
        std::move(inputArgs),
        std::move(outputArgs)
    );
}

void Cfa::addErrorCode(Location* location, ExprPtr errorCodeExpr)
{
    assert(location->isError());
    mErrorFieldExprs[location] = errorCodeExpr;
}

ExprPtr Cfa::getErrorFieldExpr(Location* location)
{
    assert(location->isError());
    auto expr = mErrorFieldExprs.lookup(location);

    assert(expr != nullptr);
    return expr;
}

// Member variables
//-----------------------------------------------------------------------------

Variable *Cfa::createInput(const std::string& name, Type& type)
{
    Variable* variable = this->createMemberVariable(name, type);
    mInputs.push_back(variable);

    return variable;
}

void Cfa::addOutput(Variable* variable)
{
    mOutputs.push_back(variable);
}

Variable *Cfa::createLocal(const std::string& name, Type& type)
{
    Variable* variable = this->createMemberVariable(name, type);
    mLocals.push_back(variable);

    return variable;
}

Variable *Cfa::createMemberVariable(const std::string& name, Type &type)
{
    std::string baseName = mName + "/" + name;
    auto newName = baseName;

    while (mContext.getVariable(newName) != nullptr) {
        newName = baseName + "_" + std::to_string(mTmp++);
    }

    auto variable = mContext.createVariable(newName, type);
    mSymbolNames[variable] = name;

    return variable;
}

size_t Cfa::getInputNumber(gazer::Variable* variable) const
{
    auto it = std::find(mInputs.begin(), mInputs.end(), variable);
    assert(it != mInputs.end() && "Variable must be present in the input list!");

    return std::distance(mInputs.begin(), it);
}

size_t Cfa::getOutputNumber(gazer::Variable* variable) const
{
    auto it = std::find(mOutputs.begin(), mOutputs.end(), variable);
    assert(it != mOutputs.end() && "Variable must be present in the output list!");

    return std::distance(mOutputs.begin(), it);
}

bool Cfa::isOutput(Variable* variable) const
{
    return std::find(mOutputs.begin(), mOutputs.end(), variable) != mOutputs.end();
}

Variable* Cfa::findVariableByName(const std::vector<Variable*>& vec, llvm::StringRef name) const
{
    auto variableName = (llvm::Twine(mName, "/") + name).str();
    Variable* variable = mContext.getVariable(variableName);
    if (variable == nullptr) {
        return nullptr;
    }

    // We must also make sure that the required vector contains said variable.
    if (std::find(vec.begin(), vec.end(), variable) == vec.end()) {
        return nullptr;
    }

    return variable;
}

Variable* Cfa::findInputByName(llvm::StringRef name) const
{
    return findVariableByName(mInputs, name);
}

Variable* Cfa::findLocalByName(llvm::StringRef name) const
{
    return findVariableByName(mLocals, name);
}

Variable* Cfa::findOutputByName(llvm::StringRef name) const
{
    return findVariableByName(mOutputs, name);
}

// Transformations
//-----------------------------------------------------------------------------

void Cfa::disconnectLocation(Location* location)
{
    for (Transition* edge : location->incoming()) {
        edge->mTarget = nullptr;
        edge->mSource->removeOutgoing(edge);
    }

    for (Transition* edge : location->outgoing()) {
        edge->mSource = nullptr;
        edge->mTarget->removeIncoming(edge);
    }

    location->mIncoming.clear();
    location->mOutgoing.clear();
}

void Cfa::disconnectEdge(Transition* edge)
{
    edge->getSource()->removeOutgoing(edge);
    edge->getTarget()->removeIncoming(edge);

    edge->mSource = nullptr;
    edge->mTarget = nullptr;
}

void Cfa::removeUnreachableLocations()
{
    llvm::df_iterator_default_set<Location*> visited;
    auto begin = llvm::df_ext_begin(*this, visited);
    auto end = llvm::df_ext_end(*this, visited);

    for (auto it = begin; it != end; ++it) {
        // The DFS algorithm is executed by running the iterators.
    }

    std::vector<Location*> unreachable;
    for (auto& loc : mLocations) {
        if (visited.count(&*loc) == 0) {
            this->disconnectLocation(&*loc);
        }
    }

    this->clearDisconnectedElements();
}

void Cfa::clearDisconnectedElements()
{
    mLocations.erase(std::remove_if(mLocations.begin(), mLocations.end(), [](auto& loc) {
        return loc->mIncoming.empty() && loc->mOutgoing.empty();
    }), mLocations.end());

    mTransitions.erase(std::remove_if(mTransitions.begin(), mTransitions.end(), [](auto& edge) {
        return edge->mSource == nullptr || edge->mTarget == nullptr;
    }), mTransitions.end());
}

Cfa::~Cfa() {}

// Support code for locations
//-----------------------------------------------------------------------------

void Location::addIncoming(Transition *edge)
{
    assert(edge->getTarget() == this);
    mIncoming.push_back(edge);
}

void Location::addOutgoing(Transition *edge)
{
    assert(edge->getSource() == this);
    mOutgoing.push_back(edge);
}

void Location::removeIncoming(Transition *edge)
{
    mIncoming.erase(std::remove(mIncoming.begin(), mIncoming.end(), edge), mIncoming.end());
}

void Location::removeOutgoing(Transition *edge)
{
    mOutgoing.erase(std::remove(mOutgoing.begin(), mOutgoing.end(), edge), mOutgoing.end());
}

// Transitions
//-----------------------------------------------------------------------------
AssignTransition::AssignTransition(
    Location *source, Location *target, ExprPtr guard,
    std::vector<VariableAssignment> assignments
) : Transition(source, target, std::move(guard), Transition::Edge_Assign), mAssignments(std::move(assignments))
{}

CallTransition::CallTransition(
    Location *source, Location *target, ExprPtr guard, Cfa *callee,
    std::vector<VariableAssignment> inputArgs, std::vector<VariableAssignment> outputArgs
) : Transition(source, target, guard, Transition::Edge_Call), mCallee(callee),
    mInputArgs(std::move(inputArgs)), mOutputArgs(std::move(outputArgs))
{
    assert(source != nullptr);
    assert(target != nullptr);
    assert(callee != nullptr);
    assert(callee->getNumInputs() == mInputArgs.size());
}

// Automata system
//-----------------------------------------------------------------------------

AutomataSystem::AutomataSystem(GazerContext &context)
    : mContext(context)
{}

Cfa *AutomataSystem::createCfa(std::string name)
{
    Cfa* cfa = new Cfa(mContext, name, *this);
    mAutomata.emplace_back(cfa);

    return cfa;
}

Cfa* AutomataSystem::getAutomatonByName(llvm::StringRef name) const
{
    auto result = std::find_if(begin(), end(), [name](Cfa& cfa) {
        return cfa.getName() == name;
    });

    if (result == end()) {
        return nullptr;
    }

    return &*result;
}

void AutomataSystem::setMainAutomaton(Cfa* cfa)
{
    assert(&cfa->getParent() == this);
    mMainAutomaton = cfa;
}
