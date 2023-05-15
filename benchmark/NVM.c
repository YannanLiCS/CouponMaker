#include <stdio.h>
#include <stdlib.h>

typedef unsigned char BYTE;
typedef unsigned long WORD;


#define MAX_SIZE 20

//#define DEBUG_TYPE "NVM"

static int NVMInt32[MAX_SIZE];
static BYTE NVMBYTE[MAX_SIZE];
static WORD NVMWORD[MAX_SIZE];

void storeInt32_NVmem(int i, int x) {
    NVMInt32[i] = x;
#ifdef DEBUG_TYPE
    printf("Stored to NVMem Successfully! NVMInt32[%d] = %d\n", i, x);
#endif
}


void storeBYTE_NVmem(int i, BYTE x) {
    NVMBYTE[i] = x;
#ifdef DEBUG_TYPE
    printf("Stored to NVMem Successfully! NVMBYTE[%d] = %d\n", i, (int)x);
#endif
}


void storeWORD_NVmem(int i, WORD x) {
    NVMWORD[i] = x;
#ifdef DEBUG_TYPE
    printf("Stored to NVMem Successfully! NVMWORD[%d] = %l\n", i, x);
#endif
}

void storeBYTEArray_NVmem(int start_index, BYTE x[], int len) {
    if ((long long)start_index + (long long)len > (long long)MAX_SIZE) {
        printf("NVMBYTE[] OVERFLOW!\n");
        return;
    }
    for (int j = 0; j < len; j ++) {
        NVMBYTE[j + start_index] = x[j];
    }
#ifdef DEBUG_TYPE
    printf("Successfully Store Arrray [%d x BYTE] to NVMBYTE[%d] ~ NVMBYTE[%d]\n",
    len, start_index, start_index + len - 1);
#endif
}



void storeInt32Array_NVmem(int start_index, int x[], int len) {
    if ((long long)start_index + (long long)len > (long long)MAX_SIZE) {
        printf("NVMBInt32[] OVERFLOW!\n");
        return;
    }
    for (int j = 0; j < len; j++) {
        NVMInt32[j + start_index] = x[j];
    }
#ifdef DEBUG_TYPE
    printf("Successfully Store Arrray [%d x Int32] to NVMInt32[%d] ~ NVMInt32[%d]\n",
    len, start_index, start_index + len - 1);
#endif
}

void storeWORDArray_NVmem(int start_index, WORD x[], int len) {
    if ((long long)start_index + (long long)len > (long long)MAX_SIZE) {
        printf("NVMWORD[] OVERFLOW!\n");
        return;
    }
    for (int j = 0; j < len; j ++) {
        NVMWORD[j + start_index] = x[j];
    }
#ifdef DEBUG_TYPE
    printf("Successfully Store Arrray [%d x WORD] to NVMWORD[%d] ~ NVMWORD[%d]\n",
    len, start_index, start_index + len - 1);
#endif
}

int loadInt32_NVmem(int i){
#ifdef DEBUG_TYPE
    printf("Load from NVMem Successfully! NVMInt32[%d] = %d\n", i, NVMInt32[i]);
#endif
    return NVMInt32[i];
}

WORD loadWORD_NVmem(int i){
#ifdef DEBUG_TYPE
    printf("Load from NVMem Successfully! NVMWORD[%d] = %l\n", i, NVMWORD[i]);
#endif
    return NVMWORD[i];
}


BYTE loadBYTE_NVmem(int i){
#ifdef DEBUG_TYPE
    printf("Load from NVMem Successfully! NVMBYTE[%d] = %d\n", i, (int)NVMBYTE[i]);
#endif
    return NVMBYTE[i];
}


void loadBYTEArray_NVmem(int start_index, BYTE x[], int len){
    for (int j = 0; j < len; j++){
        x[j] = NVMBYTE[start_index + j];
    }
#ifdef DEBUG_TYPE
    printf("Loaded Arrray [%d x BYTE] from NVMBYTE[%d] ~ NVMBYTE[%d]\n",
           len, start_index, start_index + len - 1);
#endif
}


void loadInt32Array_NVmem(int start_index, int x[], int len){
    for (int j = 0; j < len; j++){
        x[j] = NVMInt32[start_index + j];
    }
#ifdef DEBUG_TYPE
    printf("Loaded Arrray [%d x Int32] from NVMInt32[%d] ~ NVMInt32[%d]\n",
           len, start_index, start_index + len - 1);
#endif
}

void loadWORDArray_NVmem(int start_index, WORD x[], int len){
    for (int j = 0; j < len; j++){
        x[j] = NVMWORD[start_index + j];
    }
#ifdef DEBUG_TYPE
    printf("Loaded Arrray [%d x WORD] from NVMWORD[%d] ~ NVMWORD[%d]\n",
           len, start_index, start_index + len - 1);
#endif
}


