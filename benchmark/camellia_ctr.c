#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stddef.h>


#define CAMELLIA_BLOCK_SIZE 16


typedef unsigned char BYTE;
typedef unsigned long WORD;



/*********************** FUNCTION DEFINITIONS ***********************/
void mm_memset(BYTE arr[], BYTE val, int len) {
    int i;
    for (i = 0; i < len; i++) {
           arr[i] = val;
    }
}

void mm_memcpy(BYTE arr1[], BYTE arr2[], int len) {
    int i;
    for (i = 0; i < len; i++) {
           arr1[i] = arr2[i];
    }
}


// ----- Camellia

const BYTE SIGMA[48] = {
    0xa0,0x9e,0x66,0x7f,0x3b,0xcc,0x90,0x8b,
    0xb6,0x7a,0xe8,0x58,0x4c,0xaa,0x73,0xb2,
    0xc6,0xef,0x37,0x2f,0xe9,0x4f,0x82,0xbe,
    0x54,0xff,0x53,0xa5,0xf1,0xd3,0x6f,0x1c,
    0x10,0xe5,0x27,0xfa,0xde,0x68,0x2d,0x1d,
    0xb0,0x56,0x88,0xc2,0xb3,0xe6,0xc1,0xfd};

const int KSFT1[26] = {
    0,64,0,64,15,79,15,79,30,94,45,109,45,124,60,124,77,13,
    94,30,94,30,111,47,111,47 };
const int KIDX1[26] = {
    0,0,4,4,0,0,4,4,4,4,0,0,4,0,4,4,0,0,0,0,4,4,0,0,4,4 };
const int KSFT2[34] = {
    0,64,0,64,15,79,15,79,30,94,30,94,45,109,45,109,60,124,
    60,124,60,124,77,13,77,13,94,30,94,30,111,47,111,47 };
const int KIDX2[34] = {
    0,0,12,12,8,8,4,4,8,8,12,12,0,0,4,4,0,0,8,8,12,12,
    0,0,4,4,8,8,4,4,0,0,12,12 };

const BYTE SBOX[256] = {
    112,130, 44,236,179, 39,192,229,228,133, 87, 53,234, 12,174, 65,
    35,239,107,147, 69, 25,165, 33,237, 14, 79, 78, 29,101,146,189,
    134,184,175,143,124,235, 31,206, 62, 48,220, 95, 94,197, 11, 26,
    166,225, 57,202,213, 71, 93, 61,217,  1, 90,214, 81, 86,108, 77,
    139, 13,154,102,251,204,176, 45,116, 18, 43, 32,240,177,132,153,
    223, 76,203,194, 52,126,118,  5,109,183,169, 49,209, 23,  4,215,
    20, 88, 58, 97,222, 27, 17, 28, 50, 15,156, 22, 83, 24,242, 34,
    254, 68,207,178,195,181,122,145, 36,  8,232,168, 96,252,105, 80,
    170,208,160,125,161,137, 98,151, 84, 91, 30,149,224,255,100,210,
    16,196,  0, 72,163,247,117,219,138,  3,230,218,  9, 63,221,148,
    135, 92,131,  2,205, 74,144, 51,115,103,246,243,157,127,191,226,
    82,155,216, 38,200, 55,198, 59,129,150,111, 75, 19,190, 99, 46,
    233,121,167,140,159,110,188,142, 41,245,249,182, 47,253,180, 89,
    120,152,  6,106,231, 70,113,186,212, 37,171, 66,136,162,141,250,
    114,  7,185, 85,248,238,172, 10, 54, 73, 42,104, 60, 56,241,164,
    64, 40,211,123,187,201, 67,193, 21,227,173,244,119,199,128,158};

#define SBOX1(n) SBOX[(n)]
#define SBOX2(n) (BYTE)((SBOX[(n)]>>7^SBOX[(n)]<<1)&0xff)
#define SBOX3(n) (BYTE)((SBOX[(n)]>>1^SBOX[(n)]<<7)&0xff)
#define SBOX4(n) SBOX[((n)<<1^(n)>>7)&0xff]

/*
int main()
{
    const int keysize = 128; // must be 128, 192 or 256

    const BYTE ptext[16] = {
        0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
        0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10 };
    const BYTE key[32] = {
        0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
        0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10,
        0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
        0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff };

    BYTE rtext[16], ekey[272];
    int  i;

    printf( "Plaintext  " );
    for( i=0; i<16; i++ ) printf( "%02x ", ptext[i] );
    printf( "\n%dbit Key ", keysize );
    for( i=0; i<keysize/8; i++ ) printf( "%02x ", key[i] );
    printf( "\n" );

    Camellia_Ekeygen( keysize, key, ekey );
    for (int i = 0; i < 272; i++) {
        printf("0x%x, ",ekey[i]);
        if ((i + 1) % 16 == 0) {
            printf("\n");
        }
    }

    BYTE *cipherText = Camellia_Encrypt(keysize, ptext, ekey);

    printf( "Ciphertext " );
    for( i=0; i<16; i++ ) printf( "%02x ", cipherText[i] );
    printf( "\n" );

    Camellia_Decrypt( keysize, cipherText, ekey, rtext );

    printf( "Plaintext  " );
    for( i=0; i<16; i++ ) printf( "%02x ", rtext[i] );
    printf( "\n" );
    return 0;
}*/

void XorBlock( const BYTE *x, const BYTE *y, BYTE *z )
{
    int i;
    for( i=0; i<16; i++ ) z[i] = x[i] ^ y[i];
}


void Camellia_Feistel( const BYTE *x, const BYTE *k, BYTE *y )
{
    BYTE t[8];

    t[0] = SBOX1(x[0]^k[0]);
    t[1] = SBOX2(x[1]^k[1]);
    t[2] = SBOX3(x[2]^k[2]);
    t[3] = SBOX4(x[3]^k[3]);
    t[4] = SBOX2(x[4]^k[4]);
    t[5] = SBOX3(x[5]^k[5]);
    t[6] = SBOX4(x[6]^k[6]);
    t[7] = SBOX1(x[7]^k[7]);

    y[0] ^= t[0]^t[2]^t[3]^t[5]^t[6]^t[7];
    y[1] ^= t[0]^t[1]^t[3]^t[4]^t[6]^t[7];
    y[2] ^= t[0]^t[1]^t[2]^t[4]^t[5]^t[7];
    y[3] ^= t[1]^t[2]^t[3]^t[4]^t[5]^t[6];
    y[4] ^= t[0]^t[1]^t[5]^t[6]^t[7];
    y[5] ^= t[1]^t[2]^t[4]^t[6]^t[7];
    y[6] ^= t[2]^t[3]^t[4]^t[5]^t[7];
    y[7] ^= t[0]^t[3]^t[4]^t[5]^t[6];
}






void BYTEWORD( const BYTE *x, WORD *y )
{
    int i;
    for( i=0; i<4; i++ ){
        y[i] = ((WORD)x[(i<<2)+0]<<24) + ((WORD)x[(i<<2)+1]<<16)
             + ((WORD)x[(i<<2)+2]<<8 ) + ((WORD)x[(i<<2)+3]<<0 );
    }
}

void WORDBYTE( const WORD *x, BYTE *y )
{
    int i;
    for( i=0; i<4; i++ ){
        y[(i<<2)+0] = (BYTE)(x[i]>>24&0xff);
        y[(i<<2)+1] = (BYTE)(x[i]>>16&0xff);
        y[(i<<2)+2] = (BYTE)(x[i]>> 8&0xff);
        y[(i<<2)+3] = (BYTE)(x[i]>> 0&0xff);
    }
}

void RotBlock( const WORD *x, const int n, WORD *y )
{
    int r;
    if ( r == (n & 31) ){
        y[0] = x[((n>>5)+0)&3]<<r^x[((n>>5)+1)&3]>>(32-r);
        y[1] = x[((n>>5)+1)&3]<<r^x[((n>>5)+2)&3]>>(32-r);
    }
    else{
        y[0] = x[((n>>5)+0)&3];
        y[1] = x[((n>>5)+1)&3];
    }
}

void SwapHalf( BYTE *x )
{
    BYTE t;
    int  i;
    for( i=0; i<8; i++ ){
        t = x[i];
        x[i] = x[8+i];
        x[8+i] = t;
    }
}

void Camellia_FLlayer( BYTE *x, const BYTE *kl, const BYTE *kr )
{
    WORD t[4],u[4],v[4];

    BYTEWORD( x, t );
    BYTEWORD( kl, u );
    BYTEWORD( kr, v );

    t[1] ^= (t[0]&u[0])<<1^(t[0]&u[0])>>31;
    t[0] ^= t[1]|u[1];
    t[2] ^= t[3]|v[1];
    t[3] ^= (t[2]&v[0])<<1^(t[2]&v[0])>>31;

    WORDBYTE( t, x );
}


void Camellia_Ekeygen( const int n, const BYTE *k, BYTE *e )
{
    BYTE t[64];
    WORD u[20];
    int  i;

    if( n == 128 ){
        for( i=0 ; i<16; i++ ) t[i] = k[i];
        for( i=16; i<32; i++ ) t[i] = 0;
    }
    else if( n == 192 ){
        for( i=0 ; i<24; i++ ) t[i] = k[i];
        for( i=24; i<32; i++ ) t[i] = k[i-8]^0xff;
    }
    else if( n == 256 ){
        for( i=0 ; i<32; i++ ) t[i] = k[i];
    }

    XorBlock( t+0, t+16, t+32 );

    Camellia_Feistel( t+32, SIGMA+0, t+40 );
    Camellia_Feistel( t+40, SIGMA+8, t+32 );

    XorBlock( t+32, t+0, t+32 );

    Camellia_Feistel( t+32, SIGMA+16, t+40 );
    Camellia_Feistel( t+40, SIGMA+24, t+32 );

    BYTEWORD( t+0,  u+0 );
    BYTEWORD( t+32, u+4 );

    if( n == 128 ){
        for( i=0; i<26; i+=2 ){
            RotBlock( u+KIDX1[i+0], KSFT1[i+0], u+16 );
            RotBlock( u+KIDX1[i+1], KSFT1[i+1], u+18 );
            WORDBYTE( u+16, e+i*8 );
        }
    }
    else{
        XorBlock( t+32, t+16, t+48 );

        Camellia_Feistel( t+48, SIGMA+32, t+56 );
        Camellia_Feistel( t+56, SIGMA+40, t+48 );

        BYTEWORD( t+16, u+8  );
        BYTEWORD( t+48, u+12 );

        for( i=0; i<34; i+=2 ){
            RotBlock( u+KIDX2[i+0], KSFT2[i+0], u+16 );
            RotBlock( u+KIDX2[i+1], KSFT2[i+1], u+18 );
            WORDBYTE( u+16, e+(i<<3) );
        }
    }
}


void Camellia_Encrypt( const int n, const BYTE *p, const BYTE *e, BYTE *cipherText)
{
    int i;
    XorBlock( p, e+0, cipherText );

    for( i = 0; i < 3; i ++ ){
        Camellia_Feistel( cipherText+0, e+16+(i<<4), cipherText + 8 );
        Camellia_Feistel( cipherText+8, e+24+(i<<4), cipherText + 0 );
    }

    Camellia_FLlayer( cipherText, e+64, e+72 );

    for( i=0; i<3; i++ ){
        Camellia_Feistel( cipherText+0, e+80+(i<<4), cipherText + 8 );
        Camellia_Feistel( cipherText+8, e+88+(i<<4), cipherText + 0 );
    }

    Camellia_FLlayer( cipherText, e+128, e+136 );

    for( i=0; i<3; i++ ){
        Camellia_Feistel( cipherText+0, e+144+(i<<4), cipherText + 8 );
        Camellia_Feistel( cipherText+8, e+152+(i<<4), cipherText + 0 );
    }

    if( n == 128 ){
        SwapHalf( cipherText );
        XorBlock( cipherText, e+192, cipherText );
    }
    else{
        Camellia_FLlayer( cipherText, e+192, e+200 );

        for( i=0; i<3; i++ ){
            Camellia_Feistel( cipherText+0, e+208+(i<<4), cipherText + 8 );
            Camellia_Feistel( cipherText+8, e+216+(i<<4), cipherText + 0 );
        }

        SwapHalf( cipherText );
        XorBlock( cipherText, e+256, cipherText );
    }
}

void Camellia_Decrypt( const int n, const BYTE *c, const BYTE *e, BYTE *p )
{
    int i;

    if( n == 128 ){
        XorBlock( c, e+192, p );
    }
    else{
        XorBlock( c, e+256, p );

        for( i=2; i>=0; i-- ){
            Camellia_Feistel( p+0, e+216+(i<<4), p+8 );
            Camellia_Feistel( p+8, e+208+(i<<4), p+0 );
        }

        Camellia_FLlayer( p, e+200, e+192 );
    }

    for( i=2; i>=0; i-- ){
        Camellia_Feistel( p+0, e+152+(i<<4), p+8 );
        Camellia_Feistel( p+8, e+144+(i<<4), p+0 );
    }

    Camellia_FLlayer( p, e+136, e+128 );

    for( i=2; i>=0; i-- ){
        Camellia_Feistel( p+0, e+88+(i<<4), p+8 );
        Camellia_Feistel( p+8, e+80+(i<<4), p+0 );
    }

    Camellia_FLlayer( p, e+72, e+64 );

    for( i=2; i>=0; i-- ){
        Camellia_Feistel( p+0, e+24+(i<<4), p+8 );
        Camellia_Feistel( p+8, e+16+(i<<4), p+0 );
    }

    SwapHalf( p );
    XorBlock( p, e+0, p );
}


//-------- CNT --------

BYTE ekey[272] = {
    0x1, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,
    0xae, 0x71, 0xc3, 0xd5, 0x5b, 0xa6, 0xbf, 0x1d, 0x16, 0x92, 0x40, 0xa7, 0x95, 0xf8, 0x92, 0x56,
    0x1, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,
    0xae, 0x71, 0xc3, 0xd5, 0x5b, 0xa6, 0xbf, 0x1d, 0x16, 0x92, 0x40, 0xa7, 0x95, 0xf8, 0x92, 0x56,
    0xae, 0x71, 0xc3, 0xd5, 0x5b, 0xa6, 0xbf, 0x1d, 0x16, 0x92, 0x40, 0xa7, 0x95, 0xf8, 0x92, 0x56,
    0x89, 0xab, 0xcd, 0xef, 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10, 0x1, 0x23, 0x45, 0x67,
    0x5b, 0xa6, 0xbf, 0x1d, 0x16, 0x92, 0x40, 0xa7, 0x76, 0x54, 0x32, 0x10, 0x1, 0x23, 0x45, 0x67,
    0x5b, 0xa6, 0xbf, 0x1d, 0x16, 0x92, 0x40, 0xa7, 0x95, 0xf8, 0x92, 0x56, 0xae, 0x71, 0xc3, 0xd5,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10, 0x1, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10, 0x1, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0x16, 0x92, 0x40, 0xa7, 0x95, 0xf8, 0x92, 0x56, 0xae, 0x71, 0xc3, 0xd5, 0x5b, 0xa6, 0xbf, 0x1d,
    0x76, 0x54, 0x32, 0x10, 0x1, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0xfe, 0xdc, 0xba, 0x98,
    0x95, 0xf8, 0x92, 0x56, 0xae, 0x71, 0xc3, 0xd5, 0x5b, 0xa6, 0xbf, 0x1d, 0x16, 0x92, 0x40, 0xa7,
    0x30, 0x6a, 0xe7, 0xe6, 0xfe, 0x7f, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x48, 0x6a, 0xe7, 0xe6, 0xfe, 0x7f, 0x0, 0x0,
    0x30, 0x6a, 0xe7, 0xe6, 0xfe, 0x7f, 0x0, 0x0, 0x0, 0x90, 0xd8, 0x8, 0x1, 0x0, 0x0, 0x0,
    0x1, 0x0, 0x0, 0x0, 0xe, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
};


const int keysize = 128;



void increment_CNT(BYTE *CNT, int len){
    int i = len;
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
    Camellia_Encrypt(keysize, CNT, ekey, CipherText);
    xor_buf(currBlkPlain, CipherText, CAMELLIA_BLOCK_SIZE);
    increment_CNT(CNT, CAMELLIA_BLOCK_SIZE);
}





void my_main(){
    /// assume we use the block-cipher to send a sequence of value (e.g. temperature)
    /// and each time we guanartee the size of the value is BLOCKSIZE
    /// so that we do not need to append total_data_size to the end of sequence
    BYTE CNT[CAMELLIA_BLOCK_SIZE] = {
        0xf6, 0x43, 0x25, 0x1c, 0xd2, 0x43, 0x4, 0x4,
        0xdc, 0xa2, 0xda, 0xb1, 0xca, 0xa6, 0x77, 0x60
    };
    BYTE currBlkPlain[CAMELLIA_BLOCK_SIZE] = {
        0xe2, 0x70, 0x8c, 0x91, 0x35, 0xff, 0xdf, 0xa5,
        0x6b, 0x76, 0xdd, 0x55, 0xef, 0xf4, 0x6c, 0x28
    };
    BYTE CipherText[CAMELLIA_BLOCK_SIZE];
    int i;
    for (i = 0; i < 100; i++) {
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
