//===- PDG.cpp  -------*- C++ -*-===//
//
//                     The CouponMaker Data structure 
//
//===----------------------------------------------------------------------===//
//
// This file provides inter-procedual analysis interfaces used to implement CouponMaker.
// Limitation: Do not consider GlobalValue (const in our test programs); Bybass Branch. 
//
//===----------------------------------------------------------------------===//


#include "DataStructure.h"
#include <tuple>
#include <fstream>
#include <stack>
#include <queue>
#include "llvm/Analysis/Interval.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/BasicBlock.h"

// TODO: tmp size for pointer (how to decide the size of pointer)
#define KEY_SIZE 2
#define DES_BLOCK_SIZE 8
#define CAMELLIA_BLOCK_SIZE 16

typedef std::set<Function *> FuncSet;


typedef std::pair<AllocaInst *, StoreInst *> varDefPair;
// map ==> one var can have at most one def ==> it is reaonsable within a BB
typedef std::map<AllocaInst *, StoreInst *> varDefMap;



//#define DEBUG_TYPE "CouponMakerPDG"

using namespace llvm;


namespace  {
    template<typename T> bool setHas(std::set<T> set, T item) {
        if (set.find(item) != set.end()) {
            return true;
        }
        return false;
    }
    AllocaInst *getChangedAI(Instruction *inst) {
		Instruction *inst_try = inst;
    	while (!dyn_cast<AllocaInst>(inst_try)) {
    	   	if (LoadInst *ld_inst = dyn_cast<LoadInst>(inst_try)) {
    	      	inst_try = dyn_cast<Instruction>(ld_inst->getPointerOperand());
    	   	} else if (StoreInst *st_inst = dyn_cast<StoreInst>(inst_try)) {
    	      	inst_try = dyn_cast<Instruction>(st_inst->getPointerOperand());
    	   	} else if (GetElementPtrInst *gep_inst = dyn_cast<GetElementPtrInst>(inst_try)) {
    	   		inst_try = dyn_cast<Instruction>(gep_inst->getPointerOperand());
    	   	} else if (CastInst *cast_inst = dyn_cast<CastInst>(inst_try)) {	
				inst_try = dyn_cast<Instruction>(cast_inst->getOperand(0));
    	   	} else {
    	   		errs() << "other types in getChangedAI()!" << *inst << "\n";
    	   		std::exit(EXIT_FAILURE); 
    	   	}
    	}
    	return dyn_cast<AllocaInst>(inst_try);
    }
}   //namespace



//------------------------ Class DGNode -------------------------

void DGNode::mergeWithNode(DGNode *dg_node_2) {
   NodeType node_type_2 = dg_node_2->getNodeType();
   if (node_type_ == UserCallTy or node_type_2 == UserCallTy) { node_type_ = UserCallTy; }
   if (node_type_ == SysCallTy or node_type_2 == SysCallTy) { node_type_ = SysCallTy; }
   if (node_type_ == BranchTy or node_type_2 == BranchTy) { node_type_ = BranchTy; }
   if (node_type_ == InitialTy or node_type_2 == InitialTy) { node_type_ = InitialTy; }
   if (node_type_ == ReturnTy or node_type_2 == ReturnTy) { node_type_ = ReturnTy; }
   std::set<AllocaInst *> used_vars_2 = dg_node_2->getUsedVars();
   std::set<AllocaInst *> defined_vars_2 = dg_node_2->getDefinedVars();
   used_vars_.insert(used_vars_2.begin(), used_vars_2.end());
   defined_vars_.insert(defined_vars_2.begin(), defined_vars_2.end());
}

void DGNode::printInfo() {
    errs() << "Node" << id_ << " type: " << node_type_ << "\n";
    errs() << "contained instructions:\n";
    for (auto *inst : getContainedInsts()) { errs() << *inst << "\n"; }
    if (head_inst_ != nullptr) { errs() << "head: " << *head_inst_ << "\n"; } 
    if (tail_inst_ != nullptr) { errs() << "tail: " << *tail_inst_ << "\n"; } 
    errs() << "used vars:\n";
    for (AllocaInst *AI: used_vars_) { errs() << *AI << "\n"; } 
    errs() << "\ndefined vars:\n";
    for (AllocaInst *AI: defined_vars_) { errs() << *AI << "\n"; }
    errs() << "\n";
}


//------------------------ Class NodeSeq -------------------------

// split F into a sequence of PDGNodes
std::vector<DGNode *> NodeSeq::construct() { 
	std::set<BranchInst *> branches;
   	for (inst_iterator I = inst_begin(F_); I != inst_end(F_); I++) {
   		// 1a. Create the Initial Node.
		creatInitialNode();
		Instruction *inst = dyn_cast<Instruction>(&*I);
		if (inst_node_map_.find(inst) != inst_node_map_.end()) { continue; }
		// 1b. Create the other Nodes.
        DGNode *dg_node = new DGNode;
        node_insts_map_[dg_node] = std::set<Instruction *>();
        node_insts_map_[dg_node].insert(inst);
        //Default Normal nodes 
        inst_node_map_[inst] = dg_node;
        // 2. Add used vars + Merge nodes
        for (Use &U : inst->operands()) {
			if (AllocaInst *AI = dyn_cast<AllocaInst>(U.get())) {
				dg_node->addUsedVar(AI);
			} else if (Instruction *varInst = dyn_cast<Instruction>(U.get())) {
				//merge nodes of varInst and inst; Return merged node address
				dg_node = mergeNodes(varInst, inst);
			}
		}
        // 3. Calculate successors
		if (CallInst *CI = dyn_cast<CallInst>(inst)) {
			setNodeSuccsOfCall(CI, dg_node);
      	} else if (StoreInst *st_inst = dyn_cast<StoreInst>(inst)) {
			dg_node->addDefinedVar(getChangedAI(st_inst));
      	} else if (dyn_cast<ReturnInst>(inst)) {
			dg_node->setNodeType(ReturnTy);
   		} else if (BranchInst *br_inst = dyn_cast<BranchInst>(&*I)) {
			branches.insert(br_inst);	
		}
   	}
	// 4. Process branches (if-else, loop)
	std::set<BranchInst *> processed_branches;
	for (BranchInst *br_inst : branches) {
		if (!setHas(processed_branches, br_inst)) {
			auto curr_processed_branches = processBranch(br_inst);
			processed_branches.insert(curr_processed_branches.begin(), curr_processed_branches.end());
		}
	}
   	//5. Set the head and tail of each Node and construct node_seq_
   	constructNodeSeq();
   	return node_seq_;
}

// process branches (if-else, loop)
std::set<BranchInst *> NodeSeq::processBranch(BranchInst *br_inst) {
	std::queue<BasicBlock *> work_list;
	std::set<BasicBlock *> bbs_to_merge;
	std::set<BranchInst *> processed_branches;
	processed_branches.insert(br_inst);
	for (unsigned int i = 0; i < br_inst->getNumSuccessors(); i++) {
		work_list.push(br_inst->getSuccessor(i));
	}
	while (!work_list.empty()) {
		Instruction *inst = work_list.front()->getTerminator();
		work_list.pop();
		if (!dyn_cast<BranchInst>(inst)) { continue; }
		BranchInst *another_branch_inst = dyn_cast<BranchInst>(inst);
		processed_branches.insert(another_branch_inst);
		for (unsigned int i = 0; i < another_branch_inst->getNumSuccessors(); i++) {
			BasicBlock *bb = another_branch_inst->getSuccessor(i);
			if (setHas(bbs_to_merge, bb)) { continue; }
			work_list.push(bb);
			bbs_to_merge.insert(bb);
		}
	}
	for (BasicBlock *bb : bbs_to_merge) {
		for (BasicBlock::iterator I = bb->begin(); I != bb->end(); I++) {
			Instruction *inst = dyn_cast<Instruction>(&*I);
			if (inst_node_map_[inst]->getNodeType() == ReturnTy) { continue; }
			mergeNodes(br_inst, inst);
		}
	}
	return processed_branches;
} 

void NodeSeq::creatInitialNode() {
// initial node = AllocaInsts + Passing Args
	DGNode *initial_node = new DGNode;
	initial_node->setNodeType(InitialTy);
	for (inst_iterator I = inst_begin(F_); I != inst_end(F_); I++) {
		// 1. Detect the Initial Node, FuncCall Node(s), Normal node(s) and Retuen Node(s).
		if (AllocaInst *AI = dyn_cast<AllocaInst>(&*I)) {
   			inst_node_map_[AI] = initial_node;
   			continue;
   		} else if (StoreInst *st_inst = dyn_cast<StoreInst>(&*I)) {
            if (dyn_cast<Argument>(st_inst->getValueOperand())){
            	inst_node_map_[st_inst] = initial_node;
            	AllocaInst *defined_var = dyn_cast<AllocaInst>(st_inst->getPointerOperand());
            	initial_node->addDefinedVar(defined_var);
            	continue;
            } 
        } 
        break;
	}
}

// merge nodes of varInst and inst; Return merged node address
DGNode *NodeSeq::mergeNodes(Instruction *inst1, Instruction *inst2) {
	if (inst_node_map_.find(inst1) == inst_node_map_.end() || 
		inst_node_map_.find(inst2) == inst_node_map_.end()) { 
		errs() << "Cannot find nodes to merge!\n";
		std::exit(EXIT_FAILURE); 
	}
	DGNode *dg_node = inst_node_map_[inst2];
	if (dg_node == inst_node_map_[inst1]) { return dg_node; }
	inst_node_map_[inst1]->mergeWithNode(dg_node);
    std::set<Instruction*> contained_insts = node_insts_map_[dg_node];
	node_insts_map_.erase(dg_node);
	free(dg_node);
	dg_node = inst_node_map_[inst1];
	node_insts_map_[dg_node].insert(contained_insts.begin(), contained_insts.end());
	for (Instruction *contained_inst : contained_insts) {
		inst_node_map_[contained_inst] = dg_node;
	}
	return dg_node;
}

// Calculate the successors of 
void NodeSeq::setNodeSuccsOfCall(CallInst *CI, DGNode *dg_node) {
	// args of CI 
	for (Use &U : CI->arg_operands()) {
		if (Instruction *arg_inst = dyn_cast<Instruction>(U.get())) {
			dg_node->pushBackCallArg(getChangedAI(arg_inst));
		} else {
			dg_node->pushBackCallArg(nullptr);
		}
	}
	// successors of call (changed array args inside function)
	Function *called_func = CI->getCalledFunction();
	dg_node->addCalledFunc(called_func);
	// system call <==> noraml;  user call <==> FuncCall;
	if (!called_func->empty()) { 
    	dg_node->setNodeType(UserCallTy);
		if (func_changed_array_args_map_.find(called_func) == func_changed_array_args_map_.end()){
			errs() << "Called Function NOT found!\n";
				std::exit(EXIT_FAILURE);
		}
		for (int argNo : func_changed_array_args_map_[called_func]) {
			if (dg_node->getCallArg(argNo) != nullptr) { dg_node->addDefinedVar(dg_node->getCallArg(argNo)); }
        }
	} else { // system call 
	    dg_node->setNodeType(SysCallTy);
		if (called_func->getName() == "printf") { return; };
		if (called_func->getName().find("llvm.memcpy") !=std::string::npos) {
			if (dg_node->getCallArg(0) != nullptr) { dg_node->addDefinedVar(dg_node->getCallArg(0)); }
			return;
		}
		// other system calls -- assume all the array args are changed  
		for (AllocaInst *AI : dg_node->getCallArgs()) {
			if (AI != nullptr) dg_node->addDefinedVar(AI);
		}
	}
}

// Set the head and tail of each Node and construct node_seq_
void NodeSeq::constructNodeSeq() {
	int node_id = 0;
   	DGNode *curr_node = nullptr;
   	for (inst_iterator I = inst_begin(F_); I != inst_end(F_); I++) {
        Instruction *inst = dyn_cast<Instruction>(&*I);
   		if (curr_node == nullptr || curr_node != inst_node_map_[inst]) {
   			if (curr_node != nullptr) { node_seq_.push_back(curr_node); }
   			curr_node = inst_node_map_[inst];
   			curr_node->setNodeID(node_id++);
   			curr_node->setNodeHead(inst);
   		} 
   		curr_node->pushBackInst(inst);
   		curr_node->setNodeTail(inst);
   }
   node_seq_.push_back(curr_node);   
#ifdef DEBUG_TYPE
   	// 6. Print Out Node Sequences
   	errs() << "Function " << F_->getName() << " Node sequences:\n";
   	for (DGNode *node : node_seq_) {
 		node->printInfo();
   	}
#endif 
}


   
 
//------------------------ Class PDG -------------------------


// data depdence (branch -- single node)
// do not consider GlobalVars (const in our test examples)
void PDG::construct(std::map<Function *, std::set<int>> func_changed_array_args_map) {
	// construct node_seq_
	NodeSeq node_seq(F_, func_changed_array_args_map);
	node_seq_ = node_seq.construct(); 
    analyzeChangedArrayArgs();
	for (DGNode *node : node_seq_) {
		use_defs_map_[node] = std::set<DGNode *>();
		def_uses_map_[node] = std::set<DGNode *>();
	}
    // node_seq_ is single path --> do not need worklist any more
    std::map<AllocaInst *, DGNode *> in_set;
	std::map<AllocaInst *, DGNode *> out_set;
	for (DGNode *node : node_seq_) {
		in_set.insert(out_set.begin(), out_set.end());
		for (AllocaInst *aI : node->getUsedVars()) {
			if (in_set.find(aI) != in_set.end()) {
				DGNode *node_def_var = in_set[aI];
				use_defs_map_[node].insert(node_def_var);
				def_uses_map_[node_def_var].insert(node);
				std::pair<DGNode *, DGNode *> du_pair = std::make_pair(node_def_var, node);
				if (du_aI_map_.find(du_pair) == du_aI_map_.end()) { du_aI_map_[du_pair] = std::set<AllocaInst *>(); }
				du_aI_map_[du_pair].insert(aI);
			}
		}
		// out = in - kill + gen; genset == definded_vars
		for (AllocaInst *aI : node->getDefinedVars()) {
			out_set[aI] = node;
		}
    }   
}


// analyze the array/ptr args changed inside the function
void PDG::analyzeChangedArrayArgs() {
    std::map<AllocaInst *, int> ArguLocalAINosMap;
    // decide AllocaInst of arg (only consider array/ptr)
    for (inst_iterator I = inst_begin(F_); I != inst_end(F_); I++) {
        if (StoreInst *stI = dyn_cast<StoreInst>(&*I)) {
            if (Argument *arg = dyn_cast<Argument>(stI->getValueOperand())){
                AllocaInst *argLocalAI = dyn_cast<AllocaInst>(stI->getPointerOperand());
                ArguLocalAINosMap[argLocalAI] = arg->getArgNo();
                args_[arg->getArgNo()] = argLocalAI;
            }
        }
    }
    for (DGNode *node : node_seq_) {
    	if (node->getNodeType() == InitialTy) { continue; }
    	for (AllocaInst *aI : node->getDefinedVars()) {
    		if (ArguLocalAINosMap.find(aI) != ArguLocalAINosMap.end()) {
    			changed_array_args_.insert(ArguLocalAINosMap[aI]);
    		}
    	}
    }
}


void PDG::printDefToUsesMap() {
	errs() << "============DefToUsesMap============\n";
    for (auto iter = def_uses_map_.begin(); iter != def_uses_map_.end(); iter ++){
        if (!iter->second.empty()) {
        	errs() << "Node" << iter->first->getNodeID() << " SuccNodeSet: { ";
            std::set<DGNode *> succSet = iter->second;
            for (DGNode *node : succSet) {
                errs() << node->getNodeID() << " ";
            }
            errs() << "}\n";
        }
    }
    errs() << "==============================\n";
}
    
void PDG::printDUAIMap() {
	errs() << "============DUVarsMap============\n";
	for (auto iter = du_aI_map_.begin(); iter != du_aI_map_.end(); iter ++){
       std::pair<DGNode *, DGNode *> duPair = iter->first;
       errs() << "[defInst, useInst] = [Node" << duPair.first->getNodeID()
       << ", " << duPair.second->getNodeID() << "] varNames = { ";
       for (AllocaInst *AI : iter->second) {
   	    	errs() << AI->getName() << " ";
       }
       errs() << "}\n";
   	}
	errs() << "==============================\n";
}

