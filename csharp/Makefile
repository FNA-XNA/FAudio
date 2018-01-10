# Makefile for FACT#
# Written by Ethan "flibitijibibo" Lee

build: clean
	mkdir bin
	cp FACT-CS.dll.config bin
	dmcs /unsafe -debug -out:bin/FACT-CS.dll -target:library FACT.cs

clean:
	rm -rf bin
