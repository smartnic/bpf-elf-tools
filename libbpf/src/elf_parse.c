#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libelf.h>
#include <gelf.h>

#include "libbpf.h"
#include <string.h>

int main(int argc, char** argv) 
{
    extract();
    printf("Elf parse\n");
    return 0;
}
