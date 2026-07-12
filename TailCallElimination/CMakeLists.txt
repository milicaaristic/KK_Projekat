add_llvm_library(LLVMTailCallElimination MODULE
  TailCallEliminationPass.cpp

  PLUGIN_TOOL
  opt
  )
