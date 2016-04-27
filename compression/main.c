#include "Compression.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* main function - only for testing 
 * example compilation: g++ -Wall -c -o Compression.o Compression.cpp
 *                      gcc -Wall -c -o main.o main.c
 *                      g++ -Wall -o test Compression.o main.o
 */
int main(int argc, char *argv[])
{
    char str[] = "1234123412341234 lel lel lel lel432112341234123412341234ABCDABCD";
    uint32_t len = strlen(str), newLen;

    char *comp = charCompress_C(str, len, &newLen);
    printf("Length: %d, Compressed Length: %d\n", len, newLen);
    fwrite(comp, sizeof(char), newLen, stdout);
    printf("\n");

    char *decomp = charDecompress_C(comp, newLen, &len);
    fwrite(decomp, sizeof(char), len, stdout);
    printf("\n");

    free(comp);
    free(decomp);

    return 0;
}
