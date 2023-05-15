//===- mm_IntraAnalysis.h  -------*- C++ -*-===//
//
//                     The CouponMaker Infrastructure
//
//===----------------------------------------------------------------------===//
//
// This file provides intra-procedual analysis interfaces used to implement A2PC.
//
//===----------------------------------------------------------------------===//


#ifndef INFRASTRUCTURE_H
#define INFRASTRUCTURE_H

#include "DataStructure.h"


using namespace llvm;





namespace llvm {
    
    /// Add two argumetns "preFlag" and "onlineFlag" to the args list of F
    /// That is : transform a func(param1, param2, ...) to
    /// a newFunc(preFlag, onlinePlag, param1, param2, ...)
    /// 1. transfrom func;   2. transform corresponding CallInsts;
    class FormTrans {
        Module &M;
        
        // type for preFlag and onlineFlag (c does not have type)
        IntegerType *Int32Ty;
        
        /// NOTE: Do not replace the function call within the iter process
        /// otherwise it will make the iter become a null pointer
        /// NOTE: Do not delete/replace the function when traverse the Module
        /// otherwie it will make
        
        //for each funcName, get pointer ot the corresponding function
        std::map<std::string, Function *> nameFuncMap;
        
        // for each function, get corresponding CallInstSet calling it
        std::map<Function *, std::set<CallInst *>> funcCallMap;
        
        /// note transformed function, and its knownInputSet
        std::map<Function *, std::set<AllocaInst *>> transedFuncMap;
        
    public:
        FormTrans(Module &M) : M(M) {
            Int32Ty = IntegerType::get(M.getContext(), 32);
            getfuncCallMap(M);
        };
        
        // transform the form of a specific function, and return the newF
        Function *transFuncArgsForms(std::string funcName);
        
    private:
        void getfuncCallMap(Module &M);
    };
    


// inter-procedual analysis and Transformation
class CouponMaker {
	Module &M_;
        
	// the name of the top function, e.g, "my_main"
	std::string start_func_name_;
	
	// online input names in start function
	std::set<StringRef> online_input_names_;
        
	// sort funtions in the topological order in call graph
	std::vector<Function *> funcs_topo_order_;
        
	// how many times a function is called
	std::map<Function *, int> func_called_times_;
        
	// func -- argNo Set of arrays/ptrs arguments changed within F
	std::map<Function *, std::set<int>> func_changed_array_args_map_;
        
	std::map<Function *, PDG *> func_pdg_map_;
	
	std::map<Function *, std::set<int>> func_online_args_;
	
	std::map<Function *, std::set<DGNode *>> func_precomp_nodes_;

public:
	CouponMaker(Module &M, std::string start_func_name, std::set<StringRef> online_input_names)
		: M_(M), start_func_name_(start_func_name), online_input_names_(online_input_names) {}
	
	void analyzeProgram();
	
	void transformProgram();
        
private:
	void analyzeCG();
	
	void analyzeStartFunc(Function *start_func, std::list<Function *> &work_list);
	
	void analyzeOtherFunction(Function *func, std::list<Function *> &work_list);
	
	void detectFuncPrecompSet(Function *func, std::list<Function *> &work_list, std::set<AllocaInst *> &online_vars);
	
	void printDebugInfo();
	
	void transformFunction(Function *func);
	
	// precomp/online phases (continous precomp/online nodes)
	Sections *splitSections(Function *func);
	
	void SplitBB(Sections *secs, int sec_id);
	
	SecType calcuSecType(NodeType node_type, DGNode *dg_node, std::set<DGNode *> precomp_nodes);
	
	AllocaInst *getAIbyName(Function *F, StringRef varName);
	
	void insertBranch(Function *func, Value *condition, Sections *secs, int sec_id);
	
	// detect coupon and insert NVM Store/Load coupons instructions
   	void processCoupons(Function *func, Value *condition, Sections *secs, int sec_id);
};




    
    
class TransformFunction {
	Module &M;
        
	Function *F;
        
	PDG pdg_;
        
	InstSet PreSet;
        
	InstSet ReturnTypeSet;
        
	IntegerType *Int32Ty;
        
	// AllocaInst to allocate preFlag.addr and onlineFlag.addr
	AllocaInst *precompFlagAI, *onlineFlagAI;

public:
	TransformFunction(Module &M, Function *F, PDG pdg, InstSet PreSet)
		: M(M), F(F), pdg_(pdg), PreSet(PreSet) {
		Int32Ty = IntegerType::get(M.getContext(), 32);
		// ReturnTypeSet = {load rtValue, returnInst}
		initReturnTypeSet();
	}
        
	///process loops whose part member instructions in preSet
	void processLoop();
        
	//instrument: get sections, detect coupons and instrument
	void transformation();
    

private:
	// return the SecType of the inst
	SecType getSecTypeOfInst(Instruction *inst);
        
	// ReturnTypeSet = {load rtValue, returnInst}
	void initReturnTypeSet();
};
} //end of namespace


#endif
