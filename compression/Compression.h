/*******************************************************************************
 * 
 * Name:       Ben Coomes, Joshua Dunster, Megan Fowler, Johnathan Sarasua
 * Due Date:   29 April 2016 @ 8:00 AM
 * Class:      CPSC 3600 - Section 001
 * Assignment: Project
 * 
 * Module Name: Compression.h
 * Summary: This module contains the required functions to compress and
 *          decompress a character string based on pattern matching with
 *          a dictionary.
 *
 * Routines:
 *  string compress(string str)
 *  char*  charCompress(const char *str, uint32_t len, uint32_t *newLen)
 *  string decompress(string str)
 *  char*  charDecompress(const char *str, uint32_t len, uint32_t *newLen)
 *
 ******************************************************************************/
#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <stdint.h>
#include <string>
#include <map>

using namespace std;

typedef map<string, int>::iterator dict_iter;

/* Compresses the given string and returns the compressed version */
string compress(string str);

/* Compresses the given character string str of length len to its compressed
 * version, returning a pointer to the compressed string and storing the length
 * of the compressed string in newLen. Returrned memory must be free'd.
 */
char* charCompress(const char *str, uint32_t len, uint32_t *newLen);

/* Decompresses the given string previously compressed by compress() with
 * and returns the decompressed version. Returns an empty string if the
 * compressed string is improperly formatted.
 */
string decompress(string str);

/* Decompresses the given character string str of length len which was
 * previously compressed with charCompress(), returning a pointer to
 * the decompressed character string and storing the length of the
 * decompressed string in newLen. Returns an empty string if the compressed
 * string is improperly formatted. The returned memory will need to be free'd.
 */
char* charDecompress(const char *str, uint32_t len, uint32_t *newLen);

#endif
