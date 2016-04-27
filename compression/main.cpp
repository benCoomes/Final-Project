#include "Compression.h"
#include <fstream>
#include <iostream>
#include <stdlib.h>

/* main function - only for testing */
int main(int argc, char *argv[])
{
    fstream filein;
    fstream fileout;
    string input = "";
    string line = "";

    filein.open("../../Compression_Examples/snapshot.jpg", ios::in);
    fileout.open("../../Compression_Examples/test.jpg.output", ios::out);

    if(filein.is_open())
    {
        while(getline(filein, line))
            input = input + line + '\n';
        filein.close();
    }
  
    uint32_t len = input.size();
    uint32_t newLen = input.size();
    char* comp = charCompress(input.c_str(), len, &newLen);
    char* decomp = charDecompress(comp, newLen, &len);
    string output(comp, newLen);

    if(fileout.is_open())
    {
        fileout << output;
        fileout.close();
    }

    free(comp);
    free(decomp);
    return 0;
}
