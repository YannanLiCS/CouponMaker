#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stddef.h>



/* 32-bit word, change to "long" for 16-bit machines */
typedef unsigned long WORD;

/*2 WORD*/
#define GOST_BLOCK_SIZE 2

/* Designed to cope with 15-bit rand() implementations */
#define RAND32 ((WORD)rand() << 17 ^ (WORD)rand() << 9 ^ rand())




/*********************** FUNCTION DEFINITIONS ***********************/
void mm_memset(WORD arr[], WORD val, int len) {
    int i;
    for (i = 0; i < len; i++) {
           arr[i] = val;
    }
}

void mm_memcpy(WORD arr1[], WORD arr2[], int len) {
    int i;
    for (i = 0; i < len; i++) {
           arr1[i] = arr2[i];
    }
}


// ----- gost

static unsigned char const k8[16] = {
    14,  4, 13,  1,  2, 15, 11,  8,  3, 10,  6, 12,  5,  9,  0,  7 };
static unsigned char const k7[16] = {
    15,  1,  8, 14,  6, 11,  3,  4,  9,  7,  2, 13, 12,  0,  5, 10 };
static unsigned char const k6[16] = {
    10,  0,  9, 14,  6,  3, 15,  5,  1, 13, 12,  7, 11,  4,  2,  8 };
static unsigned char const k5[16] = {
     7, 13, 14,  3,  0,  6,  9, 10,  1,  2,  8,  5, 11, 12,  4, 15 };
static unsigned char const k4[16] = {
     2, 12,  4,  1,  7, 10, 11,  6,  8,  5,  3, 15, 13,  0, 14,  9 };
static unsigned char const k3[16] = {
    12,  1, 10, 15,  9,  2,  6,  8,  0, 13,  3,  4, 14,  7,  5, 11 };
static unsigned char const k2[16] = {
     4, 11,  2, 14, 15,  0,  8, 13,  3, 12,  9,  7,  5, 10,  6,  1 };
static unsigned char const k1[16] = {
    13,  2,  8,  4,  6, 15, 11,  1, 10,  9,  3, 14,  5,  0, 12,  7 };

/* Byte-at-a-time substitution boxes */
static unsigned char k87[256];
static unsigned char k65[256];
static unsigned char k43[256];
static unsigned char k21[256];

/*
 * Build byte-at-a-time subtitution tables.
 * This must be called once for global setup.
 */
void
kboxinit(void)
{
    int i;
    for (i = 0; i < 256; i++) {
        k87[i] = k8[i >> 4] << 4 | k7[i & 15];
        k65[i] = k6[i >> 4] << 4 | k5[i & 15];
        k43[i] = k4[i >> 4] << 4 | k3[i & 15];
        k21[i] = k2[i >> 4] << 4 | k1[i & 15];
    }
}

/*
 * Do the substitution and rotation that are the core of the operation,
 * like the expansion, substitution and permutation of the DES.
 * It would be possible to perform DES-like optimisations and store
 * the table entries as 32-bit words, already rotated, but the
 * efficiency gain is questionable.
 *
 * This should be inlined for maximum speed
 */
#if __GNUC__
__inline__
#endif
static WORD
f(WORD x)
{
    /* Do substitutions */
#if 0
    /* This is annoyingly slow */
    x = k8[x>>28 & 15] << 28 | k7[x>>24 & 15] << 24 |
        k6[x>>20 & 15] << 20 | k5[x>>16 & 15] << 16 |
        k4[x>>12 & 15] << 12 | k3[x>> 8 & 15] <<  8 |
        k2[x>> 4 & 15] <<  4 | k1[x     & 15];
#else
    /* This is faster */
    x = k87[x>>24 & 255] << 24 | k65[x>>16 & 255] << 16 |
        k43[x>> 8 & 255] <<  8 | k21[x & 255];
#endif

    /* Rotate left 11 bits */
    return x<<11 | x>>(32-11);
}

/*
 * The GOST standard defines the input in terms of bits 1..64, with
 * bit 1 being the lsb of in[0] and bit 64 being the msb of in[1].
 *
 * The keys are defined similarly, with bit 256 being the msb of key[7].
 */
void gostcrypt_1RD(WORD const in[2], WORD const key[8], WORD out[])
{
    register WORD n1, n2; /* As named in the GOST */

    n1 = in[0];
    n2 = in[1];

    /* Instead of swapping halves, swap names each round */
    n2 ^= f(n1+key[0]);
    n1 ^= f(n2+key[1]);
    n2 ^= f(n1+key[2]);
    n1 ^= f(n2+key[3]);
    n2 ^= f(n1+key[4]);
    n1 ^= f(n2+key[5]);
    n2 ^= f(n1+key[6]);
    n1 ^= f(n2+key[7]);

    n2 ^= f(n1+key[0]);
    n1 ^= f(n2+key[1]);
    n2 ^= f(n1+key[2]);
    n1 ^= f(n2+key[3]);
    n2 ^= f(n1+key[4]);
    n1 ^= f(n2+key[5]);
    n2 ^= f(n1+key[6]);
    n1 ^= f(n2+key[7]);

    n2 ^= f(n1+key[0]);
    n1 ^= f(n2+key[1]);
    n2 ^= f(n1+key[2]);
    n1 ^= f(n2+key[3]);
    n2 ^= f(n1+key[4]);
    n1 ^= f(n2+key[5]);
    n2 ^= f(n1+key[6]);
    n1 ^= f(n2+key[7]);

    n2 ^= f(n1+key[7]);
    n1 ^= f(n2+key[6]);
    n2 ^= f(n1+key[5]);
    n1 ^= f(n2+key[4]);
    n2 ^= f(n1+key[3]);
    n1 ^= f(n2+key[2]);
    n2 ^= f(n1+key[1]);
    n1 ^= f(n2+key[0]);

    /* There is no swap after the last round */
    out[0] = n2;
    out[1] = n1;
}
    

/*
 * The key schedule is somewhat different for decryption.
 * (The key table is used once forward and three times backward.)
 * You could define an expanded key, or just write the code twice,
 * as done here.
 */
void gostdecrypt_1RD(WORD const in[2], WORD const key[8], WORD out[])
{
    register WORD n1, n2; /* As named in the GOST */

    n1 = in[0];
    n2 = in[1];

    n2 ^= f(n1+key[0]);
    n1 ^= f(n2+key[1]);
    n2 ^= f(n1+key[2]);
    n1 ^= f(n2+key[3]);
    n2 ^= f(n1+key[4]);
    n1 ^= f(n2+key[5]);
    n2 ^= f(n1+key[6]);
    n1 ^= f(n2+key[7]);

    n2 ^= f(n1+key[7]);
    n1 ^= f(n2+key[6]);
    n2 ^= f(n1+key[5]);
    n1 ^= f(n2+key[4]);
    n2 ^= f(n1+key[3]);
    n1 ^= f(n2+key[2]);
    n2 ^= f(n1+key[1]);
    n1 ^= f(n2+key[0]);

    n2 ^= f(n1+key[7]);
    n1 ^= f(n2+key[6]);
    n2 ^= f(n1+key[5]);
    n1 ^= f(n2+key[4]);
    n2 ^= f(n1+key[3]);
    n1 ^= f(n2+key[2]);
    n2 ^= f(n1+key[1]);
    n1 ^= f(n2+key[0]);

    n2 ^= f(n1+key[7]);
    n1 ^= f(n2+key[6]);
    n2 ^= f(n1+key[5]);
    n1 ^= f(n2+key[4]);
    n2 ^= f(n1+key[3]);
    n1 ^= f(n2+key[2]);
    n2 ^= f(n1+key[1]);
    n1 ^= f(n2+key[0]);

    out[0] = n2;
    out[1] = n1;
}

/*
 * The GOST "Output feedback" standard.  It seems closer morally
 * to the counter feedback mode some people have proposed for DES.
 * The avoidance of the short cycles that are possible in OFB seems
 * like a Good Thing.
 *
 * Calling it the stream mode makes more sense.
 *
 * The IV is encrypted with the key to produce the initial counter value.
 * Then, for each output block, a constant is added, modulo 2^32-1
 * (0 is represented as all-ones, not all-zeros), to each half of
 * the counter, and the counter is encrypted to produce the value
 * to XOR with the output.
 *
 * Len is the number of blocks.  Sub-block encryption is
 * left as an exercise for the user.  Remember that the
 * standard defines everything in a little-endian manner,
 * so you want to use the low bit of gamma[0] first.
 *
 * OFB is, of course, self-inverse, so there is only one function.
 */

/* The constants for addition */
#define C1 0x01010104
#define C2 0x01010101




/*
 * The CFB mode is just what you'd expect.  Each block of ciphertext y[] is
 * derived from the input x[] by the following pseudocode:
 * y[i] = x[i] ^ gostcrypt_1RD(y[i-1])
 * x[i] = y[i] ^ gostcrypt_1RD(y[i-1])
 * Where y[-1] is the IV.
 *
 * The IV is modified in place.  Again, len is in *blocks*.
 */


/*
 * The message suthetication code uses only 16 of the 32 rounds.
 * There *is* a swap after the 16th round.
 * The last block should be padded to 64 bits with zeros.
 * len is the number of *blocks* in the input.
 */
void
gostmac(WORD const *in, int len, WORD out[2], WORD const key[8])
{
    register WORD n1, n2; /* As named in the GOST */

    n1 = 0;
    n2 = 0;

    while (len--) {
        n1 ^= *in++;
        n2 = *in++;

        /* Instead of swapping halves, swap names each round */
        n2 ^= f(n1+key[0]);
        n1 ^= f(n2+key[1]);
        n2 ^= f(n1+key[2]);
        n1 ^= f(n2+key[3]);
        n2 ^= f(n1+key[4]);
        n1 ^= f(n2+key[5]);
        n2 ^= f(n1+key[6]);
        n1 ^= f(n2+key[7]);

        n2 ^= f(n1+key[0]);
        n1 ^= f(n2+key[1]);
        n2 ^= f(n1+key[2]);
        n1 ^= f(n2+key[3]);
        n2 ^= f(n1+key[4]);
        n1 ^= f(n2+key[5]);
        n2 ^= f(n1+key[6]);
        n1 ^= f(n2+key[7]);
    }

    out[0] = n1;
    out[1] = n2;
}






//-------- OFB --------

WORD key[8] = {
        0x6abeb7e0, 0xe643a35c, 0xb1ffd545, 0x85f47db1,
        0x928f1d20, 0x9082472c, 0xdd8060a6, 0x98253225
};



// XORs the in and out buffers, storing the result in out. Length is in bytes.
void xor_buf(const WORD in[], WORD out[], size_t len)
{
    size_t idx;
    for (idx = 0; idx < len; idx++)
        out[idx] ^= in[idx];
}


void increment_CNT(WORD *CNT, int len){
    int i = len;
    while ((i > 0) && (CNT[i - 1] == 0xffffffff)){
        CNT[i - 1] = 0;
        i--;
    }

    if (i) {
        CNT[i - 1]++;
    }
}


void CTR_Blk_encrypt(WORD *CNT, WORD *currBlkPlain, WORD *CipherText){
    gostcrypt_1RD(CNT, key, CipherText);
    xor_buf(currBlkPlain, CipherText, GOST_BLOCK_SIZE);
    increment_CNT(CNT, GOST_BLOCK_SIZE);
}







void my_main(){
    /// assume we use the block-cipher to send a sequence of value (e.g. temperature)
    /// and each time we guanartee the size of the value is BLOCKSIZE
    /// so that we do not need to append total_data_size to the end of sequence
    WORD CNT[GOST_BLOCK_SIZE] = {0x3abdf7e0, 0xe643a43d};;
    WORD currBlkPlain[GOST_BLOCK_SIZE] = {0x6dfeb7e1, 0xe643a47c};
    WORD CipherText[GOST_BLOCK_SIZE];
    int i;
    for (i = 0; i < 64/GOST_BLOCK_SIZE; i++) {
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
