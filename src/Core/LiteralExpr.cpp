#include "gazer/Core/LiteralExpr.h"

#include <map>

using namespace gazer;

std::shared_ptr<IntLiteralExpr> IntLiteralExpr::get(int value)
{
    static std::map<int, std::shared_ptr<IntLiteralExpr>> exprs;
    
    auto result = exprs.find(value);
    if (result == exprs.end()) {
        auto ptr = std::shared_ptr<IntLiteralExpr>(new IntLiteralExpr(value));
        exprs[value] = ptr;

        return ptr;
    }

    return result->second;
}

void IntLiteralExpr::print(std::ostream& os) const {
    os << mValue;
}
