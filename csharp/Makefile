# Makefile for FAudio#
# Written by Ethan "flibitijibibo" Lee

build: clean
	mkdir bin
	dmcs /unsafe -debug -out:bin/FAudio-CS.dll -target:library FAudio.cs

clean:
	rm -rf bin
