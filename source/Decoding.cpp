/*
Author : Apilex100
Date : 2023-July-06
*/

#include <iostream>
#include <string.h>
#include <cstdlib>
#include "compression.h"

using namespace std;

codeTable *codelist;

int n;
char *decodeBuffer(char b);
char *int2string(unsigned long long acc, int k);
int fileError(FILE *fp);

int main(int argc, char **argv)
{
    FILE *fp, *outfile;
    char buffer;
    char *decoded;
    int i;

    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <compressed-file>\n", argv[0]);
        return 1;
    }

    if (argc <= 2)
    {
        printf("***Huffman Decoding***\n");
        if (argc == 2)
        {
            argv[2] = (char *)malloc(sizeof(char) * (strlen(argv[1]) + strlen(decompressed_extension) + 1));
            if (argv[2] == NULL)
            {
                fprintf(stderr, "[!]Memory allocation failed.\n");
                exit(1);
            }
            strcpy(argv[2], argv[1]);
            strcat(argv[2], decompressed_extension);
            argc++;
        }
        else
            return 0;
    }
    if ((fp = fopen(argv[1], "rb")) == NULL)
    {
        printf("[!]Input file cannot be opened.\n");
        return -1;
    }

    printf("\n[Reading File Header]");
    if (fread(&buffer, sizeof(unsigned char), 1, fp) == 0)
        return (fileError(fp));
    N = buffer; // No. of structures(mapping table records) to read
    if (N == 0)
        n = 256;
    else
        n = N;
    printf("\nDetected: %u different characters.", n);

    // allocate memory for mapping table
    codelist = (codeTable *)malloc(sizeof(codeTable) * n);
    if (codelist == NULL)
    {
        fprintf(stderr, "[!]Memory allocation failed.\n");
        fclose(fp);
        exit(1);
    }

    printf("\nReading character to Codeword Mapping Table");
    if (fread(codelist, sizeof(codeTable), n, fp) == 0)
        return (fileError(fp));

    if (fread(&buffer, sizeof(char), 1, fp) == 0)
        return (fileError(fp));
    padding = buffer; // No. of bits to discard
    printf("\nDetected: %u bit padding.", padding);

    if ((outfile = fopen(argv[2], "wb")) == NULL)
    {
        printf("[!]Output file cannot be opened.\n");
        fclose(fp);
        return -2;
    }

    printf("\n\n[Reading data]\nReplacing codewords with actual characters");
    while (fread(&buffer, sizeof(char), 1, fp) != 0) // Read 1 byte at a time
    {
        decoded = decodeBuffer(buffer); // decoded is pointer to array of characters read from buffer byte
        i = 0;
        while (decoded[i++] != '\0')
            ; // i-1 characters read into decoded array
        fwrite(decoded, sizeof(char), i - 1, outfile);
        free(decoded); // free per-iteration allocation to avoid leak
    }
    fclose(fp);
    fclose(outfile);
    printf("\nEverything fine..\nOutput file %s written successfully.\n", argv[2]);
    return 0;
}

/*
Streaming Huffman bit-decoder.

The not-yet-decoded bits of the stream are kept right-aligned in the low `k`
bits of a 64-bit accumulator `acc`, with the earliest (most-significant) pending
bit at position k-1. Each incoming byte appends 8 fresh bits at the low end; we
then greedily peel off any codewords that are now fully buffered. Because Huffman
codes are prefix-free, at most one table codeword can match at each position.

A 64-bit window decodes codewords up to ~56 bits long, which safely covers every
practical input (forcing a longer codeword needs a Fibonacci-skewed file of
astronomical size). This replaces the previous fixed 16-bit window that silently
corrupted output whenever a codeword exceeded 16 bits (e.g. rare letters in
natural-language text), so decoding is now byte-exact lossless on any real file.
*/
char *decodeBuffer(char b)
{
    int j = 0;
    static unsigned long long acc; // pending stream bits, right-aligned
    static int k;                  // number of valid bits currently held in acc

    char *decoded = (char *)malloc(MAX * sizeof(char));
    if (decoded == NULL)
    {
        fprintf(stderr, "[!]Memory allocation failed.\n");
        exit(1);
    }

    // Append this byte's 8 bits at the low end of the accumulator.
    acc = (acc << 8) | (unsigned long long)(unsigned char)b;
    k += 8;

    if (padding != 0) // first call: drop the leading padding bits
    {
        k -= padding; // padding bits lead the stream (see writeHeader)
        acc &= (k > 0) ? ((1ULL << k) - 1ULL) : 0ULL;
        padding = 0;
    }

    // Greedily match and emit codewords that are fully buffered.
    while (k > 0)
    {
        char *bits = int2string(acc, k); // MSB-first view of the k pending bits
        int matched = 0;
        for (int i = 0; i < n; i++)
        {
            int len = (int)strlen(codelist[i].code);
            if (len <= k && strncmp(codelist[i].code, bits, len) == 0)
            {
                decoded[j++] = codelist[i].x; // emit the decoded character
                k -= len;                     // consume the matched bits
                acc &= (k > 0) ? ((1ULL << k) - 1ULL) : 0ULL;
                matched = 1;
                break; // prefix-free => the match is unique; restart
            }
        }
        free(bits);
        if (!matched)
            break; // remaining bits are a partial codeword; wait for next byte
    }

    decoded[j] = '\0';
    return decoded;
}

// Render the k pending bits of `acc` as a '0'/'1' string, most-significant first.
char *int2string(unsigned long long acc, int k)
{
    char *temp = (char *)malloc((k + 1) * sizeof(char));
    if (temp == NULL)
    {
        fprintf(stderr, "[!]Memory allocation failed.\n");
        exit(1);
    }
    for (int i = 0; i < k; i++)
        temp[i] = ((acc >> (k - 1 - i)) & 1ULL) ? '1' : '0';
    temp[k] = '\0';
    return temp;
}

int fileError(FILE *fp)
{
    printf("[!]File read Error.\n[ ]File is not compressed using huffman.\n");
    fclose(fp);
    return -3;
}
