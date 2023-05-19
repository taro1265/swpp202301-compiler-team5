#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"
#include "./loop_analysis.h"
#include "./loop_branch_condition.h"
#include "./add_sum.h"
#include "./load_to_aload.h"
#include "./load_reordering.h"
#include "./add_sccp.h"
#include "./switch_to_br.h"
#include "./br_to_switch.h"
#include "./recursive_branch_condition.h"

using namespace llvm;

class SimplePass : public PassInfoMixin<SimplePass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);
};

class ShiftConstantAddPass : public PassInfoMixin<ShiftConstantAddPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);

private:
  void shiftToOp(Instruction *I, Instruction::BinaryOps Operator,
                  IRBuilder<> &Builder);
};

class OraclePass : public PassInfoMixin<OraclePass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
};
