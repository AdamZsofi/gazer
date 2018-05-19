#include "gazer/Core/Automaton.h"
#include "gazer/Core/SymbolTable.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/ExprTypes.h"

#include <fmt/format.h>

#include <sstream>
#include <iostream>

using namespace gazer;

int main(int argc, char* argv[])
{
    SymbolTable st;
    Variable& x = st.create("x", IntType::get());
    Variable& y = st.create("y", IntType::get());
    Variable& c = st.create("c", BoolType::get());

    Automaton cfa("entry", "exit");
    Location& loc1 = cfa.createLocation();
    Location& loc2 = cfa.createLocation();

    auto condition = LtExpr::Create(
        y.getRefExpr(), IntLiteralExpr::get(10)
    );

    cfa.insertEdge(AssignEdge::Create(cfa.entry(), loc1, {
        mk_assign(x, IntLiteralExpr::get(0)),
        mk_assign(y, IntLiteralExpr::get(0))
    }));
    cfa.insertEdge(AssumeEdge::Create(loc1, loc2, condition));
    cfa.insertEdge(AssignEdge::Create(loc2, loc1, {
        {x, AddExpr::Create(x.getRefExpr(), y.getRefExpr()) },
        {y, AddExpr::Create(y.getRefExpr(), IntLiteralExpr::get(1)) }
    }));
    cfa.insertEdge(AssumeEdge::Create(loc1, cfa.exit(), NotExpr::Create(condition)));

    std::cerr << "digraph G {\n";
    for (auto& loc : cfa.locs()) {
        std::cerr << fmt::format("\tnode_{0} [label=\"{1}\"];\n",
            static_cast<void*>(loc.get()), loc->getName()
        );
    }
    std::cerr << "\n";
    for (auto& edge : cfa.edges()) {
        std::stringstream ss;
        edge->print(ss);

        std::cerr << fmt::format("\tnode_{0} -> node_{1} [label=\"{2}\"];\n",
            static_cast<void*>(&edge->getSource()),
            static_cast<void*>(&edge->getTarget()),
            ss.str()
        );
    }
    std::cerr << "};\n";


    return 0;
}
