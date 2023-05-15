//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//

#include "Infrastructure.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include <time.h>

using namespace llvm;

#define DEBUG_TYPE "hello"

namespace{
    struct Hello : public ModulePass {
        static char ID; // Pass identification, replacement for typeid
        
        Hello() : ModulePass(ID) {}
        
        virtual bool runOnModule(Module &M) override {
            std::string start_func_name = "my_main";
            std::set<StringRef> online_input_names = {"currBlkPlain"};
            process(M, start_func_name, online_input_names);
            return false;
        }
        
        
        
        void process(Module &M, std::string start_func_name, std::set<StringRef> online_input_names) {
            clock_t start, end;
            start = clock();
            CouponMaker coupon_maker(M, start_func_name, online_input_names);
            coupon_maker.analyzeProgram();
            coupon_maker.transformProgram();
            end = clock();
            errs() << "analysis time: " << ((double) (end - start)) / CLOCKS_PER_SEC << "\n";
        }
    };
}

char Hello::ID = 0;
static RegisterPass<Hello> X("hello", "test function exist", false, false);

