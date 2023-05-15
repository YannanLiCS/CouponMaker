//===- mm_IntraAnalysis.h  -------*- C++ -*-===//
//
//                     The CouponMaker Infrastructure
//
//===----------------------------------------------------------------------===//
//
// This file provides intra-procedual analysis interfaces used to implement CouponMaker.
//
//===----------------------------------------------------------------------===//


#ifndef DATA_STRUCTURE_H
#define DATA_STRUCTURE_H

#include "llvm/ADT/GraphTraits.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Dominators.h"

#define MAX_NVM_BYTE_NUM 3000
#define MAX_NVM_INT32_NUM 3000

using namespace llvm;

typedef std::pair<Instruction *, Instruction *> InstPair;
typedef std::list<Instruction *> InstList;
typedef std::map<Instruction *, int> instIntMap;
typedef std::set<Instruction *> InstSet;
typedef std::map<Instruction *, InstSet> Inst2InstSet;
typedef std::set<AllocaInst *> AllocaInstSet;



// Initial: Allocate and assign incoming parameters -- level 1
// Normal: Single Instruction -- level 4
// UserCall/SysCall:  Function call (including passing parameter) -- level 3
// Branch: Loop/Branch (+ Function call)  -- level 2
// Return: -- level 0 (highest)
enum NodeType { InitialTy, UserCallTy, SysCallTy, BranchTy, NormalTy, ReturnTy };
enum SecType { InitialType, PrecompType, OnlineType, ReturnType };
enum NVMUnitTy { Int32, BYTE };

namespace llvm {


// smallest unit for intra-dependecy analysis with each funtion;
// nodes construct a single path in Function.
// because in benchmarks there are nearly no if-statement
// and loops are usually to assign a array
class DGNode {

	int id_ = 0;
    
    // Defualt type: Normal
    NodeType node_type_ = NormalTy;
    
    // the called function and args of UserCall/SysCall;
    Function *called_func_ = nullptr;    
    std::vector<AllocaInst *> call_args_;
    
    // first/last inst in this node
    Instruction *head_inst_ = nullptr;
    Instruction *tail_inst_ = nullptr;
    // contained instructions in this node
    std::vector<Instruction *> contained_insts;
    
    // all the variables used by this node
    AllocaInstSet used_vars_;
    
    // all the variables changed by this node
    AllocaInstSet defined_vars_;
    
public:
    DGNode() {}
    
    void setNodeID(int id) { id_ = id; }
    
    void addCalledFunc(Function *called_func) { called_func_ = called_func;}
    
    void pushBackCallArg(AllocaInst * arg) { call_args_.push_back(arg); }
    
    void addUsedVar(AllocaInst *AI) { used_vars_.insert(AI); }   
    
    void addDefinedVar(AllocaInst *AI) { defined_vars_.insert(AI); }
    
    void setNodeType(NodeType node_type) { node_type_ = node_type; }
    
    void setNodeHead(Instruction *head) { head_inst_ = head; }
    
    void setNodeTail(Instruction *tail) { tail_inst_ = tail; }
    
    void pushBackInst(Instruction *inst) { contained_insts.push_back(inst); }
    
    int getNodeID() { return id_; }
    
    Function *getCalledFunc() { return called_func_; }
    
    AllocaInst *getCallArg(int index) { return call_args_[index]; }
    
    std::vector<AllocaInst *> getCallArgs() { return call_args_; }
    
    NodeType getNodeType() { return node_type_; }
    
    Instruction *getNodeHead() { return head_inst_; }
    
    Instruction *getNodeTail() { return tail_inst_; }
    
    std::vector<Instruction *> getContainedInsts() { return contained_insts; }
    
    const AllocaInstSet &getUsedVars() { return used_vars_; }
    
    const AllocaInstSet &getDefinedVars() { return defined_vars_; }
    
    // deal with Node type
    // merge used_vars_ and defined_vars_
    void mergeWithNode(DGNode *dg_node_2);
    
    // print information for debug; given node index
    void printInfo();
};


// Construct the node sequence of function F
// Each node contains used and defined AllocaInsts
class NodeSeq {
	
	Function *F_;
	
	std::map<Function *, std::set<int>> func_changed_array_args_map_;
	
	std::vector<DGNode *> node_seq_;
	
	std::map<Instruction *, DGNode *> inst_node_map_;
    
    // node -> contained instructions
    std::map<DGNode *, std::set<Instruction *>> node_insts_map_;

public:	
	NodeSeq(Function *F, std::map<Function *, std::set<int>> func_changed_array_args_map)
		: F_(F), func_changed_array_args_map_(func_changed_array_args_map) {}
		
	std::vector<DGNode *> construct();
	
	
private:
	void setNodeSuccsOfCall(CallInst *CI, DGNode *dg_node);
	
	// merge the nodes of inst1 and inst2; Return the merged node address
	DGNode *mergeNodes(Instruction *inst1, Instruction *inst2);
    
    void constructNodeSeq();
    
    void creatInitialNode();
    
    std::set<BranchInst *> processBranch(BranchInst *br_inst);
};



// Program Depdence Graph of function F
// analysis includes three parts:
// (1) dependence between instructions;
// (2) dependence from args to instructions;
// (3) which arg arrays are changed inside F.
class PDG {
	Function *F_;
	
	// the node sequence of some function
	std::vector<DGNode *> node_seq_;
	
	// argNo -- arg AI
	std::map<int, AllocaInst *> args_;
	
	// the changed array args inside function
	std::set<int> changed_array_args_;	
	
    // node (use vars) -- nodes defining vars   [predseccors]
    std::map<DGNode *, std::set<DGNode *>> use_defs_map_;
    
    // node (define vars) -- nodes using vars   [successors]
    std::map<DGNode *, std::set<DGNode *>> def_uses_map_;
    
    // <def, use> -- var set (may be more than one vars)
    std::map<std::pair<DGNode*, DGNode*>, std::set<AllocaInst *>> du_aI_map_;
	
public:
	PDG(Function *F) : F_(F) {}
	
	void construct(std::map<Function *, std::set<int>> func_changed_array_args_map);
	
	std::vector<DGNode *> getNodeSeq() { return node_seq_; }
	
	std::map<int, AllocaInst *> getArgs() { return args_; }
	
	// return changed_array_args in F_
	std::set<int> getChangedArrayArgs() { return changed_array_args_; }
    
    std::set<DGNode *> getUsesOfNode(DGNode *node) { return def_uses_map_[node]; }
    
    std::set<DGNode *> getDefsOfNode(DGNode *node) { return use_defs_map_[node]; }
    
    const AllocaInstSet &getDUVars(std::pair<DGNode*, DGNode*> DUPair) { return du_aI_map_[DUPair]; };
    
    void printDefToUsesMap();
    
    void printDUAIMap();
    
private:
    
    void analyzeChangedArrayArgs();
	
};


class Sections{
    int sec_cnt_;
    
    // map from sectionID to secType
    std::map<int, SecType> sec_type_map_;
    
    // map from sectionID to a list of instructions in this section
    std::map<int, InstList> sec_insts_map_;
    
    // map from instruction to secID
    std::map<Instruction *, int> inst_sec_map_;
    
    // map from sectionID to the head Instruction
	std::map<int, Instruction *> sec_head_map_;
    
    // map from sectionID to the tail Instruction
	std::map<int, Instruction *> sec_tail_map_;
	
	// map from setionID to a set of all the variables used in this section
	std::map<int, AllocaInstSet> sec_usedVars_map_;
	
	// map from setionID to a set of all the variables defined in this section
	std::map<int, AllocaInstSet> sec_definedVars_map_; 
	
	// map from a precomp sectionID to a set of online section IDs which has dependecy on it
	std::map<int, std::set<int>> sec_couponSuccs_map_;
	
	// <precomp secID, online secID> -- var set (may be more than one vars)
    std::map<std::pair<int, int>, std::set<AllocaInst *>> du_aIs_map_;
	
    
public:
    Sections(){
        sec_cnt_ = 0;
        sec_type_map_ = std::map<int, SecType>();
        sec_insts_map_ = std::map<int, InstList>();
        inst_sec_map_ = std::map<Instruction *, int>();
        sec_head_map_ = std::map<int, Instruction *>();
        sec_tail_map_ = std::map<int, Instruction *>();
    }
    
    // increse the number of sections
    void increseSecCnt() { sec_cnt_++; }
    
    // get the number of sections
    int getSecCnt() { return sec_cnt_; }
    
    void setSecHead(int sec_id, Instruction *head) { sec_head_map_[sec_id] = head; }

	void setSecTail(int sec_id, Instruction *tail) { sec_tail_map_[sec_id] = tail; }
	
	void insertSecUsedVars(int sec_id, const AllocaInstSet &used_vars) { sec_usedVars_map_[sec_id].insert(used_vars.begin(), used_vars.end()); };
	
	void insertSecDefinedVars(int sec_id, const AllocaInstSet &defined_vars) { sec_definedVars_map_[sec_id].insert(defined_vars.begin(), defined_vars.end()); };
	
	void setSecCouponSuccs(int sec_id, int succ_id) { sec_couponSuccs_map_[sec_id].insert(succ_id); }
	
	void setDUAIs(int def_sec_id, int use_sec_id, const AllocaInstSet &vars) { du_aIs_map_[{def_sec_id, use_sec_id}].insert(vars.begin(), vars.end()); }
	
	void setSecType(int sec_id, SecType sec_type) {sec_type_map_[sec_id] = sec_type; }
	    
	Instruction *getSecHead(int sec_id) { return sec_head_map_[sec_id]; }
    
    Instruction *getSecTail(int sec_id) { return sec_tail_map_[sec_id]; }
    
    const AllocaInstSet &getSecUsedVars(int sec_id) { return sec_usedVars_map_[sec_id]; }
    
    const AllocaInstSet &getSecDefinedVars(int sec_id) { return sec_definedVars_map_[sec_id]; }
    
    const std::map<int, std::set<int>> &getSecCouponSuccsMap() { return sec_couponSuccs_map_; }
    
    const AllocaInstSet &getDUVars(int sec_id, int succ_sec_id) { return du_aIs_map_[{sec_id, succ_sec_id}]; }
    
	SecType getSecType(int sec_id) { return sec_type_map_[sec_id]; }
    
//-------------------below functions are useless    
    // create a section given a secID, head of this section, secType
    void CreateSection(int secID, Instruction *head, SecType secType);
   
    // push inst at the end of section
    void PushBakcToSec(int secID, Instruction *inst);
    
    // return the secID of a given instruction
    int getSecIDOfInst(Instruction *inst) { return inst_sec_map_[inst]; }
    
    // return the list of all its instructions given a secID
    InstList getInstsOfSec(int sec_id) { return sec_insts_map_[sec_id]; }
    
    // return the fist online secID
    // if there are no online sections, return 0
    int getFirstOnlineSecID();
    
    // return the last pre secID
    // if there are no pre sections, return 0
    int getLastPreSecID();
    
    void printInfo();

};


// operations of NVMem
class NVMem {
    Module &M;
    Function *F;
private: int index_for_BYTE;
private: int index_for_int32;
    IntegerType *Int64Ty, *Int32Ty, *Int8Ty;
    PointerType *Int8PointerTy, *Int32PointerTy;
    Function *NVMStoreI32, *NVMStoreI8, *NVMStoreAI8, *NVMStoreAI32;
    Function *NVMLoadI32, *NVMLoadI8, *NVMLoadAI8, *NVMLoadAI32, *NVMLoadPtrI8, *NVMLoadPtrI32;
    Function *print64;
    Function *NVMStoreFile, *NVMLoadFile;

public:
    NVMem(Module &M, Function *F): M(M), F(F) {
        initNVMOperations();
        index_for_BYTE = 0;
        index_for_int32 = 0;
    }
    
    // insert NVMStore <index, value> before the last instruction of section
    void inset_NVMStore(Instruction *inst, IRBuilder<> builder,
                                    std::vector<AllocaInst *> coupons);
    // insert NVMLoad <index, value> at the begining of section
    void inset_NVMLoad(Sections *secs, int sec_id, IRBuilder<> builder, 
                          			std::vector<AllocaInst *> coupons);

    // update the index of next NVMem Coupon
    void updateIndex(NVMUnitTy couponUnitTy, int couponSize);
    
    // insert NVMStoreFile before inst
    void insert_NVMStoreFile(Instruction *inst, IRBuilder<> builder);
    
    // insert NVMLoadFile before inst
    void insert_NVMLoadFile(Sections *secs, int secID, IRBuilder<> builder);
    
private:
    void initNVMOperations();
    
};
    
} //end of namespace


#endif
