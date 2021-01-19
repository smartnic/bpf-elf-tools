
# BPF ELF tools

Tools to operate on compiled ELF files for Superopt. Currently this repository consists of text extractor, a tool for
retrieving the instructions, map data, and relocation data from an ELF file. The text extractor uses the libbpf repository 
(https://github.com/libbpf/libbpf). 

Installation:

`cd text-extractor`

`make && gcc staticobjs/* -lelf -lz -o elf_extract`

To build the old text extractor, perform the same steps in the old-text-extractor directory.

Usage:

```./elf_extract <path_to_file> <progname> <output_file_name>```

