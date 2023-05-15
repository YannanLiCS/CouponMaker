//===- mm_IntraAnalysis.h  -------*- C++ -*-===//
//
//                     The CouponMaker Infrastructure
//
//===----------------------------------------------------------------------===//
//
// This file provides intra-procedual analysis interfaces used to implement A2PC.
//
//===----------------------------------------------------------------------===//


#include "DataStructure.h"
#include <tuple>
#include <fstream>
#include <stack>
#include "llvm/Analysis/Interval.h"
#include "llvm/Analysis/CallGraph.h"

// TODO: tmp size for pointer (how to decide the size of pointer)
#define KEY_SIZE 2
#define DES_BLOCK_SIZE 8
#define CAMELLIA_BLOCK_SIZE 16

typedef std::set<Function *> FuncSet;


typedef std::pair<AllocaInst *, StoreInst *> varDefPair;
// map ==> one var can have at most one def ==> it is reaonsable within a BB
typedef std::map<AllocaInst *, StoreInst *> varDefMap;



//#define DEBUG_TYPE "Coupon"

using namespace llvm;


namespace  {
    template<typename T> bool setHas(std::set<T> set, T item) {
        if (set.find(item) != set.end()) {
            return true;
        }
        return false;
    }
}   //namespace



// create a section given a secID, head of this section, secType
void Sections::CreateSection(int sec_id, Instruction *head,  SecType sec_type) {
    sec_type_map_[sec_id] = sec_type;
    sec_head_map_[sec_id] = head;
    sec_insts_map_[sec_id] = std::list<Instruction *>();
    sec_insts_map_[sec_id].push_back(head);
    inst_sec_map_[head] = sec_id;
}


// push inst at the end of section
void Sections::PushBakcToSec(int sec_id, Instruction *inst) {
    sec_insts_map_[sec_id].push_back(inst);
    inst_sec_map_[inst] = sec_id;
}


// return the fist online secID
// if there are no online sections, return 0
int Sections::getFirstOnlineSecID(){
    for (int i = 0; i < sec_cnt_; i ++) {
        if (getSecType(i) == OnlineType) {
            return i;
        }
    }
    return 0;
}


// return the last pre secID
// if there are no pre sections, return 0
int Sections::getLastPreSecID(){
    for (int i = sec_cnt_ - 1; i >= 0; i --) {
        if (getSecType(i) == PrecompType) {
            return i;
        }
    }
    return 0;
}



// insert NVMStore <index, value> before inst
void NVMem::inset_NVMStore(Instruction *inst, IRBuilder<> builder,
                                  std::vector<AllocaInst *> coupons) {
    for (AllocaInst *coupon : coupons) {
        std::vector<Value *> args = std::vector<Value*>();
        Type *couponType = coupon->getAllocatedType();
        builder.SetInsertPoint(inst);
        int local_ind_int = index_for_int32;
        int local_ind_BYTE = index_for_BYTE;
        if (couponType->isArrayTy() &&
            couponType->getArrayElementType()->isIntegerTy(8)) {
            args.push_back(ConstantInt::get(Int32Ty, local_ind_BYTE, true));
            std::vector<Value*> args_sep = std::vector<Value*>();
            args_sep.push_back(ConstantInt::get(Int32Ty, 0, true));
            args_sep.push_back(ConstantInt::get(Int32Ty, 0, true));
            GetElementPtrInst *getelementptr = GetElementPtrInst::CreateInBounds(couponType, coupon,
                                                                                 args_sep, "", inst);
            args.push_back(getelementptr);
            args.push_back(ConstantInt::get(Int32Ty, couponType->getArrayNumElements(), true));
            builder.CreateCall(NVMStoreAI8, args);  //insert before inst
            local_ind_BYTE += (int)couponType->getArrayNumElements();
        } else if (couponType->isArrayTy() &&
                   couponType->getArrayElementType()->isIntegerTy(32)) {
            args.push_back(ConstantInt::get(Int32Ty, local_ind_int, true));
            std::vector<Value*> args_sep = std::vector<Value*>();
            args_sep.push_back(ConstantInt::get(Int32Ty, 0, true));
            args_sep.push_back(ConstantInt::get(Int32Ty, 0, true));
            GetElementPtrInst *getelementptr = GetElementPtrInst::CreateInBounds(couponType, coupon,
                                                                                     args_sep, "", inst);
            args.push_back(getelementptr);
            args.push_back(ConstantInt::get(Int32Ty, couponType->getArrayNumElements(), true));
            builder.CreateCall(NVMStoreAI32, args);  //insert before inst
            local_ind_int += (int)couponType->getArrayNumElements();
        }
        else if (couponType->isIntegerTy(32)) {
            args.push_back(ConstantInt::get(Int32Ty, local_ind_int, true));
            LoadInst *ldCoupon = builder.CreateLoad(Int32Ty, coupon);
            args.push_back(ldCoupon);
            builder.CreateCall(NVMStoreI32, args);  //insert before inst
            local_ind_int += 1;
        } else if (couponType->isIntegerTy(8)) {
            args.push_back(ConstantInt::get(Int32Ty, local_ind_BYTE, true));
            LoadInst *ldCoupon = builder.CreateLoad(Int8Ty, coupon);
            args.push_back(ldCoupon);
            builder.CreateCall(NVMStoreI8, args);  //insert before inst
            local_ind_BYTE += 1;
        } else if (couponType->isPointerTy() &&
                   couponType->getPointerElementType()->isIntegerTy(32)) {
            args.push_back(ConstantInt::get(Int32Ty, local_ind_int, true));
            LoadInst *ldCoupon = builder.CreateLoad(Int32PointerTy, coupon);
            args.push_back(ldCoupon);
            args.push_back(ConstantInt::get(Int32Ty, KEY_SIZE, true));
            builder.CreateCall(NVMStoreAI32, args);  //insert before inst
            local_ind_int += KEY_SIZE;
        } else if (couponType->isPointerTy() &&
                    couponType->getPointerElementType()->isIntegerTy(8)) {
            args.push_back(ConstantInt::get(Int32Ty, local_ind_BYTE, true));
            LoadInst *ldCoupon = builder.CreateLoad(Int8PointerTy, coupon);
            args.push_back(ldCoupon);
            args.push_back(ConstantInt::get(Int32Ty, CAMELLIA_BLOCK_SIZE, true));
            builder.CreateCall(NVMStoreAI8, args);  //insert before inst
            local_ind_BYTE += CAMELLIA_BLOCK_SIZE;
        } else {
            errs() << *couponType << " is unsuppoted NVM-Store type!\n";
            std::exit(EXIT_FAILURE);
        }
    }
}


// insert NVMLoad <index, value> before inst
/// NOTE: we create a new basic block to insert NVMLoad operations
///        to avoid the situation where secHead is within some loop
void NVMem::inset_NVMLoad(Sections *secs, int sec_id,
                          IRBuilder<> builder, std::vector<AllocaInst *> coupons){
    /// Create a bb beween <preBB, currBB>
    /// NOTE: cannot remove terminator at first
    ///         e.g., there is only one inst in a bb
    BasicBlock *predBB = secs->getSecTail(sec_id - 1)->getParent();
    BasicBlock *currBB = secs->getSecHead(sec_id)->getParent();
    // create NVMLoadBB
    BasicBlock *NVMLoadBB = BasicBlock::Create(M.getContext(), "", F, currBB);
    // set predcessorBB of condBB
    BranchInst::Create(NVMLoadBB, predBB->getTerminator());
    predBB->getTerminator()->eraseFromParent();
    // update the tailInst of preSec to the new tail(preBB)
    secs->setSecTail(sec_id - 1, predBB->getTerminator());
    
    builder.SetInsertPoint(NVMLoadBB);
    //Insert the NVMLoad operations
    for (AllocaInst *coupon : coupons) {
        std::vector<Value *> args = std::vector<Value *>();
        Type *couponType = coupon->getAllocatedType();
        // for Array: change the value of array
        if (couponType->isArrayTy() &&
            couponType->getArrayElementType()->isIntegerTy(8)) {
            args.push_back(ConstantInt::get(Int32Ty, index_for_BYTE, true));
            std::vector<Value*> args_sep = std::vector<Value*>();
            args_sep.push_back(ConstantInt::get(Int32Ty, 0, true));
            args_sep.push_back(ConstantInt::get(Int32Ty, 0, true));
            Value *getelementptr = builder.CreateInBoundsGEP(couponType, coupon, args_sep);
            args.push_back(getelementptr);
            args.push_back(ConstantInt::get(Int32Ty, couponType->getArrayNumElements(), true));
            builder.CreateCall(NVMLoadAI8, args);
            updateIndex(BYTE, (int) couponType->getArrayNumElements());
        }  if (couponType->isArrayTy() &&
               couponType->getArrayElementType()->isIntegerTy(32)) {
            args.push_back(ConstantInt::get(Int32Ty, index_for_int32, true));
            std::vector<Value*> args_sep = std::vector<Value*>();
            args_sep.push_back(ConstantInt::get(Int32Ty, 0, true));
            args_sep.push_back(ConstantInt::get(Int32Ty, 0, true));
            Value *getelementptr = builder.CreateInBoundsGEP(couponType, coupon, args_sep);
            args.push_back(getelementptr);
            args.push_back(ConstantInt::get(Int32Ty, couponType->getArrayNumElements(), true));
            builder.CreateCall(NVMLoadAI32, args);
            updateIndex(Int32, (int) couponType->getArrayNumElements());
        } else if (couponType->isIntegerTy(32)) {
            // (1) NVMLoad the value
            args.push_back(ConstantInt::get(Int32Ty, index_for_int32, true));
            CallInst *call = builder.CreateCall(NVMLoadI32, args);
            // (2) Store the value to coupon
            builder.CreateStore(call, coupon);
            updateIndex(Int32, 1);
        } else if (couponType->isIntegerTy(8)) {
            args.push_back(ConstantInt::get(Int32Ty, index_for_BYTE, true));
            CallInst *call = builder.CreateCall(NVMLoadI8, args);
            // (2) Store the value to coupon
            builder.CreateStore(call, coupon);
            updateIndex(BYTE, 1);
        } else if (couponType->isPointerTy() &&
                 couponType->getPointerElementType()->isIntegerTy(32)) {
            args.push_back(ConstantInt::get(Int32Ty, index_for_int32, true));
            args.push_back(ConstantInt::get(Int32Ty, KEY_SIZE, true));
            CallInst *call = builder.CreateCall(NVMLoadPtrI32, args);
            builder.CreateStore(call, coupon);
            updateIndex(Int32, KEY_SIZE);
        } else if (couponType->isPointerTy() &&
                     couponType->getPointerElementType()->isIntegerTy(8)) {
            args.push_back(ConstantInt::get(Int32Ty, index_for_BYTE, true));
            args.push_back(ConstantInt::get(Int32Ty, CAMELLIA_BLOCK_SIZE, true));
            CallInst *call = builder.CreateCall(NVMLoadPtrI8, args);
            builder.CreateStore(call, coupon);
            updateIndex(BYTE, CAMELLIA_BLOCK_SIZE);
        } else {
            errs() << *couponType << " is unsuppoted NVM-Store type!\n";
            std::exit(EXIT_FAILURE);
        }
    }
    // set successorBB of NVMLoadBB and
    // update the headInst of section to the head(if.cond)
    builder.CreateBr(currBB);
    secs->setSecHead(sec_id, &*NVMLoadBB->begin());
}


// update the index of next NVMem Coupon
void NVMem::updateIndex(NVMUnitTy couponUnitTy, int couponSize) {
    if (couponUnitTy == Int32) {
        if (index_for_int32 + couponSize < MAX_NVM_INT32_NUM) {
            index_for_int32 += couponSize;
        } else {
            errs() << "NVM-IntArray Overflow!\n";
            std::exit(EXIT_FAILURE);
        }
    } else if (couponUnitTy == BYTE) {
        if (index_for_BYTE + couponSize < MAX_NVM_BYTE_NUM) {
            index_for_BYTE += couponSize;
        } else {
            errs() << "NVM-BYTEArray Overflow!\n";
            std::exit(EXIT_FAILURE);
        }
    } else {
        errs() << "NVM-Index Update Failed!\n";
        std::exit(EXIT_FAILURE);
    }
}



// insert NVMStoreFile before inst
void NVMem::insert_NVMStoreFile(Instruction *inst, IRBuilder<> builder) {
    std::vector<Value*> args;
    args.push_back(ConstantInt::get(Int32Ty, index_for_int32, true));
    args.push_back(ConstantInt::get(Int32Ty, index_for_BYTE, true));
    CallInst::Create(NVMStoreFile, args, "", /*insert before*/ inst);
}

// insert NVMLoadFile before inst
void NVMem::insert_NVMLoadFile(Sections *secs, int sec_id, IRBuilder<> builder) {
    /// Create a bb beween <preBB, currBB>
    /// NOTE: cannot remove terminator at first
    ///         e.g., there is only one inst in a bb
    BasicBlock *predBB = secs->getSecTail(sec_id - 1)->getParent();
    BasicBlock *currBB = secs->getSecHead(sec_id)->getParent();
    // create NVMLoadBB
    BasicBlock *NVMLoadFileBB = BasicBlock::Create(M.getContext(), "", F, currBB);
    // set predcessorBB of condBB
    BranchInst::Create(NVMLoadFileBB, predBB->getTerminator());
    predBB->getTerminator()->eraseFromParent();
    // update the tailInst of preSec to the new tail(preBB)
    secs->setSecTail(sec_id - 1, predBB->getTerminator());
    
    // insert loadFromFileCall
    builder.SetInsertPoint(NVMLoadFileBB);
    std::vector<Value*> args;
    builder.CreateCall(NVMLoadFile, args);
    
    // set successorBB of NVMLoadBB and
    // update the headInst of section to the head(if.cond)
    builder.CreateBr(currBB);
    secs->setSecHead(sec_id, &*NVMLoadFileBB->begin());
}


void NVMem::initNVMOperations(){
    Int32Ty = IntegerType::get(M.getContext(), 32);
    Int8Ty = IntegerType::get(M.getContext(), 8);
    Int8PointerTy = PointerType::getUnqual(Int8Ty);
    Int32PointerTy = PointerType::getUnqual(Int32Ty);
    // NVMStoreInt32 function
    std::vector<Type *> args_storeInt32 = std::vector<Type *>();
    args_storeInt32.push_back(Int32Ty);
    args_storeInt32.push_back(Int32Ty);
    FunctionType *SI32FT = FunctionType::get(Type::getVoidTy(M.getContext()), args_storeInt32, false);
    Constant *storeI32Func = M.getOrInsertFunction("storeInt32_NVmem", SI32FT);
    NVMStoreI32 = cast<Function>(storeI32Func);
    // NVMStoreInt8 function
    std::vector<Type *> args_storeInt8 = std::vector<Type *>();
    args_storeInt8.push_back(Int32Ty);
    args_storeInt8.push_back(Int8Ty);
    FunctionType *SI8FT = FunctionType::get(Type::getVoidTy(M.getContext()), args_storeInt8, false);
    Constant *storeI8Func = M.getOrInsertFunction("storeBYTE_NVmem", SI8FT);
    NVMStoreI8 = cast<Function>(storeI8Func);
    // NVMStoreArrayInt8 function
    std::vector<Type *> args_storeArrayInt8 = std::vector<Type *>();
    args_storeArrayInt8.push_back(Int32Ty);
    args_storeArrayInt8.push_back(Int8PointerTy);
    args_storeArrayInt8.push_back(Int32Ty);
    FunctionType *SAI8FT = FunctionType::get(Type::getVoidTy(M.getContext()), args_storeArrayInt8, false);
    Constant *storeAI8Func = M.getOrInsertFunction("storeBYTEArray_NVmem", SAI8FT);
    NVMStoreAI8 = cast<Function>(storeAI8Func);
    // NVMStoreArrayInt32 function
    std::vector<Type *> args_storeArrayInt32 = std::vector<Type *>();
    args_storeArrayInt32.push_back(Int32Ty);
    args_storeArrayInt32.push_back(Int32PointerTy);
    args_storeArrayInt32.push_back(Int32Ty);
    FunctionType *SAI32FT = FunctionType::get(Type::getVoidTy(M.getContext()), args_storeArrayInt32, false);
    Constant *storeAI32Func = M.getOrInsertFunction("storeInt32Array_NVmem", SAI32FT);
    NVMStoreAI32 = cast<Function>(storeAI32Func);
    // NVMLoadAI8 function
    Constant *loadAI8Func = M.getOrInsertFunction("loadBYTEArray_NVmem", SAI8FT);
    NVMLoadAI8 = cast<Function>(loadAI8Func);
    // NVMLoadAI32 function
    Constant *loadAI32Func = M.getOrInsertFunction("loadInt32Array_NVmem", SAI32FT);
    NVMLoadAI32 = cast<Function>(loadAI32Func);
    // NVMLoadPtrI8 function
    std::vector<Type *> args_NVMLoadPtr = std::vector<Type *>();
    args_NVMLoadPtr.push_back(Int32Ty);
    args_NVMLoadPtr.push_back(Int32Ty);
    FunctionType *LPI8FT = FunctionType::get(Type::getInt8PtrTy(M.getContext()), args_NVMLoadPtr, false);
    Constant *loadPtrI8Func = M.getOrInsertFunction("loadBYTEPointer_NVmem", LPI8FT);
    NVMLoadPtrI8 = cast<Function>(loadPtrI8Func);
    // NVMLoadPtrI32 function
    FunctionType *LPI32FT = FunctionType::get(Type::getInt32PtrTy(M.getContext()), args_NVMLoadPtr, false);
    Constant *loadPtrI32Func = M.getOrInsertFunction("loadInt32Pointer_NVmem", LPI32FT);
    NVMLoadPtrI32 = cast<Function>(loadPtrI32Func);
    // NVMLoadInt32 function
    std::vector<Type *> args_NVMLoad = std::vector<Type *>();
    args_NVMLoad.push_back(Int32Ty);
    FunctionType *LI32FT = FunctionType::get(Type::getInt32Ty(M.getContext()), args_NVMLoad, false);
    Constant *loadI32Func = M.getOrInsertFunction("loadInt32_NVmem", LI32FT);
    NVMLoadI32 = cast<Function>(loadI32Func);
    // NVMLoadInt8 function
    FunctionType *LI8FT = FunctionType::get(Type::getInt8Ty(M.getContext()), args_NVMLoad, false);
    Constant *loadI8Func = M.getOrInsertFunction("loadBYTE_NVmem", LI8FT);
    NVMLoadI8 = cast<Function>(loadI8Func);
    // NVMStoreFile
    std::vector<Type *> args_storeFile = std::vector<Type *>();
    args_storeFile.push_back(Int32Ty);
    args_storeFile.push_back(Int32Ty);
    FunctionType *STFILEFT = FunctionType::get(Type::getVoidTy(M.getContext()), args_storeFile, false);
    Constant *STFILEFunc = M.getOrInsertFunction("storeToFile", STFILEFT);
    NVMStoreFile = cast<Function>(STFILEFunc);
    // NVMLoadFile
    std::vector<Type *> args_loadFile = std::vector<Type *>();
    FunctionType *LDFILEFT = FunctionType::get(Type::getVoidTy(M.getContext()), args_loadFile, false);
    Constant *LDFILEFunc = M.getOrInsertFunction("loadFromFile", LDFILEFT);
    NVMLoadFile = cast<Function>(LDFILEFunc);
}

    

