//===- Transform.cpp  -------*- C++ -*-===//
//
//                     The CouponMaker Transformation
//
//===----------------------------------------------------------------------===//
//
// This file provides implementation to transform the program.
//
//===----------------------------------------------------------------------===//


#include "Infrastructure.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include <limits.h>


#define DEBUG_TYPE "Transform"

using namespace llvm;

typedef std::pair<int, int> IntPair;

namespace {
    // return the max secID containing defs instruction
    Instruction *getLastPreDefInst(InstSet defsSet, Sections secs) {
        Instruction *retInst = nullptr;
        for (Instruction *defInst : defsSet) {
            // if pre-computing defInst
            if (secs.getSecType(secs.getSecIDOfInst(defInst)) == PrecompType) {
                if (!retInst) {
                    retInst = defInst;
                } else if (secs.getSecIDOfInst(defInst) > secs.getSecIDOfInst(retInst)) {
                    retInst = defInst;
                }
            }
        }
        return retInst;
    }
}  //namespace



void CouponMaker::transformProgram() {
	for (auto iter = func_precomp_nodes_.begin(); iter != func_precomp_nodes_.end(); iter ++){
       Function *func = iter->first;
       transformFunction(func);
   	}
}

void CouponMaker::transformFunction(Function *func) {
    // split the func into sections
	Sections *secs = splitSections(func); 
	/// insert if-statement to each section
    /// NOTE: backwards! update the sec heads backward
    ///        to guarantee the correctnesss of elseBB
    AllocaInst *precomp_flag_aI = getAIbyName(func, "precompFlag.addr");
    AllocaInst *online_flag_aI = getAIbyName(func, "onlineFlag.addr");
    assert(precomp_flag_aI != nullptr);
    assert(online_flag_aI != nullptr);
    for (int sec_id = secs->getSecCnt() - 2; sec_id >= 1; sec_id --) {
        if (secs->getSecType(sec_id) == PrecompType){      //pre-computing section
            insertBranch(func, precomp_flag_aI, secs, sec_id);
        } else if (secs->getSecType(sec_id) == OnlineType){  //online-computing section
            insertBranch(func, online_flag_aI, secs, sec_id);
        }
    }
#ifdef DEBUG_TYPE
	errs() << *func;
#endif
	free(secs);
}

Sections *CouponMaker::splitSections(Function *func) {
	std::set<DGNode *> precomp_nodes = func_precomp_nodes_[func];
    Sections *secs = new Sections;
    std::map<DGNode *, int> node_sec_map;
    SecType prev_sec_type = InitialType;
    SecType curr_sec_type;
    for (DGNode *node: func_pdg_map_[func]->getNodeSeq()) {
    	curr_sec_type = calcuSecType(node->getNodeType(), node, precomp_nodes);
    	// merge continous precomp / online nodes
    	// but for a different-type node, create a new secssion
    	if (curr_sec_type != prev_sec_type
    		|| curr_sec_type == InitialType 
    		|| curr_sec_type == ReturnType) {
    		secs->increseSecCnt();
    		secs->setSecType(secs->getSecCnt() - 1, curr_sec_type);
    		secs->setSecHead(secs->getSecCnt() - 1, node->getNodeHead());
    		prev_sec_type = curr_sec_type;
    	}
    	node_sec_map[node] = secs->getSecCnt() - 1;
    	secs->setSecTail(secs->getSecCnt() - 1, node->getNodeTail()); 
    	secs->insertSecUsedVars(secs->getSecCnt() - 1, node->getUsedVars());
    	secs->insertSecDefinedVars(secs->getSecCnt() - 1, node->getDefinedVars());
    }
    // detect du-pair between precomp_section and online_section
    PDG *pdg = func_pdg_map_[func];
    for (DGNode *node: func_pdg_map_[func]->getNodeSeq()) {
    	int sec_id = node_sec_map[node];
    	if (secs->getSecType(sec_id) != PrecompType) { continue; }
    	for (DGNode *succ : pdg->getUsesOfNode(node)) {
    		int succ_sec_id = node_sec_map[succ];   	
    		if (secs->getSecType(succ_sec_id) != OnlineType) { continue; }
    		secs->setSecCouponSuccs(sec_id, succ_sec_id);
    		secs->setDUAIs(sec_id, succ_sec_id, pdg->getDUVars({node, succ}));
    	}
    }
    for (int sec_id = 1; sec_id < secs->getSecCnt(); sec_id ++)	{ 
    	SplitBB(secs, sec_id); 
    }
#ifdef DEBUG_TYPE
	errs() << *func;
	for (int sec_id = 0; sec_id < secs->getSecCnt(); sec_id ++) {
		errs() << "Section " << sec_id 
			<< ": type " << secs->getSecType(sec_id) << "\n"
			<< " head: " << *secs->getSecHead(sec_id) << "\n"
			<< " tail: " << *secs->getSecTail(sec_id) << "\n";
		errs() << "used vars:\n";
    	for (AllocaInst *AI: secs->getSecUsedVars(sec_id)) { errs() << *AI << "\n"; } 
    	errs() << "\ndefined vars:\n";
    	for (AllocaInst *AI: secs->getSecDefinedVars(sec_id)) { errs() << *AI << "\n"; }
    	errs() << "\n";
	}
	errs() << "\n";
#endif
	return secs;
}


SecType CouponMaker::calcuSecType(NodeType node_type, DGNode *dg_node, std::set<DGNode *> precomp_nodes) {
	if (node_type == InitialTy) { return InitialType; }
	if (node_type == ReturnTy) { return ReturnType; }
	if (precomp_nodes.find(dg_node) != precomp_nodes.end()) { return PrecompType; }
	return OnlineType;
}

AllocaInst *CouponMaker::getAIbyName(Function *F, StringRef varName){
	for (inst_iterator I = inst_begin(F); I != inst_end(F); I++){
		if (AllocaInst *aI = dyn_cast<AllocaInst>(&*I)){
			if (aI->getName().equals(varName)){
				return aI;
			}
		} 
	}
	return nullptr;
}

// split basic block at instruction inst
void CouponMaker::SplitBB(Sections *secs, int sec_id) {
	Instruction *sec_head = secs->getSecHead(sec_id);
	BasicBlock *bb = sec_head->getParent();
	if (sec_head!= &*bb->begin()) {
		bb->splitBasicBlock(sec_head);
		/// NOTE: need to update tail(i - 1) to br(head(i))
		secs->setSecTail(sec_id - 1, bb->getTerminator());
	}
}

/// insert if branch before current bb (thenBB)
/// create a bb -- if.cond
/// if.cond >>> thenBB / elseBB
void CouponMaker::insertBranch(Function *func, Value *condition_value,
                                          Sections *secs, int sec_id) {
    IRBuilder<> builder(M_.getContext());
    BasicBlock *predBB = secs->getSecTail(sec_id - 1)->getParent();
    BasicBlock* thenBB = secs->getSecHead(sec_id)->getParent();
    BasicBlock* elseBB = secs->getSecHead(sec_id + 1)->getParent();
    /// create a bb between <preBB, thenBB>
    /// NOTE: cannot remove terminator at first
    ///         e.g., there is only one inst in a bb
    BasicBlock *condBB = BasicBlock::Create (M_.getContext(), "", func, thenBB);
    // set predcessorBB of condBB
    BranchInst::Create(condBB, predBB->getTerminator());
    predBB->getTerminator()->eraseFromParent();
    // update the tailInst of preSec to the new tail(preBB)
    secs->setSecTail(sec_id - 1, predBB->getTerminator());

    // Insert the branch instruction for the "if"
    builder.SetInsertPoint(condBB);
    Instruction *load = builder.CreateLoad(condition_value);
   	IntegerType *Int32Ty = IntegerType::get(M_.getContext(), 32);	//precompFlag / onlineFlag type
    Value *int_true = ConstantInt::get(Int32Ty, 1, true);
    Value *cmp = builder.CreateICmpEQ(load, int_true, "cmp");
    builder.CreateCondBr(cmp, thenBB, elseBB);
    // update the headInst of section to the head(if.cond)
    secs->setSecHead(sec_id, &*condBB->begin());
}


// detect coupon and insert NVM Store/Load coupons instructions
void CouponMaker::processCoupons(Function *func, Value *_coupon,
                               			Sections *secs, int sec_id) {
  	IRBuilder<> builder(M_.getContext());
  	/// insert instructions to deal with coupon
    /// note that the program is well-formed here
    /// any branch of the original program are included within a section
    NVMem nvMem(M_, func);
    // precomp sec ids -- vars to store (order matters)
    std::map<int, std::vector<AllocaInst *>> sec_stores_map;
    // precomp sec ids -- vars to load (same order)
    std::map<int, std::vector<AllocaInst *>> sec_loads_map;
 	// map from a precomp sectionID to a set of online section IDs which has dependecy on it   
    const auto sec_succ_map = secs->getSecCouponSuccsMap();
    for (auto iter = sec_succ_map.begin(); iter != sec_succ_map.end(); iter ++) {
    	sec_id = iter->first;
    	for (const auto succ_id : iter->second) {
    		for (AllocaInst *coupon : secs->getDUVars(sec_id, succ_id)) {
    		    sec_stores_map[sec_id].push_back(coupon);
    		    sec_loads_map[succ_id].push_back(coupon);
    		}
    	}
    }
    // insert instructions to store coupons
    for (auto iter = sec_stores_map.begin(); iter != sec_stores_map.end(); iter ++) {
        nvMem.inset_NVMStore(secs->getSecTail(iter->first), builder, iter->second);
    }
    // insert instructions to load coupons
    for (auto iter = sec_loads_map.begin(); iter != sec_loads_map.end(); iter ++) {
        nvMem.inset_NVMLoad(secs, iter->first, builder, iter->second);
    }
}
