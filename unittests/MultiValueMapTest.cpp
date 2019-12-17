#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "gtest/gtest.h"

namespace {

TEST(TestTest, TestTestTest) {
  llvm::dbgs() << "test\n";
}

}
