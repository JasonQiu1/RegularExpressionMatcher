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
#include <string.h>

// Keeps track of the number of states to allocate memory when interpreting.
int numState;

//  Represents the states in the NFA.
//  'c' represents the value of the state, which is 0-255 for matching a
//	character, 256 to represent the 'Match' state, or 257 to represent a
//	state that splits into two states, that may or may not include itself.
//  'pOut' points to the next state.
//  'pOutSplit' points to the other state of a 'Split' state.
//  'lastListID' is used in addState to prevent duplicates when interpreting
//  the NFA to prevent duplicates.
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

// Builds an NFA from postfix regex.
// Returns the starting state.
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

// Incrememnt listID when a list is created.
static int listID;
typedef struct {
    State** ppStates;
    int len;
} List;

// Empty list constructor.
List*
pEmptyList(List* l, int size) {
    //l->ppStates = (State**)realloc(l->ppStates, sizeof(State*) * size);
    l->len = 0;
    listID++;
    return l;
}

// Add a state to the list if it was not already added this step.
void
addState(List* l, State* s) {
    if (s == NULL || s->lastListID == listID) {
        return;
    }

    s->lastListID = listID;
    if (s->c == Split) {
        addState(l, s->pOut);
        addState(l, s->pOutSplit);
    } else {
        l->ppStates[l->len++] = s;
    }
}

// List constructor.
List*
pList(List* l, State* pStart, int size) {
    pEmptyList(l, size);
    addState(l, pStart);
    return l;
}

// Stores the matching string.
char* matchedString; 
int matchLength;
// Add the outs of the states in pCurrStates that match c to pNextStates.
void
step(List* pCurrStates, List* pNextStates, int c) {
    int i;
    State* pCurrState;
    listID++;
    pNextStates->len = 0;
    for (i = 0; i < pCurrStates->len; i++) {
        pCurrState = pCurrStates->ppStates[i];
        if (pCurrState->c == '.' || pCurrState->c == c) {
            if (pCurrState->c < 256) 
                matchedString[matchLength++] = pCurrState->c;
            addState(pNextStates, pCurrState->pOut);
        }
    }
}

int checkMatch(List* pFinalStates) {
    int i;
    for (i = 0; i < pFinalStates->len; i++) {
        if (pFinalStates->ppStates[i]->c == Match) {
            return 1;
        }
    }
    return 0;
}

// Holds current and next states in the NFA.
List* pCurrStates;
List* pNextStates;
// Free all states in the NFA by walking through all states.
void freeStates(State* pStart) {
    pCurrStates = pList(pCurrStates, pStart, numState);
    pNextStates = pEmptyList(pNextStates, numState);
    State* s;
    int i;
    List* pTemp;

    while (pCurrStates->len > 0) {
        for (i = 0; i < pCurrStates->len; i++) {
            s = pCurrStates->ppStates[i];
            if (!s) continue;

            addState(pNextStates, s->pOut);
            if (s->c == Split) {
                addState(pNextStates, s->pOutSplit);
            }
            
            free(s);
            s = NULL;
        }
        pTemp = pCurrStates;
        pCurrStates = pNextStates;
        pNextStates = pTemp;
    }
}

// Match a regex against a string by building and interpreting an NFA.
int
match(char* regex, char* string) {
    char* postfix = regexToPostfix(regex);

    listID = 0;
    numState = 0;
    matchLength = 0;
    State* pStart = buildNFA(postfix);

    pCurrStates = pList(pCurrStates, pStart, numState);
    pNextStates = pEmptyList(pNextStates, numState);
    List* pTemp;

    for (; *string; string++) {
        step(pCurrStates, pNextStates, *string);
        // Break early if no more states to follow.
        if (!pNextStates->len) {
            break;    
        }
        
        // Avoid reallocating memory by swapping the buffers.
        pTemp = pCurrStates;
        pCurrStates = pNextStates;
        pNextStates = pTemp;
    }

    matchedString[matchLength] = '\0';
    int result = checkMatch(pCurrStates);

    // Free memory that may be leaked when trying another match.
    free(postfix);
    //freeStates(pStart);
    return result;
}

void printHelpLine(const char* cmd, const char* desc) {
    static const char* indent = "  ";
    printf("%s%-35s%s\n", indent, cmd, desc);
}

int main(int argc, char* argv[]) {
    char regex[MAX_STRING_LENGTH]; 
    char string[MAX_STRING_LENGTH]; 

    matchedString = (char*)malloc(sizeof(char) * MAX_STRING_LENGTH);
    pCurrStates = (List*)malloc(sizeof(List));
    pCurrStates->ppStates = (State**)malloc(sizeof(State*)*MAX_STRING_LENGTH);
    pNextStates = (List*)malloc(sizeof(List));
    pNextStates->ppStates = (State**)malloc(sizeof(State*)*MAX_STRING_LENGTH);
    // Run interactive mode if no args
    if (argc == 1) {
        char doAgain[1]; 
        do {
            printf("Enter regular expression:\n");
            scanf("%s", regex);
            printf("Enter string to match:\n");
            scanf("%s", string);

            if (match(regex, string)) {
                printf("Match found: %s\n", matchedString);
            } else {
                printf("Match not found.\n");
            }

            printf("Again? (y/n): ");
            scanf("%s", doAgain);
            printf("\n");
        } while (strcmp(doAgain,"y") == 0);
    } else if (argc == 3) {
        strcpy(regex, argv[1]);
        strcpy(string, argv[2]);
        if (match(regex, string)) {
            printf("%s", matchedString);
        } 
    } else if (argc == 4 && strcmp(argv[1], "-f") == 0) {
        // For regexes on each line of a file, 
        // group each regex (separated by line break or space), 
        // and join them with alternation ('|').
        FILE* regexFile = fopen(argv[2], "r");
        int len = 0;
        char re[MAX_STRING_LENGTH];
        while (fscanf(regexFile, "%s", re) != EOF) {
            regex[len++] = '(';
            strcpy(&regex[len], re);
            len += strlen(re);
            regex[len++] = ')';
            regex[len++] = '|';
        }
        regex[--len] = '\0';
        strcpy(string, argv[3]);
        if (match(regex, string)) {
            printf("%s", matchedString);
        } 
    } else {
        printf("Usage: %s [args] <string>\n", argv[0]);
        printf("Args:");
        printHelpLine("(no args)", "Interactive mode.");
        printHelpLine("<regex>", "Match one regex pattern against a string.");
        printHelpLine("-f <regexFile>", "Match a file of regexes against a string.");
    }

    return 0;
}
