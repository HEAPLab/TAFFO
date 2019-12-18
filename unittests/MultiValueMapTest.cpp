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
    
  MultiValueMapTest() :
      ConstantV(ConstantInt::get(Type::getInt32Ty(Context), 0)),
      BitcastV(new BitCastInst(ConstantV, Type::getInt32Ty(Context))),
      AddV(BinaryOperator::CreateAdd(ConstantV, ConstantV)) {}
};


TEST_F(MultiValueMapTest, TestEmpty) {
  MultiValueMap<Value *, int> VVMap;
  ASSERT_EQ(VVMap.size(), 0);
  ASSERT_EQ(VVMap.empty(), true);
  ASSERT_EQ(VVMap.begin(), VVMap.end());
}

TEST_F(MultiValueMapTest, TestInsertOne) {
  MultiValueMap<Value *, int> VVMap;
  auto R1 = VVMap.insert(VVMap.begin(), {this->AddV.get(), 10});
  ASSERT_EQ(R1.second, true);
  ASSERT_EQ(R1.first->first, this->AddV.get());
  ASSERT_EQ(R1.first->second, 10);
  auto R2 = VVMap.insert(VVMap.begin(), {this->AddV.get(), 20});
  ASSERT_EQ(R2.second, false);
  ASSERT_EQ(R2.first, VVMap.begin());
  ASSERT_EQ(R2.first, R1.first);
}

}
