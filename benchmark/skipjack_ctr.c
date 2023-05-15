#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stddef.h>



typedef unsigned char    BYTE;
typedef unsigned long WORD;       // Should be 32-bit = 4 bytes, change to "long" for 16-bit machines

#define SKIPJACK_BLOCK_SIZE 8



/*********************** FUNCTION DEFINITIONS ***********************/
void mm_memset(BYTE arr[], BYTE val, long len) {
    long i;
    for (i = 0; i < len; i++) {
           arr[i] = val;
    }
}

void mm_memcpy(BYTE arr1[], BYTE arr2[], long len) {
    long i;
    for (i = 0; i < len; i++) {
           arr1[i] = arr2[i];
    }
}


// ----- skipjack

/**
 * The F-table BYTE permutation (see description of the G-box permutation)
 */
static const BYTE fTable[256] = {
    0xa3,0xd7,0x09,0x83,0xf8,0x48,0xf6,0xf4,0xb3,0x21,0x15,0x78,0x99,0xb1,0xaf,0xf9,
    0xe7,0x2d,0x4d,0x8a,0xce,0x4c,0xca,0x2e,0x52,0x95,0xd9,0x1e,0x4e,0x38,0x44,0x28,
    0x0a,0xdf,0x02,0xa0,0x17,0xf1,0x60,0x68,0x12,0xb7,0x7a,0xc3,0xe9,0xfa,0x3d,0x53,
    0x96,0x84,0x6b,0xba,0xf2,0x63,0x9a,0x19,0x7c,0xae,0xe5,0xf5,0xf7,0x16,0x6a,0xa2,
    0x39,0xb6,0x7b,0x0f,0xc1,0x93,0x81,0x1b,0xee,0xb4,0x1a,0xea,0xd0,0x91,0x2f,0xb8,
    0x55,0xb9,0xda,0x85,0x3f,0x41,0xbf,0xe0,0x5a,0x58,0x80,0x5f,0x66,0x0b,0xd8,0x90,
    0x35,0xd5,0xc0,0xa7,0x33,0x06,0x65,0x69,0x45,0x00,0x94,0x56,0x6d,0x98,0x9b,0x76,
    0x97,0xfc,0xb2,0xc2,0xb0,0xfe,0xdb,0x20,0xe1,0xeb,0xd6,0xe4,0xdd,0x47,0x4a,0x1d,
    0x42,0xed,0x9e,0x6e,0x49,0x3c,0xcd,0x43,0x27,0xd2,0x07,0xd4,0xde,0xc7,0x67,0x18,
    0x89,0xcb,0x30,0x1f,0x8d,0xc6,0x8f,0xaa,0xc8,0x74,0xdc,0xc9,0x5d,0x5c,0x31,0xa4,
    0x70,0x88,0x61,0x2c,0x9f,0x0d,0x2b,0x87,0x50,0x82,0x54,0x64,0x26,0x7d,0x03,0x40,
    0x34,0x4b,0x1c,0x73,0xd1,0xc4,0xfd,0x3b,0xcc,0xfb,0x7f,0xab,0xe6,0x3e,0x5b,0xa5,
    0xad,0x04,0x23,0x9c,0x14,0x51,0x22,0xf0,0x29,0x79,0x71,0x7e,0xff,0x8c,0x0e,0xe2,
    0x0c,0xef,0xbc,0x72,0x75,0x6f,0x37,0xa1,0xec,0xd3,0x8e,0x62,0x8b,0x86,0x10,0xe8,
    0x08,0x77,0x11,0xbe,0x92,0x4f,0x24,0xc5,0x32,0x36,0x9d,0xcf,0xf3,0xa6,0xbb,0xac,
    0x5e,0x6c,0xa9,0x13,0x57,0x25,0xb5,0xe3,0xbd,0xa8,0x3a,0x01,0x05,0x59,0x2a,0x46
};

/**
 * The key-dependent permutation G on V^16 is a four-round Feistel network.
 * The round function is a fixed BYTE-substitution table (permutation on V^8),
 * the F-table.  Each round of G incorporates a single BYTE from the key.
 */
#define g(tab, w, i, j, k, l) \
{ \
    w ^= (WORD)tab[i][w & 0xff] << 8; \
    w ^= (WORD)tab[j][w >>   8]; \
    w ^= (WORD)tab[k][w & 0xff] << 8; \
    w ^= (WORD)tab[l][w >>   8]; \
}

#define g0(tab, w) g(tab, w, 0, 1, 2, 3)
#define g1(tab, w) g(tab, w, 4, 5, 6, 7)
#define g2(tab, w) g(tab, w, 8, 9, 0, 1)
#define g3(tab, w) g(tab, w, 2, 3, 4, 5)
#define g4(tab, w) g(tab, w, 6, 7, 8, 9)

/**
 * The inverse of the G permutation.
 */
#define h(tab, w, i, j, k, l) \
{ \
    w ^= (WORD)tab[l][w >>   8]; \
    w ^= (WORD)tab[k][w & 0xff] << 8; \
    w ^= (WORD)tab[j][w >>   8]; \
    w ^= (WORD)tab[i][w & 0xff] << 8; \
}

#define h0(tab, w) h(tab, w, 0, 1, 2, 3)
#define h1(tab, w) h(tab, w, 4, 5, 6, 7)
#define h2(tab, w) h(tab, w, 8, 9, 0, 1)
#define h3(tab, w) h(tab, w, 2, 3, 4, 5)
#define h4(tab, w) h(tab, w, 6, 7, 8, 9)

/**
 * Preprocess a user key into a table to save an XOR at each F-table access.
 */
void makeKey(BYTE key[10], BYTE tab[10][256]) {
    /* tab[i][c] = fTable[c ^ key[i]] */
    long i;
    for (i = 0; i < 10; i++) {
        BYTE *t = tab[i], k = key[i];
        long c;
        for (c = 0; c < 256; c++) {
            t[c] = fTable[c ^ k];
        }
    }
}

/**
 * Encrypt a single block of data.
 */
void mm_encrypt(BYTE tab[10][256], BYTE in[8], BYTE out[]) {
    WORD w1, w2, w3, w4;
    

    w1 = (in[0] << 8) + in[1];
    w2 = (in[2] << 8) + in[3];
    w3 = (in[4] << 8) + in[5];
    w4 = (in[6] << 8) + in[7];

    /* stepping rule A: */
    g0(tab, w1); w4 ^= w1 ^ 1;
    g1(tab, w4); w3 ^= w4 ^ 2;
    g2(tab, w3); w2 ^= w3 ^ 3;
    g3(tab, w2); w1 ^= w2 ^ 4;
    g4(tab, w1); w4 ^= w1 ^ 5;
    g0(tab, w4); w3 ^= w4 ^ 6;
    g1(tab, w3); w2 ^= w3 ^ 7;
    g2(tab, w2); w1 ^= w2 ^ 8;

    /* stepping rule B: */
    w2 ^= w1 ^  9; g3(tab, w1);
    w1 ^= w4 ^ 10; g4(tab, w4);
    w4 ^= w3 ^ 11; g0(tab, w3);
    w3 ^= w2 ^ 12; g1(tab, w2);
    w2 ^= w1 ^ 13; g2(tab, w1);
    w1 ^= w4 ^ 14; g3(tab, w4);
    w4 ^= w3 ^ 15; g4(tab, w3);
    w3 ^= w2 ^ 16; g0(tab, w2);

    /* stepping rule A: */
    g1(tab, w1); w4 ^= w1 ^ 17;
    g2(tab, w4); w3 ^= w4 ^ 18;
    g3(tab, w3); w2 ^= w3 ^ 19;
    g4(tab, w2); w1 ^= w2 ^ 20;
    g0(tab, w1); w4 ^= w1 ^ 21;
    g1(tab, w4); w3 ^= w4 ^ 22;
    g2(tab, w3); w2 ^= w3 ^ 23;
    g3(tab, w2); w1 ^= w2 ^ 24;

    /* stepping rule B: */
    w2 ^= w1 ^ 25; g4(tab, w1);
    w1 ^= w4 ^ 26; g0(tab, w4);
    w4 ^= w3 ^ 27; g1(tab, w3);
    w3 ^= w2 ^ 28; g2(tab, w2);
    w2 ^= w1 ^ 29; g3(tab, w1);
    w1 ^= w4 ^ 30; g4(tab, w4);
    w4 ^= w3 ^ 31; g0(tab, w3);
    w3 ^= w2 ^ 32; g1(tab, w2);

    out[0] = (BYTE)(w1 >> 8); out[1] = (BYTE)w1;
    out[2] = (BYTE)(w2 >> 8); out[3] = (BYTE)w2;
    out[4] = (BYTE)(w3 >> 8); out[5] = (BYTE)w3;
    out[6] = (BYTE)(w4 >> 8); out[7] = (BYTE)w4;
    
}

/**
 * Decrypt a single block of data.
 */
void decrypt(BYTE tab[10][256], BYTE in[8], BYTE out[8]) {
    WORD w1, w2, w3, w4;

    w1 = (in[0] << 8) + in[1];
    w2 = (in[2] << 8) + in[3];
    w3 = (in[4] << 8) + in[5];
    w4 = (in[6] << 8) + in[7];

    /* stepping rule A: */
    h1(tab, w2); w3 ^= w2 ^ 32;
    h0(tab, w3); w4 ^= w3 ^ 31;
    h4(tab, w4); w1 ^= w4 ^ 30;
    h3(tab, w1); w2 ^= w1 ^ 29;
    h2(tab, w2); w3 ^= w2 ^ 28;
    h1(tab, w3); w4 ^= w3 ^ 27;
    h0(tab, w4); w1 ^= w4 ^ 26;
    h4(tab, w1); w2 ^= w1 ^ 25;

    /* stepping rule B: */
    w1 ^= w2 ^ 24; h3(tab, w2);
    w2 ^= w3 ^ 23; h2(tab, w3);
    w3 ^= w4 ^ 22; h1(tab, w4);
    w4 ^= w1 ^ 21; h0(tab, w1);
    w1 ^= w2 ^ 20; h4(tab, w2);
    w2 ^= w3 ^ 19; h3(tab, w3);
    w3 ^= w4 ^ 18; h2(tab, w4);
    w4 ^= w1 ^ 17; h1(tab, w1);

    /* stepping rule A: */
    h0(tab, w2); w3 ^= w2 ^ 16;
    h4(tab, w3); w4 ^= w3 ^ 15;
    h3(tab, w4); w1 ^= w4 ^ 14;
    h2(tab, w1); w2 ^= w1 ^ 13;
    h1(tab, w2); w3 ^= w2 ^ 12;
    h0(tab, w3); w4 ^= w3 ^ 11;
    h4(tab, w4); w1 ^= w4 ^ 10;
    h3(tab, w1); w2 ^= w1 ^  9;

    /* stepping rule B: */
    w1 ^= w2 ^ 8; h2(tab, w2);
    w2 ^= w3 ^ 7; h1(tab, w3);
    w3 ^= w4 ^ 6; h0(tab, w4);
    w4 ^= w1 ^ 5; h4(tab, w1);
    w1 ^= w2 ^ 4; h3(tab, w2);
    w2 ^= w3 ^ 3; h2(tab, w3);
    w3 ^= w4 ^ 2; h1(tab, w4);
    w4 ^= w1 ^ 1; h0(tab, w1);

    out[0] = (BYTE)(w1 >> 8); out[1] = (BYTE)w1;
    out[2] = (BYTE)(w2 >> 8); out[3] = (BYTE)w2;
    out[4] = (BYTE)(w3 >> 8); out[5] = (BYTE)w3;
    out[6] = (BYTE)(w4 >> 8); out[7] = (BYTE)w4;

}


//-------- OFB --------

BYTE tab[10][256] =
    {
        0xa3, 0xd7, 0x9, 0x83, 0xf8, 0x48, 0xf6, 0xf4, 0xb3, 0x21, 0x15, 0x78, 0x99, 0xb1, 0xaf, 0xf9,
        0xe7, 0x2d, 0x4d, 0x8a, 0xce, 0x4c, 0xca, 0x2e, 0x52, 0x95, 0xd9, 0x1e, 0x4e, 0x38, 0x44, 0x28,
        0xa, 0xdf, 0x2, 0xa0, 0x17, 0xf1, 0x60, 0x68, 0x12, 0xb7, 0x7a, 0xc3, 0xe9, 0xfa, 0x3d, 0x53,
        0x96, 0x84, 0x6b, 0xba, 0xf2, 0x63, 0x9a, 0x19, 0x7c, 0xae, 0xe5, 0xf5, 0xf7, 0x16, 0x6a, 0xa2,
        0x39, 0xb6, 0x7b, 0xf, 0xc1, 0x93, 0x81, 0x1b, 0xee, 0xb4, 0x1a, 0xea, 0xd0, 0x91, 0x2f, 0xb8,
        0x55, 0xb9, 0xda, 0x85, 0x3f, 0x41, 0xbf, 0xe0, 0x5a, 0x58, 0x80, 0x5f, 0x66, 0xb, 0xd8, 0x90,
        0x35, 0xd5, 0xc0, 0xa7, 0x33, 0x6, 0x65, 0x69, 0x45, 0x0, 0x94, 0x56, 0x6d, 0x98, 0x9b, 0x76,
        0x97, 0xfc, 0xb2, 0xc2, 0xb0, 0xfe, 0xdb, 0x20, 0xe1, 0xeb, 0xd6, 0xe4, 0xdd, 0x47, 0x4a, 0x1d,
        0x42, 0xed, 0x9e, 0x6e, 0x49, 0x3c, 0xcd, 0x43, 0x27, 0xd2, 0x7, 0xd4, 0xde, 0xc7, 0x67, 0x18,
        0x89, 0xcb, 0x30, 0x1f, 0x8d, 0xc6, 0x8f, 0xaa, 0xc8, 0x74, 0xdc, 0xc9, 0x5d, 0x5c, 0x31, 0xa4,
        0x70, 0x88, 0x61, 0x2c, 0x9f, 0xd, 0x2b, 0x87, 0x50, 0x82, 0x54, 0x64, 0x26, 0x7d, 0x3, 0x40,
        0x34, 0x4b, 0x1c, 0x73, 0xd1, 0xc4, 0xfd, 0x3b, 0xcc, 0xfb, 0x7f, 0xab, 0xe6, 0x3e, 0x5b, 0xa5,
        0xad, 0x4, 0x23, 0x9c, 0x14, 0x51, 0x22, 0xf0, 0x29, 0x79, 0x71, 0x7e, 0xff, 0x8c, 0xe, 0xe2,
        0xc, 0xef, 0xbc, 0x72, 0x75, 0x6f, 0x37, 0xa1, 0xec, 0xd3, 0x8e, 0x62, 0x8b, 0x86, 0x10, 0xe8,
        0x8, 0x77, 0x11, 0xbe, 0x92, 0x4f, 0x24, 0xc5, 0x32, 0x36, 0x9d, 0xcf, 0xf3, 0xa6, 0xbb, 0xac,
        0x5e, 0x6c, 0xa9, 0x13, 0x57, 0x25, 0xb5, 0xe3, 0xbd, 0xa8, 0x3a, 0x1, 0x5, 0x59, 0x2a, 0x46,
        0x74, 0xc8, 0xc9, 0xdc, 0x5c, 0x5d, 0xa4, 0x31, 0xcb, 0x89, 0x1f, 0x30, 0xc6, 0x8d, 0xaa, 0x8f,
        0xd2, 0x27, 0xd4, 0x7, 0xc7, 0xde, 0x18, 0x67, 0xed, 0x42, 0x6e, 0x9e, 0x3c, 0x49, 0x43, 0xcd,
        0xfb, 0xcc, 0xab, 0x7f, 0x3e, 0xe6, 0xa5, 0x5b, 0x4b, 0x34, 0x73, 0x1c, 0xc4, 0xd1, 0x3b, 0xfd,
        0x82, 0x50, 0x64, 0x54, 0x7d, 0x26, 0x40, 0x3, 0x88, 0x70, 0x2c, 0x61, 0xd, 0x9f, 0x87, 0x2b,
        0xd3, 0xec, 0x62, 0x8e, 0x86, 0x8b, 0xe8, 0x10, 0xef, 0xc, 0x72, 0xbc, 0x6f, 0x75, 0xa1, 0x37,
        0x79, 0x29, 0x7e, 0x71, 0x8c, 0xff, 0xe2, 0xe, 0x4, 0xad, 0x9c, 0x23, 0x51, 0x14, 0xf0, 0x22,
        0xa8, 0xbd, 0x1, 0x3a, 0x59, 0x5, 0x46, 0x2a, 0x6c, 0x5e, 0x13, 0xa9, 0x25, 0x57, 0xe3, 0xb5,
        0x36, 0x32, 0xcf, 0x9d, 0xa6, 0xf3, 0xac, 0xbb, 0x77, 0x8, 0xbe, 0x11, 0x4f, 0x92, 0xc5, 0x24,
        0x95, 0x52, 0x1e, 0xd9, 0x38, 0x4e, 0x28, 0x44, 0x2d, 0xe7, 0x8a, 0x4d, 0x4c, 0xce, 0x2e, 0xca,
        0x21, 0xb3, 0x78, 0x15, 0xb1, 0x99, 0xf9, 0xaf, 0xd7, 0xa3, 0x83, 0x9, 0x48, 0xf8, 0xf4, 0xf6,
        0xae, 0x7c, 0xf5, 0xe5, 0x16, 0xf7, 0xa2, 0x6a, 0x84, 0x96, 0xba, 0x6b, 0x63, 0xf2, 0x19, 0x9a,
        0xb7, 0x12, 0xc3, 0x7a, 0xfa, 0xe9, 0x53, 0x3d, 0xdf, 0xa, 0xa0, 0x2, 0xf1, 0x17, 0x68, 0x60,
        0x58, 0x5a, 0x5f, 0x80, 0xb, 0x66, 0x90, 0xd8, 0xb9, 0x55, 0x85, 0xda, 0x41, 0x3f, 0xe0, 0xbf,
        0xb4, 0xee, 0xea, 0x1a, 0x91, 0xd0, 0xb8, 0x2f, 0xb6, 0x39, 0xf, 0x7b, 0x93, 0xc1, 0x1b, 0x81,
        0xeb, 0xe1, 0xe4, 0xd6, 0x47, 0xdd, 0x1d, 0x4a, 0xfc, 0x97, 0xc2, 0xb2, 0xfe, 0xb0, 0x20, 0xdb,
        0x0, 0x45, 0x56, 0x94, 0x98, 0x6d, 0x76, 0x9b, 0xd5, 0x35, 0xa7, 0xc0, 0x6, 0x33, 0x69, 0x65,
        0x27, 0xd2, 0x7, 0xd4, 0xde, 0xc7, 0x67, 0x18, 0x42, 0xed, 0x9e, 0x6e, 0x49, 0x3c, 0xcd, 0x43,
        0xc8, 0x74, 0xdc, 0xc9, 0x5d, 0x5c, 0x31, 0xa4, 0x89, 0xcb, 0x30, 0x1f, 0x8d, 0xc6, 0x8f, 0xaa,
        0x50, 0x82, 0x54, 0x64, 0x26, 0x7d, 0x3, 0x40, 0x70, 0x88, 0x61, 0x2c, 0x9f, 0xd, 0x2b, 0x87,
        0xcc, 0xfb, 0x7f, 0xab, 0xe6, 0x3e, 0x5b, 0xa5, 0x34, 0x4b, 0x1c, 0x73, 0xd1, 0xc4, 0xfd, 0x3b,
        0x29, 0x79, 0x71, 0x7e, 0xff, 0x8c, 0xe, 0xe2, 0xad, 0x4, 0x23, 0x9c, 0x14, 0x51, 0x22, 0xf0,
        0xec, 0xd3, 0x8e, 0x62, 0x8b, 0x86, 0x10, 0xe8, 0xc, 0xef, 0xbc, 0x72, 0x75, 0x6f, 0x37, 0xa1,
        0x32, 0x36, 0x9d, 0xcf, 0xf3, 0xa6, 0xbb, 0xac, 0x8, 0x77, 0x11, 0xbe, 0x92, 0x4f, 0x24, 0xc5,
        0xbd, 0xa8, 0x3a, 0x1, 0x5, 0x59, 0x2a, 0x46, 0x5e, 0x6c, 0xa9, 0x13, 0x57, 0x25, 0xb5, 0xe3,
        0xb3, 0x21, 0x15, 0x78, 0x99, 0xb1, 0xaf, 0xf9, 0xa3, 0xd7, 0x9, 0x83, 0xf8, 0x48, 0xf6, 0xf4,
        0x52, 0x95, 0xd9, 0x1e, 0x4e, 0x38, 0x44, 0x28, 0xe7, 0x2d, 0x4d, 0x8a, 0xce, 0x4c, 0xca, 0x2e,
        0x12, 0xb7, 0x7a, 0xc3, 0xe9, 0xfa, 0x3d, 0x53, 0xa, 0xdf, 0x2, 0xa0, 0x17, 0xf1, 0x60, 0x68,
        0x7c, 0xae, 0xe5, 0xf5, 0xf7, 0x16, 0x6a, 0xa2, 0x96, 0x84, 0x6b, 0xba, 0xf2, 0x63, 0x9a, 0x19,
        0xee, 0xb4, 0x1a, 0xea, 0xd0, 0x91, 0x2f, 0xb8, 0x39, 0xb6, 0x7b, 0xf, 0xc1, 0x93, 0x81, 0x1b,
        0x5a, 0x58, 0x80, 0x5f, 0x66, 0xb, 0xd8, 0x90, 0x55, 0xb9, 0xda, 0x85, 0x3f, 0x41, 0xbf, 0xe0,
        0x45, 0x0, 0x94, 0x56, 0x6d, 0x98, 0x9b, 0x76, 0x35, 0xd5, 0xc0, 0xa7, 0x33, 0x6, 0x65, 0x69,
        0xe1, 0xeb, 0xd6, 0xe4, 0xdd, 0x47, 0x4a, 0x1d, 0x97, 0xfc, 0xb2, 0xc2, 0xb0, 0xfe, 0xdb, 0x20,
        0x20, 0xdb, 0xfe, 0xb0, 0xc2, 0xb2, 0xfc, 0x97, 0x1d, 0x4a, 0x47, 0xdd, 0xe4, 0xd6, 0xeb, 0xe1,
        0x69, 0x65, 0x6, 0x33, 0xa7, 0xc0, 0xd5, 0x35, 0x76, 0x9b, 0x98, 0x6d, 0x56, 0x94, 0x0, 0x45,
        0xe0, 0xbf, 0x41, 0x3f, 0x85, 0xda, 0xb9, 0x55, 0x90, 0xd8, 0xb, 0x66, 0x5f, 0x80, 0x58, 0x5a,
        0x1b, 0x81, 0x93, 0xc1, 0xf, 0x7b, 0xb6, 0x39, 0xb8, 0x2f, 0x91, 0xd0, 0xea, 0x1a, 0xb4, 0xee,
        0x19, 0x9a, 0x63, 0xf2, 0xba, 0x6b, 0x84, 0x96, 0xa2, 0x6a, 0x16, 0xf7, 0xf5, 0xe5, 0xae, 0x7c,
        0x68, 0x60, 0xf1, 0x17, 0xa0, 0x2, 0xdf, 0xa, 0x53, 0x3d, 0xfa, 0xe9, 0xc3, 0x7a, 0xb7, 0x12,
        0x2e, 0xca, 0x4c, 0xce, 0x8a, 0x4d, 0x2d, 0xe7, 0x28, 0x44, 0x38, 0x4e, 0x1e, 0xd9, 0x95, 0x52,
        0xf4, 0xf6, 0x48, 0xf8, 0x83, 0x9, 0xd7, 0xa3, 0xf9, 0xaf, 0xb1, 0x99, 0x78, 0x15, 0x21, 0xb3,
        0xe3, 0xb5, 0x25, 0x57, 0x13, 0xa9, 0x6c, 0x5e, 0x46, 0x2a, 0x59, 0x5, 0x1, 0x3a, 0xa8, 0xbd,
        0xc5, 0x24, 0x4f, 0x92, 0xbe, 0x11, 0x77, 0x8, 0xac, 0xbb, 0xa6, 0xf3, 0xcf, 0x9d, 0x36, 0x32,
        0xa1, 0x37, 0x6f, 0x75, 0x72, 0xbc, 0xef, 0xc, 0xe8, 0x10, 0x86, 0x8b, 0x62, 0x8e, 0xd3, 0xec,
        0xf0, 0x22, 0x51, 0x14, 0x9c, 0x23, 0x4, 0xad, 0xe2, 0xe, 0x8c, 0xff, 0x7e, 0x71, 0x79, 0x29,
        0x3b, 0xfd, 0xc4, 0xd1, 0x73, 0x1c, 0x4b, 0x34, 0xa5, 0x5b, 0x3e, 0xe6, 0xab, 0x7f, 0xfb, 0xcc,
        0x87, 0x2b, 0xd, 0x9f, 0x2c, 0x61, 0x88, 0x70, 0x40, 0x3, 0x7d, 0x26, 0x64, 0x54, 0x82, 0x50,
        0xaa, 0x8f, 0xc6, 0x8d, 0x1f, 0x30, 0xcb, 0x89, 0xa4, 0x31, 0x5c, 0x5d, 0xc9, 0xdc, 0x74, 0xc8,
        0x43, 0xcd, 0x3c, 0x49, 0x6e, 0x9e, 0xed, 0x42, 0x18, 0x67, 0xc7, 0xde, 0xd4, 0x7, 0xd2, 0x27,
        0x65, 0x69, 0x33, 0x6, 0xc0, 0xa7, 0x35, 0xd5, 0x9b, 0x76, 0x6d, 0x98, 0x94, 0x56, 0x45, 0x0,
        0xdb, 0x20, 0xb0, 0xfe, 0xb2, 0xc2, 0x97, 0xfc, 0x4a, 0x1d, 0xdd, 0x47, 0xd6, 0xe4, 0xe1, 0xeb,
        0x81, 0x1b, 0xc1, 0x93, 0x7b, 0xf, 0x39, 0xb6, 0x2f, 0xb8, 0xd0, 0x91, 0x1a, 0xea, 0xee, 0xb4,
        0xbf, 0xe0, 0x3f, 0x41, 0xda, 0x85, 0x55, 0xb9, 0xd8, 0x90, 0x66, 0xb, 0x80, 0x5f, 0x5a, 0x58,
        0x60, 0x68, 0x17, 0xf1, 0x2, 0xa0, 0xa, 0xdf, 0x3d, 0x53, 0xe9, 0xfa, 0x7a, 0xc3, 0x12, 0xb7,
        0x9a, 0x19, 0xf2, 0x63, 0x6b, 0xba, 0x96, 0x84, 0x6a, 0xa2, 0xf7, 0x16, 0xe5, 0xf5, 0x7c, 0xae,
        0xf6, 0xf4, 0xf8, 0x48, 0x9, 0x83, 0xa3, 0xd7, 0xaf, 0xf9, 0x99, 0xb1, 0x15, 0x78, 0xb3, 0x21,
        0xca, 0x2e, 0xce, 0x4c, 0x4d, 0x8a, 0xe7, 0x2d, 0x44, 0x28, 0x4e, 0x38, 0xd9, 0x1e, 0x52, 0x95,
        0x24, 0xc5, 0x92, 0x4f, 0x11, 0xbe, 0x8, 0x77, 0xbb, 0xac, 0xf3, 0xa6, 0x9d, 0xcf, 0x32, 0x36,
        0xb5, 0xe3, 0x57, 0x25, 0xa9, 0x13, 0x5e, 0x6c, 0x2a, 0x46, 0x5, 0x59, 0x3a, 0x1, 0xbd, 0xa8,
        0x22, 0xf0, 0x14, 0x51, 0x23, 0x9c, 0xad, 0x4, 0xe, 0xe2, 0xff, 0x8c, 0x71, 0x7e, 0x29, 0x79,
        0x37, 0xa1, 0x75, 0x6f, 0xbc, 0x72, 0xc, 0xef, 0x10, 0xe8, 0x8b, 0x86, 0x8e, 0x62, 0xec, 0xd3,
        0x2b, 0x87, 0x9f, 0xd, 0x61, 0x2c, 0x70, 0x88, 0x3, 0x40, 0x26, 0x7d, 0x54, 0x64, 0x50, 0x82,
        0xfd, 0x3b, 0xd1, 0xc4, 0x1c, 0x73, 0x34, 0x4b, 0x5b, 0xa5, 0xe6, 0x3e, 0x7f, 0xab, 0xcc, 0xfb,
        0xcd, 0x43, 0x49, 0x3c, 0x9e, 0x6e, 0x42, 0xed, 0x67, 0x18, 0xde, 0xc7, 0x7, 0xd4, 0x27, 0xd2,
        0x8f, 0xaa, 0x8d, 0xc6, 0x30, 0x1f, 0x89, 0xcb, 0x31, 0xa4, 0x5d, 0x5c, 0xdc, 0xc9, 0xc8, 0x74,
        0x41, 0x3f, 0xe0, 0xbf, 0xb9, 0x55, 0x85, 0xda, 0xb, 0x66, 0x90, 0xd8, 0x58, 0x5a, 0x5f, 0x80,
        0x93, 0xc1, 0x1b, 0x81, 0xb6, 0x39, 0xf, 0x7b, 0x91, 0xd0, 0xb8, 0x2f, 0xb4, 0xee, 0xea, 0x1a,
        0xfe, 0xb0, 0x20, 0xdb, 0xfc, 0x97, 0xc2, 0xb2, 0x47, 0xdd, 0x1d, 0x4a, 0xeb, 0xe1, 0xe4, 0xd6,
        0x6, 0x33, 0x69, 0x65, 0xd5, 0x35, 0xa7, 0xc0, 0x98, 0x6d, 0x76, 0x9b, 0x0, 0x45, 0x56, 0x94,
        0x4c, 0xce, 0x2e, 0xca, 0x2d, 0xe7, 0x8a, 0x4d, 0x38, 0x4e, 0x28, 0x44, 0x95, 0x52, 0x1e, 0xd9,
        0x48, 0xf8, 0xf4, 0xf6, 0xd7, 0xa3, 0x83, 0x9, 0xb1, 0x99, 0xf9, 0xaf, 0x21, 0xb3, 0x78, 0x15,
        0x63, 0xf2, 0x19, 0x9a, 0x84, 0x96, 0xba, 0x6b, 0x16, 0xf7, 0xa2, 0x6a, 0xae, 0x7c, 0xf5, 0xe5,
        0xf1, 0x17, 0x68, 0x60, 0xdf, 0xa, 0xa0, 0x2, 0xfa, 0xe9, 0x53, 0x3d, 0xb7, 0x12, 0xc3, 0x7a,
        0x6f, 0x75, 0xa1, 0x37, 0xef, 0xc, 0x72, 0xbc, 0x86, 0x8b, 0xe8, 0x10, 0xd3, 0xec, 0x62, 0x8e,
        0x51, 0x14, 0xf0, 0x22, 0x4, 0xad, 0x9c, 0x23, 0x8c, 0xff, 0xe2, 0xe, 0x79, 0x29, 0x7e, 0x71,
        0x25, 0x57, 0xe3, 0xb5, 0x6c, 0x5e, 0x13, 0xa9, 0x59, 0x5, 0x46, 0x2a, 0xa8, 0xbd, 0x1, 0x3a,
        0x4f, 0x92, 0xc5, 0x24, 0x77, 0x8, 0xbe, 0x11, 0xa6, 0xf3, 0xac, 0xbb, 0x36, 0x32, 0xcf, 0x9d,
        0xc6, 0x8d, 0xaa, 0x8f, 0xcb, 0x89, 0x1f, 0x30, 0x5c, 0x5d, 0xa4, 0x31, 0x74, 0xc8, 0xc9, 0xdc,
        0x3c, 0x49, 0x43, 0xcd, 0xed, 0x42, 0x6e, 0x9e, 0xc7, 0xde, 0x18, 0x67, 0xd2, 0x27, 0xd4, 0x7,
        0xc4, 0xd1, 0x3b, 0xfd, 0x4b, 0x34, 0x73, 0x1c, 0x3e, 0xe6, 0xa5, 0x5b, 0xfb, 0xcc, 0xab, 0x7f,
        0xd, 0x9f, 0x87, 0x2b, 0x88, 0x70, 0x2c, 0x61, 0x7d, 0x26, 0x40, 0x3, 0x82, 0x50, 0x64, 0x54,
        0xc1, 0x93, 0x81, 0x1b, 0x39, 0xb6, 0x7b, 0xf, 0xd0, 0x91, 0x2f, 0xb8, 0xee, 0xb4, 0x1a, 0xea,
        0x3f, 0x41, 0xbf, 0xe0, 0x55, 0xb9, 0xda, 0x85, 0x66, 0xb, 0xd8, 0x90, 0x5a, 0x58, 0x80, 0x5f,
        0x33, 0x6, 0x65, 0x69, 0x35, 0xd5, 0xc0, 0xa7, 0x6d, 0x98, 0x9b, 0x76, 0x45, 0x0, 0x94, 0x56,
        0xb0, 0xfe, 0xdb, 0x20, 0x97, 0xfc, 0xb2, 0xc2, 0xdd, 0x47, 0x4a, 0x1d, 0xe1, 0xeb, 0xd6, 0xe4,
        0xf8, 0x48, 0xf6, 0xf4, 0xa3, 0xd7, 0x9, 0x83, 0x99, 0xb1, 0xaf, 0xf9, 0xb3, 0x21, 0x15, 0x78,
        0xce, 0x4c, 0xca, 0x2e, 0xe7, 0x2d, 0x4d, 0x8a, 0x4e, 0x38, 0x44, 0x28, 0x52, 0x95, 0xd9, 0x1e,
        0x17, 0xf1, 0x60, 0x68, 0xa, 0xdf, 0x2, 0xa0, 0xe9, 0xfa, 0x3d, 0x53, 0x12, 0xb7, 0x7a, 0xc3,
        0xf2, 0x63, 0x9a, 0x19, 0x96, 0x84, 0x6b, 0xba, 0xf7, 0x16, 0x6a, 0xa2, 0x7c, 0xae, 0xe5, 0xf5,
        0x14, 0x51, 0x22, 0xf0, 0xad, 0x4, 0x23, 0x9c, 0xff, 0x8c, 0xe, 0xe2, 0x29, 0x79, 0x71, 0x7e,
        0x75, 0x6f, 0x37, 0xa1, 0xc, 0xef, 0xbc, 0x72, 0x8b, 0x86, 0x10, 0xe8, 0xec, 0xd3, 0x8e, 0x62,
        0x92, 0x4f, 0x24, 0xc5, 0x8, 0x77, 0x11, 0xbe, 0xf3, 0xa6, 0xbb, 0xac, 0x32, 0x36, 0x9d, 0xcf,
        0x57, 0x25, 0xb5, 0xe3, 0x5e, 0x6c, 0xa9, 0x13, 0x5, 0x59, 0x2a, 0x46, 0xbd, 0xa8, 0x3a, 0x1,
        0x49, 0x3c, 0xcd, 0x43, 0x42, 0xed, 0x9e, 0x6e, 0xde, 0xc7, 0x67, 0x18, 0x27, 0xd2, 0x7, 0xd4,
        0x8d, 0xc6, 0x8f, 0xaa, 0x89, 0xcb, 0x30, 0x1f, 0x5d, 0x5c, 0x31, 0xa4, 0xc8, 0x74, 0xdc, 0xc9,
        0x9f, 0xd, 0x2b, 0x87, 0x70, 0x88, 0x61, 0x2c, 0x26, 0x7d, 0x3, 0x40, 0x50, 0x82, 0x54, 0x64,
        0xd1, 0xc4, 0xfd, 0x3b, 0x34, 0x4b, 0x1c, 0x73, 0xe6, 0x3e, 0x5b, 0xa5, 0xcc, 0xfb, 0x7f, 0xab,
        0xba, 0x6b, 0x84, 0x96, 0x19, 0x9a, 0x63, 0xf2, 0xf5, 0xe5, 0xae, 0x7c, 0xa2, 0x6a, 0x16, 0xf7,
        0xa0, 0x2, 0xdf, 0xa, 0x68, 0x60, 0xf1, 0x17, 0xc3, 0x7a, 0xb7, 0x12, 0x53, 0x3d, 0xfa, 0xe9,
        0x8a, 0x4d, 0x2d, 0xe7, 0x2e, 0xca, 0x4c, 0xce, 0x1e, 0xd9, 0x95, 0x52, 0x28, 0x44, 0x38, 0x4e,
        0x83, 0x9, 0xd7, 0xa3, 0xf4, 0xf6, 0x48, 0xf8, 0x78, 0x15, 0x21, 0xb3, 0xf9, 0xaf, 0xb1, 0x99,
        0xc2, 0xb2, 0xfc, 0x97, 0x20, 0xdb, 0xfe, 0xb0, 0xe4, 0xd6, 0xeb, 0xe1, 0x1d, 0x4a, 0x47, 0xdd,
        0xa7, 0xc0, 0xd5, 0x35, 0x69, 0x65, 0x6, 0x33, 0x56, 0x94, 0x0, 0x45, 0x76, 0x9b, 0x98, 0x6d,
        0x85, 0xda, 0xb9, 0x55, 0xe0, 0xbf, 0x41, 0x3f, 0x5f, 0x80, 0x58, 0x5a, 0x90, 0xd8, 0xb, 0x66,
        0xf, 0x7b, 0xb6, 0x39, 0x1b, 0x81, 0x93, 0xc1, 0xea, 0x1a, 0xb4, 0xee, 0xb8, 0x2f, 0x91, 0xd0,
        0x73, 0x1c, 0x4b, 0x34, 0x3b, 0xfd, 0xc4, 0xd1, 0xab, 0x7f, 0xfb, 0xcc, 0xa5, 0x5b, 0x3e, 0xe6,
        0x2c, 0x61, 0x88, 0x70, 0x87, 0x2b, 0xd, 0x9f, 0x64, 0x54, 0x82, 0x50, 0x40, 0x3, 0x7d, 0x26,
        0x1f, 0x30, 0xcb, 0x89, 0xaa, 0x8f, 0xc6, 0x8d, 0xc9, 0xdc, 0x74, 0xc8, 0xa4, 0x31, 0x5c, 0x5d,
        0x6e, 0x9e, 0xed, 0x42, 0x43, 0xcd, 0x3c, 0x49, 0xd4, 0x7, 0xd2, 0x27, 0x18, 0x67, 0xc7, 0xde,
        0x13, 0xa9, 0x6c, 0x5e, 0xe3, 0xb5, 0x25, 0x57, 0x1, 0x3a, 0xa8, 0xbd, 0x46, 0x2a, 0x59, 0x5,
        0xbe, 0x11, 0x77, 0x8, 0xc5, 0x24, 0x4f, 0x92, 0xcf, 0x9d, 0x36, 0x32, 0xac, 0xbb, 0xa6, 0xf3,
        0x72, 0xbc, 0xef, 0xc, 0xa1, 0x37, 0x6f, 0x75, 0x62, 0x8e, 0xd3, 0xec, 0xe8, 0x10, 0x86, 0x8b,
        0x9c, 0x23, 0x4, 0xad, 0xf0, 0x22, 0x51, 0x14, 0x7e, 0x71, 0x79, 0x29, 0xe2, 0xe, 0x8c, 0xff,
        0x2, 0xa0, 0xa, 0xdf, 0x60, 0x68, 0x17, 0xf1, 0x7a, 0xc3, 0x12, 0xb7, 0x3d, 0x53, 0xe9, 0xfa,
        0x6b, 0xba, 0x96, 0x84, 0x9a, 0x19, 0xf2, 0x63, 0xe5, 0xf5, 0x7c, 0xae, 0x6a, 0xa2, 0xf7, 0x16,
        0x9, 0x83, 0xa3, 0xd7, 0xf6, 0xf4, 0xf8, 0x48, 0x15, 0x78, 0xb3, 0x21, 0xaf, 0xf9, 0x99, 0xb1,
        0x4d, 0x8a, 0xe7, 0x2d, 0xca, 0x2e, 0xce, 0x4c, 0xd9, 0x1e, 0x52, 0x95, 0x44, 0x28, 0x4e, 0x38,
        0xc0, 0xa7, 0x35, 0xd5, 0x65, 0x69, 0x33, 0x6, 0x94, 0x56, 0x45, 0x0, 0x9b, 0x76, 0x6d, 0x98,
        0xb2, 0xc2, 0x97, 0xfc, 0xdb, 0x20, 0xb0, 0xfe, 0xd6, 0xe4, 0xe1, 0xeb, 0x4a, 0x1d, 0xdd, 0x47,
        0x7b, 0xf, 0x39, 0xb6, 0x81, 0x1b, 0xc1, 0x93, 0x1a, 0xea, 0xee, 0xb4, 0x2f, 0xb8, 0xd0, 0x91,
        0xda, 0x85, 0x55, 0xb9, 0xbf, 0xe0, 0x3f, 0x41, 0x80, 0x5f, 0x5a, 0x58, 0xd8, 0x90, 0x66, 0xb,
        0x61, 0x2c, 0x70, 0x88, 0x2b, 0x87, 0x9f, 0xd, 0x54, 0x64, 0x50, 0x82, 0x3, 0x40, 0x26, 0x7d,
        0x1c, 0x73, 0x34, 0x4b, 0xfd, 0x3b, 0xd1, 0xc4, 0x7f, 0xab, 0xcc, 0xfb, 0x5b, 0xa5, 0xe6, 0x3e,
        0x9e, 0x6e, 0x42, 0xed, 0xcd, 0x43, 0x49, 0x3c, 0x7, 0xd4, 0x27, 0xd2, 0x67, 0x18, 0xde, 0xc7,
        0x30, 0x1f, 0x89, 0xcb, 0x8f, 0xaa, 0x8d, 0xc6, 0xdc, 0xc9, 0xc8, 0x74, 0x31, 0xa4, 0x5d, 0x5c,
        0x11, 0xbe, 0x8, 0x77, 0x24, 0xc5, 0x92, 0x4f, 0x9d, 0xcf, 0x32, 0x36, 0xbb, 0xac, 0xf3, 0xa6,
        0xa9, 0x13, 0x5e, 0x6c, 0xb5, 0xe3, 0x57, 0x25, 0x3a, 0x1, 0xbd, 0xa8, 0x2a, 0x46, 0x5, 0x59,
        0x23, 0x9c, 0xad, 0x4, 0x22, 0xf0, 0x14, 0x51, 0x71, 0x7e, 0x29, 0x79, 0xe, 0xe2, 0xff, 0x8c,
        0xbc, 0x72, 0xc, 0xef, 0x37, 0xa1, 0x75, 0x6f, 0x8e, 0x62, 0xec, 0xd3, 0x10, 0xe8, 0x8b, 0x86,
        0x2d, 0xe7, 0x8a, 0x4d, 0x4c, 0xce, 0x2e, 0xca, 0x95, 0x52, 0x1e, 0xd9, 0x38, 0x4e, 0x28, 0x44,
        0xd7, 0xa3, 0x83, 0x9, 0x48, 0xf8, 0xf4, 0xf6, 0x21, 0xb3, 0x78, 0x15, 0xb1, 0x99, 0xf9, 0xaf,
        0x84, 0x96, 0xba, 0x6b, 0x63, 0xf2, 0x19, 0x9a, 0xae, 0x7c, 0xf5, 0xe5, 0x16, 0xf7, 0xa2, 0x6a,
        0xdf, 0xa, 0xa0, 0x2, 0xf1, 0x17, 0x68, 0x60, 0xb7, 0x12, 0xc3, 0x7a, 0xfa, 0xe9, 0x53, 0x3d,
        0xb9, 0x55, 0x85, 0xda, 0x41, 0x3f, 0xe0, 0xbf, 0x58, 0x5a, 0x5f, 0x80, 0xb, 0x66, 0x90, 0xd8,
        0xb6, 0x39, 0xf, 0x7b, 0x93, 0xc1, 0x1b, 0x81, 0xb4, 0xee, 0xea, 0x1a, 0x91, 0xd0, 0xb8, 0x2f,
        0xfc, 0x97, 0xc2, 0xb2, 0xfe, 0xb0, 0x20, 0xdb, 0xeb, 0xe1, 0xe4, 0xd6, 0x47, 0xdd, 0x1d, 0x4a,
        0xd5, 0x35, 0xa7, 0xc0, 0x6, 0x33, 0x69, 0x65, 0x0, 0x45, 0x56, 0x94, 0x98, 0x6d, 0x76, 0x9b,
        0xcb, 0x89, 0x1f, 0x30, 0xc6, 0x8d, 0xaa, 0x8f, 0x74, 0xc8, 0xc9, 0xdc, 0x5c, 0x5d, 0xa4, 0x31,
        0xed, 0x42, 0x6e, 0x9e, 0x3c, 0x49, 0x43, 0xcd, 0xd2, 0x27, 0xd4, 0x7, 0xc7, 0xde, 0x18, 0x67,
        0x4b, 0x34, 0x73, 0x1c, 0xc4, 0xd1, 0x3b, 0xfd, 0xfb, 0xcc, 0xab, 0x7f, 0x3e, 0xe6, 0xa5, 0x5b,
        0x88, 0x70, 0x2c, 0x61, 0xd, 0x9f, 0x87, 0x2b, 0x82, 0x50, 0x64, 0x54, 0x7d, 0x26, 0x40, 0x3,
        0xef, 0xc, 0x72, 0xbc, 0x6f, 0x75, 0xa1, 0x37, 0xd3, 0xec, 0x62, 0x8e, 0x86, 0x8b, 0xe8, 0x10,
        0x4, 0xad, 0x9c, 0x23, 0x51, 0x14, 0xf0, 0x22, 0x79, 0x29, 0x7e, 0x71, 0x8c, 0xff, 0xe2, 0xe,
        0x6c, 0x5e, 0x13, 0xa9, 0x25, 0x57, 0xe3, 0xb5, 0xa8, 0xbd, 0x1, 0x3a, 0x59, 0x5, 0x46, 0x2a,
        0x77, 0x8, 0xbe, 0x11, 0x4f, 0x92, 0xc5, 0x24, 0x36, 0x32, 0xcf, 0x9d, 0xa6, 0xf3, 0xac, 0xbb,
};

void increment_CNT(BYTE *CNT, long len){
    long i = len;
    while ((i > 0) && (CNT[i - 1] == 0xff)){
        CNT[i - 1] = 0;
        i--;
    }

    if (i) {
        CNT[i - 1]++;
    }
}

// XORs the in and out buffers, storing the result in out. Length is in bytes.
void xor_buf(const BYTE in[], BYTE out[], size_t len)
{
    size_t idx;

    for (idx = 0; idx < len; idx++)
        out[idx] ^= in[idx];
}



void CTR_Blk_encrypt(BYTE *CNT, BYTE *currBlkPlain, BYTE *CipherText){
    mm_encrypt(tab, CNT, CipherText);
    xor_buf(currBlkPlain, CipherText, SKIPJACK_BLOCK_SIZE);
    increment_CNT(CNT, SKIPJACK_BLOCK_SIZE);
}



void my_main(){
    /// assume we use the block-cipher to send a sequence of value (e.g. temperature)
    /// and each time we guanartee the size of the value is BLOCKSIZE
    /// so that we do not need to append total_data_size to the end of sequence
    BYTE CNT[SKIPJACK_BLOCK_SIZE] = {0xf6, 0x43, 0x25, 0x1c, 0xd2, 0x43, 0x4, 0x4};
    BYTE currBlkPlain[SKIPJACK_BLOCK_SIZE] = {0xe2, 0x70, 0x8c, 0x91, 0x35, 0xff, 0xdf, 0xa5};
    BYTE CipherText[SKIPJACK_BLOCK_SIZE];
    int i;
    for (i = 0; i < 256/SKIPJACK_BLOCK_SIZE; i++) {
    	CTR_Blk_encrypt(CNT, currBlkPlain, CipherText);
	}
}

/*
int
main(void)
{
  my_main();
  return 0;
}
*/