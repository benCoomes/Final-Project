/*******************************************************************************
 * 
 * Name:       Ben Coomes, Joshua Dunster, Megan Fowler, Johnathan Sarasua
 * Due Date:   29 April 2016 @ 8:00 AM
 * Class:      CPSC 3600 - Section 001
 * Assignment: Project
 * 
 * Module Name: Compression.cpp
 * Summary: This module contains the required functions to compress and
 *          decompress a character string based on pattern matching with
 *          a dictionary.
 *
 * Routines:
 *  string compress(string str)
 *  string decompress(string str)
 *
 ******************************************************************************/
#include "Compression.h"
#include <iostream>

/* Compresses the given string and returns the compressed version */
string compress(string str)
{
    map<string, int> dict;
    string comp = "";

    /* Add substrings of length 4 to the dictionary */
    for(int i = 0; i < (str.size() - 3); i += 4)
        dict[str.substr(i, 4)]++;

    /* Remove dictionary elements with less than six entries */   
    for(dict_iter iter = dict.begin(); iter != dict.end(); iter++)
        if(iter->second < 6)
            dict.erase(iter);
    
    /* If more than 253 elements are in the dictionary dict, remove those
     * with the lowest count
     */
    while(dict.size() >= 253)
    {
        dict_iter min = dict.begin();
        for(dict_iter iter = dict.begin(); iter != dict.end(); iter++)
            if(iter->second < min->second)
                min = iter;
        dict.erase(min);
    }

    /* Add the dictionary to the beginning of the compressed string */
    for(dict_iter iter = dict.begin(); iter != dict.end(); iter++)
        comp = comp + (char)0x04 + iter->first;
    comp = comp + (char)0xFF;

    /* Replace keys in the string and add it to the compressed version */
    for(int i = 0; i < str.size(); i++)
    {
        if((i < (str.size() - 3)) && (dict.find(str.substr(i, 4)) != dict.end()))
        {
            char count = 0x01, j = i + 4;
            while((count < 0xFE) && (j < (str.size() - 3)) &&
                  (str.substr(i, 4).compare(str.substr(j, 4)) == 0))
            {
                count++;
                j += 4;
            }

            comp = comp + (char)(0xFE);
            comp = comp + (char)(distance(dict.begin(), dict.find(str.substr(i, 4))));
            comp = comp + count;
            i = j - 1;
        }
        else if(str[i] == 0xFE)
            comp = comp + (char)(0xFE) + (char)(0xFE);
        else
            comp = comp + str[i];
    }

    /* Print dictionary elements - only for testing */
    for(dict_iter iter = dict.begin(); iter != dict.end(); iter++)
        cout << "String: " << iter->first << "\tValue: " <<
                iter->second << endl;
    
    return comp;
}

/* Decompresses the given string previously compressed by compress() with
 * and returns the decompressed version
 */
string decompress(string str)
{
    return str;
}

/* main function - only for testing */
int main(int argc, char *argv[])
{
    string comp = "Hellooo 123412341234123412341234123412341234 <> ABCDABCDABCDABCDABCDABCDABCD********1234,";
    comp = comp + (char)(0xFE);
    comp = comp + "world!";
    cout << compress(comp) << endl;
}


