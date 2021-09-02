/*
 * =====================================================================================
 *
 *       Filename:  regExpFA.c
 *
 *    Description:  Regular expression matcher modelled after https://swtch.com/~rsc/regexp/regexp1.html. 
 *
 *        Version:  1.0
 *        Created:  09/01/21 16:52:01
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Jason Qiu (jq), jasonwqiu@gmail.com
 *   Organization:  
 *
 * =====================================================================================
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum {
    Match = 256,
    Split = 257
};

int numState = 0;

typedef struct State State;
struct State {
    int c;
    State* pOut;
    State* pOutSplit;
    int lastListID;
};

State*
pState(int c, State* pOut, State* pOutSplit) {
    numState++;
    return &(State){ c, pOut, pOutSplit, 0 };
}

typedef union PList PList;
union PList {
    PList* pNext;
    State* pOut;
};

PList*
pList(State** ppOut) {
    return &(PList){ .pOut = *ppOut };
}

typedef struct Fragment Fragment;
struct Fragment {
    State* pStart;
    PList* pOut;
};

Fragment
fragment(State* pState, PList* pOut) {
    return (Fragment){ pState, pOut };
}

int main() {
    char* regex = (char*)malloc(sizeof(char)*5000);
    char* string = (char*)malloc(sizeof(char)*5000);

    printf("Enter regular expression:\n");
    scanf("%s", regex);
    printf("Enter string to match:\n");
    scanf("%s", string);
    return 0;
}
