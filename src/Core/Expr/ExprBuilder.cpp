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
#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"

#include <llvm/ADT/APInt.h>

using namespace gazer;

ExprPtr ExprBuilder::createTupleConstructor(TupleType& type, const ExprVector& members)
{
    return TupleConstructExpr::Create(type, members);
}

ExprPtr ExprBuilder::TupSel(const ExprPtr& tuple, unsigned index)
{
    return TupleSelectExpr::Create(tuple, index);
}

std::unique_ptr<ExprBuilder> gazer::CreateExprBuilder(GazerContext& context)
{
    return std::unique_ptr<ExprBuilder>(new ExprBuilder(context));
}
