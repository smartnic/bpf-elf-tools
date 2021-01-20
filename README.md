
# BPF ELF tools

Tools to operate on compiled ELF files for Superopt. Currently this repository consists of two tools: text extractor and ELF patcher. 
The text extractor is a tool for retrieving the instructions, map data, and relocation data from an ELF file. 
There are currently two text extractors; the old one uses bpf_load and the new one the libbpf repository (https://github.com/libbpf/libbpf). 
The old text extractor is obselete and doesn't work for files generated with newer versions of Clang; 
the libbpf text extractor in `text-extractor` should always be used.
The ELF patcher replaces a current section in the ELF with a new sequence of instructions. 
Further information can be found in the README inside of `patch_insns`.

Installation:

`cd text-extractor`

`make && gcc staticobjs/* -lelf -lz -o elf_extract`

To build the old text extractor, perform the same steps in the old-text-extractor directory.

Usage:

```./elf_extract <path_to_file> <progname> <output_file_name>```

