#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/IRBuilder.h"
#include <unordered_map>

using namespace llvm;

namespace {
struct TailCallElimination : public FunctionPass {
  static char ID;

  std::unordered_map<Argument *, Value *> Slots;  

  TailCallElimination() : FunctionPass(ID) {}

  // Is CI a tail-recursive call of function F?
  bool isTailRecursiveCall(CallInst *CI, Function &F) {
    if (CI->getCalledFunction() != &F)
      return false;

    StoreInst *SI = dyn_cast_or_null<StoreInst>(CI->getNextNode());
    if (!SI || SI->getValueOperand() != CI)
      return false;

    BranchInst *Br = dyn_cast_or_null<BranchInst>(SI->getNextNode());
    if (!Br || !Br->isUnconditional())
      return false;

    return true;
  }

  // Return the first tail-recursive call in F, or nullptr if there is none.
  CallInst *findTailRecursiveCall(Function &F) {
    for (BasicBlock &BB : F)
      for (Instruction &I : BB)
        if (CallInst *CI = dyn_cast<CallInst>(&I))
          if (isTailRecursiveCall(CI, F))
            return CI;
    return nullptr;
  }

  // Split the entry block after the initial argument stores.
  BasicBlock *createLoopHeader(Function &F) {
    BasicBlock &Entry = F.getEntryBlock();

    // Split point = the instruction right after the last  store <arg>, <slot>.
    Instruction *SplitPoint = nullptr;
    for (Instruction &I : Entry)
      if (StoreInst *SI = dyn_cast<StoreInst>(&I))
        if (isa<Argument>(SI->getValueOperand()))
          SplitPoint = SI->getNextNode();

    return Entry.splitBasicBlock(SplitPoint, "header");
  }

  // Map each function argument to its stack slot (%x.addr).
  void findArgumentSlots(Function &F) {
    for (Instruction &I : F.getEntryBlock())
      if (StoreInst *SI = dyn_cast<StoreInst>(&I))
        if (Argument *A = dyn_cast<Argument>(SI->getValueOperand()))
          Slots[A] = SI->getPointerOperand();
  }

  void replaceCallWithJump(Function &F, CallInst *TailCall, BasicBlock *Header) {

    StoreInst *RetStore = cast<StoreInst>(TailCall->getNextNode());               
    BranchInst *OldBr = cast<BranchInst>(RetStore->getNextNode());               

    // Instead of the call: write the new argument values into the parameter
    // slots, then jump back to header (the start of the loop).
    IRBuilder<> Builder(OldBr);
    for (unsigned i = 0; i < TailCall->arg_size(); ++i)
      Builder.CreateStore(TailCall->getArgOperand(i), Slots[F.getArg(i)]);
    Builder.CreateBr(Header);

    OldBr->eraseFromParent();
    RetStore->eraseFromParent();
    TailCall->eraseFromParent();
  }

  bool runOnFunction(Function &F) override {
    Slots.clear();  

    CallInst *TailCall = findTailRecursiveCall(F);
    if (!TailCall)
      return false;

    BasicBlock *Header = createLoopHeader(F);
    replaceCallWithJump(F, TailCall, Header);

    errs() << "TCE: repni poziv pretvoren u petlju u funkciji ... " << F.getName() << "\n";

    return true;
  }
};
}

char TailCallElimination::ID = 0;
static RegisterPass<TailCallElimination> X("tail-call-elimination", "Tail Call Elimination (studentski)", false, false);
