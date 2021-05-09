
# Text extractor

The text extractor is a tool for retrieving the instructions, map data, and relocation data from an ELF file. Generates separate files for each BPF program inside of the ELF.

Installation:

1. Install libelf-dev

`sudo apt-get install libelf-dev`

2. Run make and compile

`make && gcc staticobjs/* -lelf -lz -o elf_extract`

To build the old text extractor, perform the same steps in the old-text-extractor directory.

Usage:

```./elf_extract <path_to_file> ```

