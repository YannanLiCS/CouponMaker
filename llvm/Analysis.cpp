//===- mm_IntraAnalysis.h  -------*- C++ -*-===//
//
//                     The CouponMaker Infrastructure
//
//===----------------------------------------------------------------------===//
//
// This file provides intra- and inter-procedual analysis interfaces used to implement A2PC.
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
#include "llvm/Analysis/Interval.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/Function.h"


typedef std::set<Function *> FuncSet;

//#define DEBUG_TYPE "CouponMakerAnalysis"

namespace  {
    template<typename T> bool setHas(std::set<T> set, T item) {
        if (set.find(item) != set.end()) {
            return true;
        }
        return false;
    }
}   //namespace




//------------------------- Class AnalyzeProgram ---------------------------

void CouponMaker::analyzeProgram() {
    analyzeCG();
    for (Function *F : funcs_topo_order_) {
        PDG *pdg = new PDG(F);
    	pdg->construct(func_changed_array_args_map_);
		func_changed_array_args_map_[F] = pdg->getChangedArrayArgs();
		func_pdg_map_[F] = pdg;
#ifdef DEBUG_TYPE
        errs() << F->getName() << " is called "<< func_called_times_[F] << " times \n";
        for (int argNo : func_changed_array_args_map_[F]) {
            errs() << " changed arg" << argNo << "\n";
        }
    	pdg->printDefsMap();
#endif
    }
	// analyze precomp_set of each once-called function	
	std::list<Function *> work_list;
	Function *start_func = M_.getFunction(start_func_name_);
	analyzeStartFunc(start_func, work_list);
	while (!work_list.empty()) {
		Function *func_to_analy = work_list.front();
		work_list.remove(func_to_analy);
		analyzeOtherFunction(func_to_analy, work_list);
	}
#ifdef DEBUG_TYPE
	printDebugInfo();
#endif	 
}

// (1) sort funtions in the topological order in call graph (start from main);
// (2) calculate how many times a function is called;
void CouponMaker::analyzeCG() {
    CallGraph CG(M_);    // provided by llvm
    std::map<Function *, int> funcIndegreeMap;
    std::map<Function *, FuncSet> funcSuccMap;
    // start traverse from the start_func
    Function *start_func = M_.getFunction(start_func_name_);
    if (!start_func) {
        errs() << "Start function " << start_func_name_ << " not found! \n";
        std::exit(EXIT_FAILURE);
    }
    func_called_times_[start_func] = 0;
    std::list<Function *> workList;
    workList.push_back(start_func);
    while (!workList.empty()) {
        Function *F = workList.front();
        workList.remove(F);
#ifdef DEBUG_TYPE
        errs() << "callerFunc: " << F->getName() << "\n";
#endif
        if (funcIndegreeMap.find(F) == funcIndegreeMap.end()) {funcIndegreeMap[F] = 0;}
        CallGraphNode *CGNode = CG[F];
        FuncSet calledFuncCounted;
        for (CallGraphNode::iterator iter = CGNode->begin(); iter < CGNode->end(); iter ++) {
            Function *CalledFunc = iter->second->getFunction();
            // CalledFunc->empty() <==> system call
            // Calculte how many times a function is called
            if (!CalledFunc->empty()) {
                if (func_called_times_.find(CalledFunc) == func_called_times_.end()) {
                    func_called_times_[CalledFunc] = 0;
                }
                func_called_times_[CalledFunc] ++;
            }
            // !setHas <==> in case that if the same function is called by F many times and then indegree[F] is increased many times
            if (!CalledFunc->empty() && !setHas<Function *>(calledFuncCounted, CalledFunc)) {
                funcIndegreeMap[F] ++;
                workList.push_back(CalledFunc);
                calledFuncCounted.insert(CalledFunc);
                if (funcSuccMap.find(CalledFunc) == funcSuccMap.end()) {
                    funcSuccMap[CalledFunc] = FuncSet();
                }
                funcSuccMap[CalledFunc].insert(F);
#ifdef DEBUG_TYPE
                errs() << "     calledFunc: " << CalledFunc->getName() << "\n";
#endif
            }
        }
    }
    // Create an queue and enqueue all vertices with
    // indegree 0
    std::list<Function *> funcQue;
    for (auto iter = funcIndegreeMap.begin(); iter != funcIndegreeMap.end(); iter ++) {
        if (iter->second == 0) {
            funcQue.push_back(iter->first);
        }
    }
    // Initialize count of visited vertices
    int cnt = 0;
    // One by one dequeue vertices from queue and enqueue
    // adjacents if indegree of adjacent becomes 0
    while (!funcQue.empty()) {
        // Extract front of queue (or perform dequeue)
        // and add it to topological order
        Function *F = funcQue.front();
        funcQue.remove(F);
        cnt ++;
        /// NOTE: we treat the function that is called more than once as an whole,
        ///  ==> we do not analyze this kind of functions ==> remove them from final topo_order
        ///if (func_called_times_[F] <= 1) {       // mainFunc is called 0 time
        ///   funcs_topo_order_.push_back(F);
        ///}
        funcs_topo_order_.push_back(F);
        for (Function *succF : funcSuccMap[F]) {
            // If in-degree becomes zero, add it to queue
            if (--funcIndegreeMap[succF] == 0)
                funcQue.push_back(succF);
        }
    }
    // Check if there was a cycle
    if ((unsigned long)cnt != funcIndegreeMap.size()) {
        errs() << "CG has cycles!\n";
        std::exit(EXIT_FAILURE);
    }
}

// analyze the dependecy of start funtion
void CouponMaker::analyzeStartFunc(Function *start_func, std::list<Function *> &work_list) {
	func_precomp_nodes_[start_func] = std::set<DGNode *>();
	std::set<AllocaInst *> online_vars;
	for (inst_iterator iter = inst_begin(start_func); iter != inst_end(start_func); iter ++) {
		if (AllocaInst *AI = dyn_cast<AllocaInst>(&*iter)) {
			if (online_input_names_.find(AI->getName()) != online_input_names_.end()) {
				online_vars.insert(AI);
			}
		} else {
			break;
		}
	}
	detectFuncPrecompSet(start_func, work_list, online_vars);
}


// analyze dependecy of other functions 
void CouponMaker::analyzeOtherFunction(Function *func, std::list<Function *> &work_list) {
	// init the precomp_nodes of func
	func_precomp_nodes_[func] = std::set<DGNode *>();
    // detect online vars
    std::set<AllocaInst *> online_vars;
    std::map<int, AllocaInst *> args = func_pdg_map_[func]->getArgs();
    for (auto iter = args.begin(); iter != args.end(); iter ++) {
    	if (func_online_args_[func].find(iter->first) != func_online_args_[func].end()) { 
    		online_vars.insert(iter->second);
    	}
    }
	detectFuncPrecompSet(func, work_list, online_vars);
}

void CouponMaker::detectFuncPrecompSet(Function *func, std::list<Function *> &work_list, 
	std::set<AllocaInst *> &online_vars) {
	// precomp_set = { node | none of the used vars in online_vars set }
    for (DGNode *node: func_pdg_map_[func]->getNodeSeq()) {
    	if (node->getNodeType() == InitialTy || node->getNodeType() == ReturnTy) { 
    		func_precomp_nodes_[func].insert(node); 
    		continue; 
    	} 
    	bool precomp = true;
    	if (node->getNodeType() == SysCallTy && node->getCalledFunc()->getName().find("llvm.memcpy")) {
    		for (AllocaInst *defined_var : node->getDefinedVars()) {
    			if (online_vars.find(defined_var) != online_vars.end()) { 
    				precomp = false; 
    				break;
    			}
    		}
    	}
    	for (AllocaInst *used_var : node->getUsedVars()) {
    		if (online_vars.find(used_var) != online_vars.end()) { 
    			precomp = false; 
				break;
    		}
    	}
    	if (precomp) {
    		func_precomp_nodes_[func].insert(node);
    	} else {
    		if (node->getNodeType() == UserCallTy && func_called_times_[node->getCalledFunc()] == 1) { 
    			Function *calledFunc = node->getCalledFunc();
    			work_list.push_back(calledFunc);
    			func_online_args_[calledFunc] = std::set<int>();
    			int arg_index = 0;
   				for (AllocaInst *AI : node->getCallArgs()) {
   					if (AI != nullptr && online_vars.find(AI) != online_vars.end()) { 
   						func_online_args_[calledFunc].insert(arg_index);
   					}
   					arg_index ++;
    			}
    		}
    		std::set<AllocaInst *> new_online_vars = node->getDefinedVars();
    		online_vars.insert(new_online_vars.begin(), new_online_vars.end());
    	}
    }
}



void CouponMaker::printDebugInfo() {
	errs() << "============FuncOnlineArgsMap============\n";
	for (auto iter = func_online_args_.begin(); iter != func_online_args_.end(); iter ++) {
		errs() << "Function " << iter->first->getName() << ":   Online_Args = { ";
		for (int arg_index : iter->second) {
   	    	errs() << "arg" << arg_index << " ";
       }
       errs() << "}\n";
	}
	errs() << "============FuncPrecompNodesMap============\n";
	for (auto iter = func_precomp_nodes_.begin(); iter != func_precomp_nodes_.end(); iter ++){
       errs() << "Function " << iter->first->getName() << ":   Precomp_Nodes = { ";
       for (DGNode *node : iter->second) {
   	    	errs() << "Node " << node->getNodeID() << " ";
   	    	node->printInfo();
       }
       errs() << "}\n";
   	}
	errs() << "==============================\n";
}




