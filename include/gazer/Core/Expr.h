#ifndef _GAZER_CORE_EXPR_H
#define _GAZER_CORE_EXPR_H

#include "gazer/Core/Type.h"

#include <memory>
#include <string>
#include <iosfwd>

namespace gazer
{

/**
 * Base class for all gazer expressions.
 */
class Expr
{
    friend class ExprRef;
public:
    enum ExprKind
    {
        // Nullary
        Literal = 0,
        VarRef,

        // Unary logic
        Not,

        // Binary arithmetic
        Add,
        Sub,
        Mul,
        Div,

        // Binary logic
        And,
        Or,
        Xor,

        // Compare
        Eq,
        NotEq,
        Lt,
        LtEq,
        Gt,
        GtEq,

        // Ternary
        Select
    };

    static constexpr int FirstUnary = Not;
    static constexpr int LastUnary = Not;
    static constexpr int FirstBinaryArithmetic = Add;
    static constexpr int LastBinaryArithmetic = Div;
    static constexpr int FirstLogic = And;
    static constexpr int LastLogic = Xor;
    static constexpr int FirstCompare = Eq;
    static constexpr int LastCompare = GtEq;

    static constexpr int FirstExprKind = Literal;
    static constexpr int LastExprKind = Select;

protected:
    Expr(ExprKind kind, const Type& type)
        : mKind(kind), mType(type)
    {}

public:
    ExprKind getKind() const { return mKind; }
    const Type& getType() const { return mType; }

    bool isNullary() const { return mKind <= FirstUnary; }
    bool isUnary() const { return FirstUnary <= mKind && mKind <= LastUnary; }

    bool isArithmetic() const {
        return FirstBinaryArithmetic <= mKind && mKind <= LastBinaryArithmetic;
    }
    bool isLogic() const {
        return FirstLogic <= mKind && mKind <= LastLogic;
    }
    bool isCompare() const {
        return FirstCompare <= mKind && mKind <= LastCompare;
    }

    virtual void print(std::ostream& os) const = 0;

    virtual ~Expr() {}

public:
    static std::string getKindName(ExprKind kind);

private:
    ExprKind mKind;
    const Type& mType;
};

using ExprPtr = std::shared_ptr<Expr>;

std::ostream& operator<<(std::ostream& os, const Expr& expr);

/**
 * Base class for all literals.
 */
class LiteralExpr : public Expr
{
protected:
    LiteralExpr(const Type& type)
        : Expr(Literal, type)
    {}
public:
    static bool classof(const Expr* expr) {
        return expr->getKind() == Literal;
    }
};

/**
 * Base class for all expressions holding one or more operands.
 */
class NonNullaryExpr : public Expr
{
protected:
    NonNullaryExpr(ExprKind kind, const Type& type, std::vector<ExprPtr> operands)
        : Expr(kind, type), mOperands(operands)
    {
        assert(mOperands.size() >= 1 && "Non-nullary expressions must have at least one operand.");
    }
public:
    virtual void print(std::ostream& os) const override;

    //---- Operand handling ----//
    using op_iterator = typename std::vector<ExprPtr>::iterator;
    using op_const_iterator = typename std::vector<ExprPtr>::const_iterator;

    op_iterator op_begin() { return mOperands.begin(); }
    op_iterator op_end() { return mOperands.end(); }

    op_const_iterator op_begin() const { return mOperands.begin(); }
    op_const_iterator op_end() const { return mOperands.end(); }

    llvm::iterator_range<op_iterator> operands() {
        return llvm::make_range(op_begin(), op_end());
    }
    llvm::iterator_range<op_const_iterator> operands() const {
        return llvm::make_range(op_begin(), op_end());
    }

    size_t getNumOperands() const { return mOperands.size(); }
    ExprPtr getOperand(size_t idx) const { return mOperands[idx]; }

public:
    static bool classof(const Expr* expr) {
        return expr->getKind() >= FirstUnary;
    }

    static bool classof(const Expr& expr) {
        return expr.getKind() >= FirstUnary;
    }

private:
    std::vector<ExprPtr> mOperands;
};

}

#endif
