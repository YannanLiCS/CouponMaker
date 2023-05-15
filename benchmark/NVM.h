#include <stdio.h>
#include <stdlib.h>

#ifndef NVM_H
#define NVM_H

typedef unsigned char BYTE;
typedef unsigned long WORD;


void storeInt32_NVmem(int i, int x); 

void storeBYTE_NVmem(int i, BYTE x);

void storeWORD_NVmem(int i, WORD x);

void storeBYTEArray_NVmem(int start_index, BYTE x[], int len); 

void storeInt32Array_NVmem(int start_index, int x[], int len);

void storeWORDArray_NVmem(int start_index, WORD x[], int len);

int loadInt32_NVmem(int i);

BYTE loadBYTE_NVmem(int i);

WORD loadWORD_NVmem(int i);

void loadBYTEArray_NVmem(int start_index, BYTE x[], int len);

void loadInt32Array_NVmem(int start_index, int x[], int len);

void loadWORDArray_NVmem(int start_index, WORD x[], int len);

#endif


