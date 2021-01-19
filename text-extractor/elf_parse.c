#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libelf.h>
#include <gelf.h>

#include "libbpf.h"
#include <string.h>

int main(int argc, char** argv) 
{
    if (argc != 4) {
        printf("Usage: elf_parse </path/to/prog.o> <progname> <output name>\n");
        return 1;
    }
    char* file_name = argv[1];
    char* prog_name = argv[2];
    char* output_name = argv[3];
    extract(file_name, prog_name, output_name);
    return 0;
}
