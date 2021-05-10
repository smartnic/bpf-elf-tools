
# Text extractor

The text extractor is a tool for retrieving the instructions and map data from an ELF file. 

Installation:

`make && gcc staticobjs/* -lelf -lz -o elf_extract`

Usage:

```./elf_extract <path_to_file> <progname> <output name>```

