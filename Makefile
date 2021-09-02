.PHONY: run compile debug

everytime: compile

run: compile
	winpty ./regExpFA

debug: compile
	gdb ./regExpFA.exe

compile:
	gcc -g -Wall -std=c99 regExpFA.c -o regExpFA
