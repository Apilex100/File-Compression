/*
Author : Apilex100 
Date : 2023-July-06
*/

// Max Huffman codeword length: for byte-frequency Huffman on up to 256 distinct
// symbols the longest codeword is at most 255 bits, so size the buffer to 256
// (plus room for the terminating '\0'). This prevents buffer overflows when
// codewords are copied into codeTable.code for skewed/large inputs.
#define MAX 256
// padding is done to ensure that the code generated for each charater will fit byte size.
// i.e : 4 byte + 3bits will be consider as 5 bits.
char padding;
unsigned char N;


// Code table regarding every character in the file
typedef struct codeTable
{
    char x;
    char code[MAX];
} codeTable;

char compressed_extension[]  = ".spd";
char decompressed_extension[] = ".txt";


