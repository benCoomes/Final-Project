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
#include <stdint.h>
#include <vector>

/* Compresses the given string and returns the compressed version */
string compress(string str)
{
    map<string, int> dict;
    string comp = "";

    /* Add substrings of length 4 to the dictionary */
    for(int i = 0; i < ((int)(str.size()) - 3); i += 4)
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
        comp = comp + (char)(0x04) + iter->first;
    comp = comp + (char)(0x00);

    /* Replace keys in the string and add it to the compressed version */
    for(uint32_t i = 0; i < str.size(); i++)
    {
        /* Check first if this substring is in the dictionary, then check if it
         * is the escape character, then default to just copying the current
         * character
         */
        if((i < (str.size() - 3)) && (dict.find(str.substr(i, 4)) != dict.end()))
        {
            /* Count the occurrences of this key string */
            unsigned char count = 0x01;
            uint32_t j = i + 4;
            while((count < (unsigned char)(0xFD)) && (j < (str.size() - 3)) &&
                  (str.substr(i, 4).compare(str.substr(j, 4)) == 0))
            {
                count++;
                j += 4;
            }

            comp = comp + (char)(0xFE);
            comp = comp + (char)(distance(dict.begin(), dict.find(str.substr(i, 4))));
            comp = comp + (char)(count);
            i = j - 1;
        }
        else if((unsigned char)(str[i]) == (unsigned char)(0xFE))
            comp = (comp + (char)(0xFE)) + (char)(0xFE);
        else
            comp = comp + str[i];
    }

    return comp;
}

/* Decompresses the given string previously compressed by compress() with
 * and returns the decompressed version. Returns an empty string if the
 * compressed string is improperly formatted.
 */
string decompress(string str)
{
    vector<string> dict;
    string decomp = "";
    uint32_t pos = 0;

    /* Rebuild the dictionary by retrieving elements from the compressed
     * string and adding them to the vector.
     */
    while((pos < str.size()) &&
          ((unsigned char)(str[pos]) != (unsigned char)(0x00)))
    {
        /* Make sure there is enough left of the string to take this entry */
        if((pos + (unsigned char)(str[pos])) >= str.size())
            return "";
        
        dict.push_back(str.substr(pos + 1, (unsigned char)(str[pos])));
        pos += (unsigned char)(str[pos]) + 1;
    }
    
    if(pos == str.size())
        return "";

    /* Go to next position after dictionary and start decompressing the string */
    pos++;
    while(pos < str.size())
    {
        if((unsigned char)(str[pos]) == (unsigned char)(0xFE))
        {
            /* Check whether the current substring is a compressed dictionary
             * entry, just an escape character, or invalid
             */
            if((pos + 2 < str.size()) && ((unsigned char)(str[pos + 1]) <
               (unsigned char)(0xFE)))
            {
                if((unsigned char)(str[pos + 1]) >= dict.size())
                    return "";
                
                for(unsigned char i = 0; i < (unsigned char)(str[pos + 2]); i++)
                    decomp = decomp + dict[(unsigned char)(str[pos + 1])];

                pos += 3;
            }
            else if((pos + 1 < str.size()) && (unsigned char)(str[pos + 1]) ==
                    (unsigned char)(0xFE))
            {
                decomp = decomp + (char)(0xFE);
                pos += 2;
            }
            else
                return "";
        }
        else
        {
            /* If the substring is just a normal character, copy it */
            decomp = decomp + str[pos];
            pos++;
        }
    }

    return decomp;
}
