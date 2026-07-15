#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/IRBuilder.h"
#include <unordered_map>
#include <vector>

using namespace llvm;

namespace {
struct TailCallElimination : public FunctionPass {
  static char ID;

  std::unordered_map<Argument *, Value *> Slots;  

  TailCallElimination() : FunctionPass(ID) {}

  // Is CI a tail-recursive call of function F?
  // Handles both the value-returning and the void case.
  bool isTailRecursiveCall(CallInst *CI, Function &F) {
    // Must call itself (recursion).
    if (CI->getCalledFunction() != &F)
      return false;

    Instruction *Next = CI->getNextNode();

    // Void case:  call void @F(...)  followed directly by a branch to a
    // block that only does 'ret void' -- nothing happens after the call.
    if (CI->getType()->isVoidTy()) {
      BranchInst *Br = dyn_cast_or_null<BranchInst>(Next);
      if (!Br || !Br->isUnconditional())
        return false;
      ReturnInst *Ret =
          dyn_cast<ReturnInst>(Br->getSuccessor(0)->getTerminator());
      return Ret && Ret->getReturnValue() == nullptr;
    }

    // Value-returning case: the call result is stored into a slot ...
    StoreInst *SI = dyn_cast_or_null<StoreInst>(Next);
    if (!SI || SI->getValueOperand() != CI)
      return false;

    // ... then an unconditional branch ...
    BranchInst *Br = dyn_cast_or_null<BranchInst>(SI->getNextNode());
    if (!Br || !Br->isUnconditional())
      return false;

    // ... to a block that returns exactly that slot's value, with no
    // operation applied to it (this rejects e.g. 'return f(n-1) + 1').
    ReturnInst *Ret =
        dyn_cast<ReturnInst>(Br->getSuccessor(0)->getTerminator());
    if (!Ret)
      return false;
    LoadInst *LI = dyn_cast_or_null<LoadInst>(Ret->getReturnValue());
    return LI && LI->getPointerOperand() == SI->getPointerOperand();
  }

  // Collect ALL tail-recursive calls in F (not just the first one).
  std::vector<CallInst *> findTailRecursiveCalls(Function &F) {
    std::vector<CallInst *> Calls;
    for (BasicBlock &BB : F)
      for (Instruction &I : BB)
        if (CallInst *CI = dyn_cast<CallInst>(&I))
          if (isTailRecursiveCall(CI, F))
            Calls.push_back(CI);
    return Calls;
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
    Instruction *Next = TailCall->getNextNode();

    // In the void case there is no result store; the branch follows the call.
    StoreInst *RetStore = dyn_cast<StoreInst>(Next);
    BranchInst *OldBr = RetStore ? cast<BranchInst>(RetStore->getNextNode())
                                 : cast<BranchInst>(Next);             

    // Instead of the call: write the new argument values into the parameter
    // slots, then jump back to header (the start of the loop).
    IRBuilder<> Builder(OldBr);
    for (unsigned i = 0; i < TailCall->arg_size(); ++i)
      Builder.CreateStore(TailCall->getArgOperand(i), Slots[F.getArg(i)]);
    Builder.CreateBr(Header);

    // Erase the old branch, the result store (if any) and the call itself.
    OldBr->eraseFromParent();
    if (RetStore)
      RetStore->eraseFromParent();
    TailCall->eraseFromParent();
  }

  bool runOnFunction(Function &F) override {
    Slots.clear();  
    if (F.arg_empty())
      return false;

    std::vector<CallInst *> TailCalls = findTailRecursiveCalls(F);
    if (TailCalls.empty())
      return false;

    findArgumentSlots(F);
    BasicBlock *Header = createLoopHeader(F);
    for (CallInst *TailCall : TailCalls)
      replaceCallWithJump(F, TailCall, Header);

    errs() << "TCE: turned tail calls into a loop in function " << F.getName() << " (" << TailCalls.size() << ")\n";

    return true;
  }
};
}

char TailCallElimination::ID = 0;
static RegisterPass<TailCallElimination> X("tail-call-elimination", "Tail Call Elimination (studentski)", false, false);