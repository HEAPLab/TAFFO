#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "gtest/gtest.h"
#include "MultiValueMap.h"

namespace {

using namespace taffo;
using namespace llvm;


class MultiValueMapTest : public testing::Test {
protected:
  LLVMContext Context;
  Constant *ConstantV;
  std::unique_ptr<BitCastInst> BitcastV;
  std::unique_ptr<BinaryOperator> AddV;
  std::unique_ptr<ICmpInst> CmpV;
  std::unique_ptr<BinaryOperator> SubV;
    
  MultiValueMapTest() :
      ConstantV(ConstantInt::get(Type::getInt32Ty(Context), 0)),
      BitcastV(new BitCastInst(ConstantV, Type::getInt32Ty(Context))),
      AddV(BinaryOperator::CreateAdd(ConstantV, ConstantV)),
      CmpV(new ICmpInst(ICmpInst::Predicate::ICMP_EQ, ConstantV, ConstantV)),
      SubV(BinaryOperator::CreateSub(ConstantV, ConstantV)) {}
};


TEST_F(MultiValueMapTest, TestEmpty) {
  MultiValueMap<Value *, int> VVMap;
  ASSERT_EQ(VVMap.size(), 0);
  ASSERT_EQ(VVMap.empty(), true);
  ASSERT_EQ(VVMap.begin(), VVMap.end());
}

TEST_F(MultiValueMapTest, TestInsertOneWhenEmpty) {
  MultiValueMap<Value *, int> VVMap;
  
  auto R1 = VVMap.insert(VVMap.begin(), {this->AddV.get(), 10});
  ASSERT_EQ(R1.second, true);
  ASSERT_EQ(R1.first->first, this->AddV.get());
  ASSERT_EQ(R1.first->second, 10);
  ASSERT_EQ(R1.first, VVMap.begin());
  auto IShouldBeEnd = R1.first; IShouldBeEnd++;
  ASSERT_EQ(IShouldBeEnd, VVMap.end());
  
  auto R2 = VVMap.insert(VVMap.begin(), {this->AddV.get(), 20});
  ASSERT_EQ(R2.second, false);
  ASSERT_EQ(R2.first, VVMap.begin());
  ASSERT_EQ(R2.first, R1.first);
}

TEST_F(MultiValueMapTest, TestInsertOneAtBeginning) {
  MultiValueMap<Value *, int> VVMap;
  
  VVMap.insert(VVMap.begin(), {this->AddV.get(), 10});
  auto R1 = VVMap.insert(VVMap.begin(), {this->BitcastV.get(), 30});
  ASSERT_EQ(R1.second, true);
  ASSERT_EQ(R1.first->first, this->BitcastV.get());
  ASSERT_EQ(R1.first->second, 30);
  ASSERT_EQ(R1.first, VVMap.begin());
  auto I2 = ++(R1.first);
  ASSERT_EQ(I2->first, this->AddV.get());
  ASSERT_EQ(I2->second, 10);
  auto IShouldBeEnd = I2; IShouldBeEnd++;
  ASSERT_EQ(IShouldBeEnd, VVMap.end());
}

TEST_F(MultiValueMapTest, TestInsertOneAtEnd) {
  MultiValueMap<Value *, int> VVMap;
  
  auto R1 = VVMap.insert(VVMap.begin(), {this->AddV.get(), 10});
  auto R2 = VVMap.insert(VVMap.end(), {this->BitcastV.get(), 30});
  ASSERT_EQ(R2.second, true);
  ASSERT_EQ(R2.first->first, this->BitcastV.get());
  ASSERT_EQ(R2.first->second, 30);
  auto R2Dup = R1.first; R2Dup++;
  ASSERT_EQ(R2Dup, R2.first);
  auto IShouldBeEnd = R2.first; IShouldBeEnd++;
  ASSERT_EQ(IShouldBeEnd, VVMap.end());
}

TEST_F(MultiValueMapTest, TestAssociateOneLeft) {
  MultiValueMap<Value *, int> VVMap;
  
  auto P1 = VVMap.insert(VVMap.begin(), {this->AddV.get(), 10});
  auto P2 = VVMap.insert(VVMap.end(), {this->BitcastV.get(), 30});
  
  auto P1K0 = VVMap.insertLeft(P1.first, this->ConstantV);
  ASSERT_EQ(P1K0.first, P1.first);
  ASSERT_EQ(P1K0.second, false);
  
  auto P1K1 = VVMap.insertLeft(P2.first, this->ConstantV);
  ASSERT_EQ(P1K1.second, true);
  ASSERT_EQ(P1K1.first->first, this->ConstantV);
  ASSERT_EQ(P1K1.first->second, 10);
  P2.first = P1K1.first; P2.first++;
  ASSERT_EQ(P2.first->first, this->BitcastV.get());
  ASSERT_EQ(P2.first->second, 30);
  
  auto P2K2 = VVMap.insertLeft(VVMap.end(), this->CmpV.get());
  ASSERT_EQ(P2K2.second, true);
  ASSERT_EQ(P2K2.first->first, this->CmpV.get());
  ASSERT_EQ(P2K2.first->second, 30);
  P2.first = P2K2.first; P2.first++;
  ASSERT_EQ(P2.first, VVMap.end());
  
  SmallVector<SmallVector<Value *, 2>, 2> Expected =
      {{this->AddV.get(), this->ConstantV},
       {this->BitcastV.get(), this->CmpV.get()}};
  for (auto EInner: Expected) {
    for (auto V: EInner) {
      SmallVector<Value *, 2> All;
      VVMap.getAssociatedValues(V, All);
      ASSERT_EQ(EInner, All);
    }
  }
}

TEST_F(MultiValueMapTest, TestAssociateOneRight) {
  MultiValueMap<Value *, int> VVMap;
  
  auto P1 = VVMap.insert(VVMap.begin(), {this->AddV.get(), 10});
  auto P2 = VVMap.insert(VVMap.end(), {this->BitcastV.get(), 30});
  
  auto P1K1 = VVMap.insertRight(P1.first, this->ConstantV);
  ASSERT_EQ(P1K1.second, true);
  ASSERT_EQ(P1K1.first->first, this->ConstantV);
  ASSERT_EQ(P1K1.first->second, 10);
  P2.first = P1K1.first; P2.first++;
  ASSERT_EQ(P2.first->first, this->AddV.get());
  ASSERT_EQ(P2.first->second, 10);
  P2.first++;
  ASSERT_EQ(P2.first->first, this->BitcastV.get());
  ASSERT_EQ(P2.first->second, 30);
  
  auto P2K1 = VVMap.insertRight(P2.first, this->SubV.get());
  ASSERT_EQ(P2K1.second, true);
  ASSERT_EQ(P2K1.first->first, this->SubV.get());
  ASSERT_EQ(P2K1.first->second, 30);
  P2.first = P2K1.first; P2.first++;
  ASSERT_EQ(P2.first->first, this->BitcastV.get());
  ASSERT_EQ(P2.first->second, 30);
  
  auto P2K2 = VVMap.insertRight(VVMap.end(), this->CmpV.get());
  ASSERT_EQ(P2K2.first, VVMap.end());
  ASSERT_EQ(P2K2.second, false);
  
  SmallVector<SmallVector<Value *, 2>, 2> Expected =
      {{this->ConstantV, this->AddV.get()},
       {this->SubV.get(), this->BitcastV.get()}};
  for (auto EInner: Expected) {
    for (auto V: EInner) {
      SmallVector<Value *, 2> All;
      VVMap.getAssociatedValues(V, All);
      ASSERT_EQ(EInner, All);
    }
  }
}


}
