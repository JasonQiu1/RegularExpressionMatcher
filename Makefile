.PHONY: run compile

everytime: compile

run: compile
	winpty ./regExpFA

compile:
	gcc -g -Wall -std=c99 regExpFA.c -o regExpFA
