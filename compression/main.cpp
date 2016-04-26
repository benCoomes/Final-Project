#include "Compression.h"
#include <fstream>

/* main function - only for testing */
int main(int argc, char *argv[])
{
    fstream filein;
    fstream fileout;
    string input = "";
    string line = "";

    filein.open("../../Compression_Examples/snapshotHUGE.jpg.output", ios::in);
    fileout.open("../../Compression_Examples/testHUGE.jpg", ios::out);

    if(filein.is_open())
    {
        while(getline(filein, line))
            input = input + line + '\n';
        filein.close();
    }
   
    string output = decompress(input);

    if(fileout.is_open())
    {
        fileout << output;
        fileout.close();
    }

    return 0;
}
