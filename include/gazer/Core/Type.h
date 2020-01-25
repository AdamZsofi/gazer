//==- Type.h - Gazer expression types ---------------------------*- C++ -*--==//
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
#ifndef GAZER_CORE_TYPE_H
#define GAZER_CORE_TYPE_H

#include "gazer/Core/GazerContext.h"

#include <llvm/Support/Casting.h>
#include <llvm/ADT/iterator.h>
#include <llvm/ADT/ArrayRef.h>

#include <boost/iterator/indirect_iterator.hpp>

#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <iosfwd>

namespace llvm {
    struct fltSemantics;
    class raw_ostream;
}

namespace gazer
{

class GazerContext;

/// Base class for all gazer types.
class Type
{
public:
    enum TypeID
    {
        // Primitive types
        BoolTypeID = 0,
        IntTypeID,
        BvTypeID,
        FloatTypeID,
        RealTypeID,

        // Composite types
        ArrayTypeID,
        TupleTypeID,
        FunctionTypeID
    };

    static constexpr int FirstPrimitive = BoolTypeID;
    static constexpr int LastPrimitive = FloatTypeID;
    static constexpr int FirstComposite = ArrayTypeID;
    static constexpr int LastComposite = FunctionTypeID;
protected:
    explicit Type(GazerContext& context, TypeID id)
        : mContext(context), mTypeID(id)
    {}

public:
    Type(const Type&) = delete;
    Type& operator=(const Type&) = delete;

    [[nodiscard]] GazerContext& getContext() const { return mContext; }
    [[nodiscard]] TypeID getTypeID() const { return mTypeID; }
    
    [[nodiscard]] bool isPrimitiveType() const {
        return mTypeID >= FirstPrimitive && mTypeID <= LastPrimitive;
    }
    [[nodiscard]] bool isCompositeType() const {
        return mTypeID >= FirstComposite && mTypeID <= LastComposite;
    }

    [[nodiscard]] bool isBoolType() const { return getTypeID() == BoolTypeID; }
    [[nodiscard]] bool isIntType() const { return getTypeID() == IntTypeID; }
    [[nodiscard]] bool isBvType() const { return getTypeID() == BvTypeID; }
    [[nodiscard]] bool isFloatType() const { return getTypeID() == FloatTypeID; }
    [[nodiscard]] bool isRealType() const { return getTypeID() == RealTypeID; }
    [[nodiscard]] bool isArrayType() const { return getTypeID() == ArrayTypeID; }
    [[nodiscard]] bool isTupleType() const { return getTypeID() == TupleTypeID; }

    [[nodiscard]] bool isArithmetic() const { return isIntType() || isRealType(); }

    //bool isPointerType() const { return getTypeID() == PointerTypeID; }

    bool equals(const Type* other) const;

    bool operator==(const Type& other) const { return equals(&other); }
    bool operator!=(const Type& other) const { return !equals(&other); }

    [[nodiscard]] std::string getName() const;

private:
    GazerContext& mContext;
    TypeID mTypeID;
};

llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const Type& type);

// Primitive type declarations
//===----------------------------------------------------------------------===//

class BoolType final : public Type
{
    friend class GazerContextImpl;
protected:
    explicit BoolType(GazerContext& context)
        : Type(context, BoolTypeID)
    {}
public:
    static BoolType& Get(GazerContext& context);

    static bool classof(const Type* type) {
        return type->getTypeID() == BoolTypeID;
    }
};

class BvType final : public Type
{
    friend class GazerContextImpl;
protected:
    BvType(GazerContext& context, unsigned width)
        : Type(context, BvTypeID), mWidth(width)
    {}
public:
    [[nodiscard]] unsigned getWidth() const { return mWidth; }

    static BvType& Get(GazerContext& context, unsigned width);

    static bool classof(const Type* type) {
        return type->getTypeID() == BvTypeID;
    }

    static bool classof(const Type& type) {
        return type.getTypeID() == BvTypeID;
    }
private:
    unsigned mWidth;
};

/// Unbounded, mathematical integer type.
class IntType final : public Type
{
    friend class GazerContextImpl;
protected:
    explicit IntType(GazerContext& context)
        : Type(context, IntTypeID)
    {}
public:
    static IntType& Get(GazerContext& context);

    static bool classof(const Type* type) {
        return type->getTypeID() == IntTypeID;
    }

    static bool classof(const Type& type) {
        return type.getTypeID() == IntTypeID;
    }
private:
};

class RealType final : public Type
{
    friend class GazerContextImpl;
protected:
    explicit RealType(GazerContext& context)
        : Type(context, RealTypeID)
    {}
public:
    static RealType& Get(GazerContext& context);

    static bool classof(const Type* type) {
        return type->getTypeID() == RealTypeID;
    }

    static bool classof(const Type& type) {
        return type.getTypeID() == RealTypeID;
    }
};

/// Represents an IEEE-754 floating point type.
class FloatType final : public Type
{
    friend class GazerContextImpl;
public:
    enum FloatPrecision
    {
        Half = 16,
        Single = 32,
        Double = 64,
        Quad = 128
    };

    static constexpr unsigned SignificandBitsInHalfTy   = 11;
    static constexpr unsigned SignificandBitsInSingleTy = 24;
    static constexpr unsigned SignificandBitsInDoubleTy = 53;
    static constexpr unsigned SignificandBitsInQuadTy   = 113;

    static constexpr unsigned ExponentBitsInHalfTy   = 5;
    static constexpr unsigned ExponentBitsInSingleTy = 8;
    static constexpr unsigned ExponentBitsInDoubleTy = 11;
    static constexpr unsigned ExponentBitsInQuadTy   = 15;

protected:
    FloatType(GazerContext& context, FloatPrecision precision)
        : Type(context, FloatTypeID), mPrecision(precision)
    {}

public:
    [[nodiscard]] FloatPrecision getPrecision() const { return mPrecision; }
    [[nodiscard]] const llvm::fltSemantics& getLLVMSemantics() const;
    [[nodiscard]] unsigned getWidth() const { return mPrecision; }

    unsigned getExponentWidth() const;
    unsigned getSignificandWidth() const;

    static FloatType& Get(GazerContext& context, FloatPrecision precision);

    static bool classof(const Type* type) {
        return type->getTypeID() == FloatTypeID;
    }
    static bool classof(const Type& type) {
        return type.getTypeID() == FloatTypeID;
    }

private:
    FloatPrecision mPrecision;
    static FloatType HalfTy, SingleTy, DoubleTy, QuadTy;
};

// Composite types
//===----------------------------------------------------------------------===//

class CompositeType : public Type
{
protected:
    CompositeType(GazerContext& context, TypeID id, std::vector<Type*> subTypes);

public:
    Type& getSubType(unsigned idx) const
    {
        assert(idx <= mSubTypes.size());
        return *mSubTypes[idx];
    }

private:
    std::vector<Type*> mSubTypes;
};

/// Represents an array type with arbitrary index and element types.
class ArrayType final : public CompositeType
{
    ArrayType(GazerContext& context, std::vector<Type*> types);
public:
    Type& getIndexType() const { return getSubType(0); }
    Type& getElementType() const { return getSubType(1); }

    static ArrayType& Get(Type& indexType, Type& elementType);

    static bool classof(const Type* type) {
        return type->getTypeID() == ArrayTypeID;
    }
};

class TupleType final : public Type
{
    TupleType(GazerContext& context, std::vector<Type*> subtypes)
        : Type(context, TupleTypeID), mSubtypeList(std::move(subtypes))
    {
        assert(!mSubtypeList.empty());
        assert(mSubtypeList.size() >= 2);
    }
public:
    Type& getTypeAtIndex(unsigned idx) const { return *mSubtypeList[idx]; }
    unsigned getNumSubtypes() const { return mSubtypeList.size(); }

    using subtype_iterator = boost::indirect_iterator<std::vector<Type*>::const_iterator>;
    subtype_iterator subtype_begin() const { return boost::make_indirect_iterator(mSubtypeList.begin()); }
    subtype_iterator subtype_end() const { return boost::make_indirect_iterator(mSubtypeList.end()); }

    template<class... Tys>
    static typename std::enable_if<(std::is_base_of_v<Type, Tys> && ...), TupleType&>::type
    Get(Type& first, Tys&... tail)
    {
        std::vector<Type*> subtypeList({ &first, &tail... });
        return TupleType::Get(subtypeList);
    }

    static bool classof(const Type* type) {
        return type->getTypeID() == TupleTypeID;
    }

private:
    static TupleType& Get(std::vector<Type*> subtypes);

private:
    std::vector<Type*> mSubtypeList;
};

class FunctionType : public Type
{
    FunctionType(GazerContext& context, Type* resultType, std::vector<Type*> paramTypes)
        : Type(context, FunctionTypeID), mResultType(resultType), mParamTypeList(paramTypes)
    {}

public:
    template<class... Tys>
    static typename std::enable_if<std::is_base_of_v<Type, Tys...>, FunctionType&>::type
    Get(Type& result, Tys&... params)
    {
        std::array<Type*, sizeof...(Tys)> paramTypes({ &params... });
        return FunctionType::Get(&result, paramTypes);
    }

    unsigned getArity() const { return mParamTypeList.size(); }

    Type& getResultType() const { return *mResultType; }
    Type& getParamType(unsigned idx) const;

private:
    FunctionType& Get(Type* resultType, llvm::ArrayRef<Type*> paramTypes);

private:
    Type* mResultType;
    std::vector<Type*> mParamTypeList;
};

}

#endif
