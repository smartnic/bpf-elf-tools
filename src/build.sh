#!/bin/bash

#make clean
make
gcc staticobjs/* -lelf -lz -o elf_parse 
