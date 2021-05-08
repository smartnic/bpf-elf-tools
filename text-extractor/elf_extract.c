#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libelf.h>
#include <gelf.h>

#include "libbpf.h"
#include <string.h>

int main(int argc, char** argv) 
{
    if (argc != 2) {
        printf("Usage: elf_parse </path/to/prog.o>\n");
        return 1;
    }
    char* file_name = argv[1];
    extract(file_name);
    return 0;
}
