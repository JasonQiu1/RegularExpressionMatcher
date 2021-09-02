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

#define MAX_STRING_LENGTH 5000

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Keeps track of the number of states to allocate memory when interpreting.
int numState;

//  Represents the states in the NFA.
//  'c' represents the value of the state, which is 0-255 for matching a
//	character, 256 to represent the 'Match' state, or 257 to represent a
//	state that splits into two states, that may or may not include itself.
//  'pOut' points to the next state.
//  'pOutSplit' points to the other state of a 'Split' state.
//  'lastListID' is used in append to prevent duplicates in the 
//	DanglingStateOutList.
typedef struct State State;
struct State {
    int c;
    State* pOut;
    State* pOutSplit;
    int lastListID;
};

enum {
    Match = 256,
    Split = 257
};

// State constructor
State*
pState(int c, State* pOut, State* pOutSplit) {
    numState++;
    return &(State){ c, pOut, pOutSplit, 0 };
}

// Initially holds all states with dangling out pointers for a given fragment 
typedef union DanglingStateOutList DanglingStateOutList;
union DanglingStateOutList {
    DanglingStateOutList* pNext;
    State* pOut;
};

// DanglingStateOutList constructor
DanglingStateOutList*
danglingStateOutList(State** ppOut) {
    return &(DanglingStateOutList){ .pOut = *ppOut };
}

// Uses last list ID to prevent duplicates from being appended.
//DanglingStateOutList*
//append(DanglingStateOutList* pFirst, DanglingStateOutList* pSecond) {
//
//}

// Points all the dangling out states in the list to the given state.
void
patch(DanglingStateOutList* pList, State* pState) {
    DanglingStateOutList* pNext;
    do {
        pNext = pList->pNext;
        pList->pOut = pState;
        pList = pNext;
    } while (pNext);
}

//  Initially represents an incomplete state.
//  'pStart' points to the starting state of the fragment.
//  'pOut' points to the state pointers waiting to be filled using patch.
typedef struct Fragment Fragment;
struct Fragment {
    State* pStart;
    DanglingStateOutList* pOut;
};

//  Fragment constructor.
Fragment
fragment(State* pState, DanglingStateOutList* pOut) {
    return (Fragment){ pState, pOut };
}

// Character used as the concatenation operator for conversion to postfix.
#define CONCAT_OP '%'

// Binding strength for each operator.
enum Binding {
    LEFTPAREN = 0,
    PIPE = 1,
    CONCATENATION = 2,
    STAR = 3,
    PLUS = 3,
    QUERY = 3
};

char getBinding(char c) {
    switch(c) {
        case '(':
            return LEFTPAREN;
        case '|':
            return PIPE;
        case CONCAT_OP:
            return CONCATENATION;
        case '*':
            return STAR;
        case '+':
            return PLUS;
        case '?':
            return QUERY;
        default:
            return 255;
    }
}

//  Converts the infix regex to postfix.
//  Escapes all 'CONCAT_OP' characters and
//	inserts CONCAT_OP as the concatenation operator.
char*
regexToPostfix(char* regex) {
    char* result = (char*)malloc(sizeof(char) * MAX_STRING_LENGTH);
    char* pResult = result;
    char ops[MAX_STRING_LENGTH];
    int opsLength = 0;

    #define push(c) *pResult++ = c
    #define peekOp() ops[opsLength-1]
    #define popOp() ops[--opsLength] 
    #define pushOp(op) ops[opsLength++] = op

    //  Pop ops into result until op is on top.
    #define popOpUntil(op) \
        while(peekOp() != op) push(popOp())

    //  Pops all operators in ops that are of equal or higher binding to op
    //      into the result stack.
    #define popOpGreBindings(op) \
        while (getBinding(peekOp()) >= getBinding(op)) push(popOp())

    char prevC = '|';
    for (; *regex; regex++) {
        switch(*regex) {
            case '|':
                // Intentional fall-through.
            case '*':
                // Intentional fall-through.
            case '+':
                // Intentional fall-through.
            case '?':
                popOpGreBindings(*regex);
                pushOp(*regex);
                break;
            case CONCAT_OP:
                // Escape regular CONCAT_OP characters.
                push('\\');
                push(CONCAT_OP);
                break;
            case ')':
                // Flush op stack until start of group is popped.
                popOpUntil('(');
                popOp();
                break;
            case '(':
                // Make sure groups are also concatenated.
                if (prevC != '|' && prevC != '(')  {
                    popOpGreBindings(CONCAT_OP);
                    pushOp(CONCAT_OP);
                }
                pushOp(*regex);
                break;
            // Adds a concat operator if the previous character is not '|'.
            default:
                if (prevC != '|' && prevC != '(')  {
                    popOpGreBindings(CONCAT_OP);
                    pushOp(CONCAT_OP);
                }
                push(*regex);
                break; 
        }
        prevC = *regex;
    }

    // Append the rest of the operators in the stack to result.
    while (opsLength > 0) push(popOp());

    // Append the null terminator.
    push('\0');

    return result;
}

//State**
//buildNFA(const char* postfix) {
//
//}

//int
//match(char* regex, char* string) {
//    State** ppStart = buildNFA(regexToPostfix(regex));
//    for (; *string; string++) {
//        if (*string == (*ppStart)->c) {
//
//        } else {
//            break;
//        }
//    }
//
//    return false;
//}

int main() {
    // Get input.
    char regex[MAX_STRING_LENGTH]; 
    char string[MAX_STRING_LENGTH]; 
    printf("Enter regular expression:\n");
    scanf("%s", regex);
    printf("Enter string to match:\n");
    scanf("%s", string);

    numState = 0;
    char* postfix = regexToPostfix(regex);
    printf("Postfix: %s\n", postfix);
    free(postfix);
    //match(regex, string);

    printf("Done.\n");
    return 0;
}
