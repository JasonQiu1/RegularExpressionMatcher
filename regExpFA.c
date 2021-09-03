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
//	DanglingStateOuts.
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
    State* s = (State*)malloc(sizeof(State));
    s->c = c;
    s->pOut = pOut;
    s->pOutSplit = pOutSplit;
    s->lastListID = 0;
    return s;
    // Why doesn't this work? 'lastListID' becomes junk? Memory not
    // being allocated properly, seems like.
    // return &(State){ c, pOut, pOutSplit, 0 };
}

// Initially holds all states with dangling out pointers for a given fragment 
typedef union DanglingStateOuts DanglingStateOuts;
union DanglingStateOuts {
    State* pOut;
    DanglingStateOuts* pNext;
};

// DanglingStateOuts constructor. Uses pointer magic to hijack a
// reference to the state pointer instead of pointing directly to the state.
DanglingStateOuts*
pDanglingStateOuts(State** ppOut) {
    DanglingStateOuts* new = (DanglingStateOuts*)(ppOut);
    new->pNext = NULL;
    return new;
}

// Appends dangling outs together.
DanglingStateOuts*
append(DanglingStateOuts* pFirst, DanglingStateOuts* pSecond) {
    DanglingStateOuts* res = pFirst;
    while (pFirst->pNext) pFirst = pFirst->pNext;
    pFirst->pNext = pSecond;
    return res;
}

// Points all the dangling out states in the list to the given state.
void
patch(DanglingStateOuts* pList, State* pState) {
    DanglingStateOuts* pNext;
    while(pList) {
        pNext = pList->pNext;
        pList->pOut = pState;
        pList = pNext;
    }
}

//  Initially represents an incomplete state.
//  'pStart' points to the starting state of the fragment.
//  'pOut' points to the state pointers waiting to be filled using patch.
typedef struct Fragment Fragment;
struct Fragment {
    State* pStart;
    DanglingStateOuts* pOut;
};

//  Fragment constructor.
Fragment
fragment(State* pState, DanglingStateOuts* pOut) {
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
    #undef push
    #undef peekOp 
    #undef popOp
    #undef pushOp
    #undef popOpUntil
    #undef popOpGreBindings
}

State*
buildNFA(const char* postfix) {
    Fragment frags[MAX_STRING_LENGTH], f1, f2;
    Fragment* pFrags = frags;
    State* s;
        
    #define pop() (*--pFrags)
    #define push(f) *pFrags++ = f

    for (; *postfix; postfix++) {
        switch(*postfix) {
            case '*':
                f1 = pop();
                s = pState(Split, f1.pStart, NULL);
                patch(f1.pOut, s);
                push(fragment(s, pDanglingStateOuts(&s->pOutSplit)));
                break;
            case '+':
                f1 = pop();
                s = pState(Split, f1.pStart, NULL);
                patch(f1.pOut, s);
                push(fragment(f1.pStart, 
                              pDanglingStateOuts(&s->pOutSplit)));
                break;
            case '?':
                f1 = pop();
                s = pState(Split, f1.pStart, NULL);
                push(fragment(s, 
                              append(f1.pOut,
                                     pDanglingStateOuts(&s->pOutSplit))));
                break;
            case CONCAT_OP:
                f2 = pop();
                f1 = pop();
                patch(f1.pOut, f2.pStart);
                push(fragment(f1.pStart, f2.pOut));
                break;
            case '|':
                f2 = pop();
                f1 = pop();
                s = pState(Split, f1.pStart, f2.pStart);
                push(fragment(s, append(f1.pOut, f2.pOut)));
                break;
            default:
                s = pState(*postfix, NULL, NULL);
                push(fragment(s, pDanglingStateOuts(&s->pOut))); 
                break;
        }
    }

    f1 = pop();
    // Bad regex
    if (pFrags != frags) return NULL;

    // Close the NFA by pointing all dangling outs to match.
    patch(f1.pOut, pState(Match, NULL, NULL));

    return f1.pStart;
    #undef pop
    #undef push
}

typedef struct {
    State** s;
    int len;
} List;

int
match(char* regex, char* string) {
    char* postfix = regexToPostfix(regex);
    State* pStart = buildNFA(postfix);
    for (; *string; string++) {
        if (*string == pStart->c) {

        } else {
            break;
        }
    }

    free(postfix);
    return 0;
}

int main() {
    // Get input.
    char regex[MAX_STRING_LENGTH]; 
    char string[MAX_STRING_LENGTH]; 
    printf("Enter regular expression:\n");
    scanf("%s", regex);
    printf("Enter string to match:\n");
    scanf("%s", string);

    numState = 0;
    match(regex, string);

    printf("Done.\n");
    return 0;
}
