//===- mm_IntraAnalysis.h  -------*- C++ -*-===//
//
//                     The CouponMaker Infrastructure
//
//===----------------------------------------------------------------------===//
//
// This file provides intra-procedual analysis interfaces used to implement A2PC.
//
//===----------------------------------------------------------------------===//


#include "Infrastructure.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Transforms/Utils/Cloning.h"


//#define DEBUG_TYPE "A2PCFormTrans"

using namespace llvm;


void FormTrans::getfuncCallMap(Module &M) {
    /// NOTE: Could not replace the function call within the iter process
    /// otherwise it will make the iter become a null pointer
    /// NOTE: Could not delete/replace the function when traverse the Module
    /// otherwie it will make
    for (Function &F : M) {
        if (!F.empty()) {           //not system function
            nameFuncMap[F.getName()] = &F;
            funcCallMap[&F] = std::set<CallInst *>();
        }
    }
    for (Function &F : M) {
        if (!F.empty()) {           //not system function
            for (inst_iterator I = inst_begin(F); I != inst_end(&F); I++) {
                if (CallInst *CI = dyn_cast<CallInst>(&*I)) {
                    Function *CalledFunc = CI->getCalledFunction();
                    if (!CalledFunc->empty() &&
                        funcCallMap.find(CalledFunc) != funcCallMap.end()) {
                        funcCallMap[CalledFunc].insert(CI);
                    }
                }
            }
        }
    }
#ifdef DEBUG_TYPE
    std::map<Function *, std::set<CallInst *>>::iterator iter;
    for (iter = funcCallMap.begin(); iter != funcCallMap.end(); iter ++) {
        errs() << "function: " << iter->first->getName() << ":\n ";
        std::set<CallInst *> CISet = iter->second;
        for (CallInst *CI : CISet) {
            errs() << " " << *CI << "\n";
        }
    }
#endif
}


/// Add two argumetns "preFlag" and "onlineFlag" to the end of args list of F
/// 1. create a new F with added args
/// 2. clone old F; add operations about new parameters
/// 3. reset CallInst(oldF) to CallInst(newF)
/// 4. return newF
Function *FormTrans::transFuncArgsForms(std::string funcName) {
    Function *newF = nullptr;
    if (nameFuncMap.find(funcName) != nameFuncMap.end()) {
        Function *oldF = nameFuncMap[funcName];
        // 1. create a new F with added args
        FunctionType *oldFTy = oldF->getFunctionType();
        Type *oldFRetTy = oldFTy->getReturnType();
        std::vector<Type *> newFParamsTy = std::vector<Type *>();
        for (Type *oldFParaTy : oldFTy->params()) {
            newFParamsTy.push_back(oldFParaTy);
        }
        newFParamsTy.push_back(Int32Ty);
        newFParamsTy.push_back(Int32Ty);
        FunctionType *newFTy = FunctionType::get(oldFRetTy, newFParamsTy, false);
        std::string newFuncName = funcName + "New";
        Constant *newFC = M.getOrInsertFunction(newFuncName, newFTy);
        newF = cast<Function>(newFC);
        newF->setCallingConv(CallingConv::C);
        
        // 2（1）. clone old F
        ValueToValueMapTy VMap;
        Function::arg_iterator AI = newF->arg_begin();
        for (Function::const_arg_iterator I = oldF->arg_begin();
            I != oldF->arg_end(); I ++, AI ++) {
            VMap[&*I] = AI;
        }
        SmallVector<ReturnInst*, 8> Returns;
        CloneFunctionInto(newF, oldF, VMap, false, Returns);
        
        // 2（2）add AllocaInst for preFlag, onlineFlag (at the begin of newF)
        ///     add StoreInst which stores paraValue to preFlag/onlineFlag
        IRBuilder<> builder(M.getContext());
        builder.SetInsertPoint(&*inst_begin(newF));
        AllocaInst *preFAI = builder.CreateAlloca(Int32Ty, nullptr, "preFlag");
        AllocaInst *onlineFAI = builder.CreateAlloca(Int32Ty, nullptr, "onlineFlag");
        /*Instruction *firstNonAIInst;
        for (inst_iterator I = inst_begin(newF); I != inst_end(newF); I++){
            if (!dyn_cast<AllocaInst>(&*I)) {
                firstNonAIInst = &*I;
                break;
            }
        }
        builder.SetInsertPoint(firstNonAIInst);*/
        builder.CreateStore(&*(newF->arg_end()-2), preFAI);
        builder.CreateStore(&*(newF->arg_end()-1), onlineFAI);
 
        /// 3. replace CallInst(oldF) with CallInst(newF)
        /// (a) Create args properly and make sure the actual and formal parameter types should be same.
        /// If not add a cast.
        /// (b) Make sure the return types are same.
        /// (c) Set the calling conventions if any.
        /// (d) Replace use if any.
        /// (e) Then replace the instruction.
/*        for (CallInst *CI : funcCallMap[oldF]) {
            errs() << "callInst: " << *CI << " has args: \n";
            CallSite CS(CI);
            // pre CI
            SmallVector<Value *, 8> newFParams(CS.arg_begin(), CS.arg_end());
            newFParams.push_back(ConstantInt::get(Int32Ty, 1, true));
            newFParams.push_back(ConstantInt::get(Int32Ty, 0, true));
            CallInst *NewCI = CallInst::Create(newF, newFParams);
            NewCI->setCallingConv(newF->getCallingConv());
            if (!CI->use_empty()) {
                CI->replaceAllUsesWith(NewCI);
            }
            ReplaceInstWithInst(CI, NewCI);
            // online CI
            SmallVector<Value *, 8> newFOnParams(CS.arg_begin(), CS.arg_end());
            newFOnParams.push_back(ConstantInt::get(Int32Ty, 0, true));
            newFOnParams.push_back(ConstantInt::get(Int32Ty, 1, true));
            CallInst *NewOnCI = CallInst::Create(newF, newFOnParams);
            NewOnCI->setCallingConv(newF->getCallingConv());
            NewOnCI->insertAfter(NewCI);
        }*/
        // to main function, to note the running time
        bool first = true;
        for (CallInst *CI : funcCallMap[oldF]) {
            if (first) {
                first = false;
                CallSite CS(CI);
                // pre CI
                SmallVector<Value *, 8> newFParams(CS.arg_begin(), CS.arg_end());
                newFParams.push_back(ConstantInt::get(Int32Ty, 1, true));
                newFParams.push_back(ConstantInt::get(Int32Ty, 0, true));
                CallInst *NewCI = CallInst::Create(newF, newFParams);
                NewCI->setCallingConv(newF->getCallingConv());
                if (!CI->use_empty()) {
                    CI->replaceAllUsesWith(NewCI);
                }
                ReplaceInstWithInst(CI, NewCI);
            } else {
                CallSite CS(CI);
                // online CI
                SmallVector<Value *, 8> newFParams(CS.arg_begin(), CS.arg_end());
                newFParams.push_back(ConstantInt::get(Int32Ty, 0, true));
                newFParams.push_back(ConstantInt::get(Int32Ty, 1, true));
                CallInst *NewCI = CallInst::Create(newF, newFParams);
                NewCI->setCallingConv(newF->getCallingConv());
                if (!CI->use_empty()) {
                    CI->replaceAllUsesWith(NewCI);
                }
                ReplaceInstWithInst(CI, NewCI);
            }
        }
        // note info and delete F from module
        funcCallMap.erase(oldF);
        transedFuncMap[newF] = std::set<AllocaInst *>();
        oldF->eraseFromParent();
    }
    //4. return newF
    return newF;
}
