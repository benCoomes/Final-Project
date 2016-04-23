/*******************************************************************************
 * 
 * Name:       Ben Coomes, Joshua Dunster, Megan Fowler, Johnathan Sarasua
 * Due Date:   29 April 2016 @ 8:00 AM
 * Class:      CPSC 3600 - Section 001
 * Assignment: Project
 * 
 * Module Name: StringHash.h
 * Summary: This module contains the required struct definitions and 
 *          functions to create a hashmap which stores null terminated
 *          character strings as well as a count of how many times a
 *          string has been added to the map. 
 *
 * Routines:
 *  void initHash(StringHash *hash)
 *  void initNHash(StringHash *hash, uint32_t size)
 *  void reallocHash(StringHash *hash, uint32_t size)
 *  void addString(StringHash *hash, char *str, uint32_t len)
 *  void deleteHash(StringHash *hash)
 *  uint32_t hashString(char* str, uint32_t len)
 *
 ******************************************************************************/
#ifndef STRINGHASH_H
#define STRINGHASH_H

#define DEF_SIZE 253;

typedef struct StringHash
{
    HashNode *buckets;
    uint32_t size;
} StringHash;

typedef struct HashNode
{
    char* key;
    int count;
    uint32_t hash;
} HashNode;

/* Initialize an empty StringHash of default size */
void initHash(StringHash *hash);

/* Initialize an empty StringHash with size buckets */
void initNHash(StringHash *hash, uint32_t size);

/* Reallocates hash to be a new StringHash with size buckets containing the same
 * elements as hash did
 */
void reallocHash(StringHash *hash, uint32_t size);

/* Adds a string str of length len to the StringHash hash, setting count to 1 if
 * it is not yet in the hash, or incrementing count if the string is already in
 * the hash
 */
void addString(StringHash *hash, char *str, uint32_t len);

/* Frees the dynamically allocated variables of the StringHash hash */
void deleteHash(StringHash *hash);

/* Creates an integer hash of the string str of length len */
uint32_t hashString(char* str, uint32_t len);

#endif
