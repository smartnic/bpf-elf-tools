#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libelf.h>
#include <gelf.h>

#include "bpf_load.h"
#include <string.h>

void interpret_maps(struct bpf_map_data** maps, int nr_map);
void interpret_bpf_insns (struct bpf_insn**, int);
void write_insns (struct bpf_insn**, int, char*);
void read_insns(char *); 
void fix_progname(char *, char *);
void prepend_ins_path(char *, char *);
void prepend_map_path(char *, char *);
void write_maps(struct bpf_map_data**, int, char*);

int main (int argc, char ** argv)
{
    if (argc != 3) {
        printf("Usage: elf_parse </path/to/prog.o> <progname>\n");
        return 1;
    }

    char* filename = argv[1];
    char* progname = argv[2];
    char full_progname[50];
    char fixed_progname[50];
    struct bpf_insn* prog_insns = '\0';
    struct bpf_map_data* map_data = '\0';
    int prog_len, ret, nr_map;

    if (load_bpf_file(filename)) {

	printf("Program load failed\n");
	return 1;
    }
    printf("Load success\n");

    if ((ret = get_prog(filename, progname, strlen(progname),  &prog_insns, &map_data, &prog_len, &nr_map))) {
        printf("Program load failed: %d\n", ret);
        return ret;
    }
    else {
     	printf("Num maps: %d\n", nr_map);
    	printf("Prog len: %d\n", prog_len);
        if (prog_insns != '\0') {
        	interpret_bpf_insns(&prog_insns, prog_len);
            fix_progname(progname, fixed_progname);
            prepend_ins_path(fixed_progname, full_progname);
            write_insns(&prog_insns, prog_len, full_progname);
            read_insns(full_progname);
        }

        fix_progname(progname, fixed_progname);
        prepend_map_path(fixed_progname, full_progname);
     	interpret_maps(&map_data, nr_map);
        write_maps(&map_data, nr_map, full_progname);
    }
    return 0;
}

void interpret_maps(struct bpf_map_data** maps, int nr_map) 
{

    int i;
    for (i = 0; i < nr_map; i++) {
        struct bpf_map_data map = (*maps)[i];
        printf("Type: %u, name: %s, key_size: %u, value_size: %u, max_entries: %u, file descriptor: %d\n", 
            map.def.type, map.name, map.def.key_size, map.def.value_size, 
            map.def.max_entries, map.fd); 
	}
}
void write_maps(struct bpf_map_data** maps, int nr_map, char* full_progname) 
{
    FILE *fp;
    printf("Writing maps to %s\n", full_progname);
    fp = fopen(full_progname, "w");

    int i;
    for (i = 0; i < nr_map; i++) {
        struct bpf_map_data map = (*maps)[i];
        fprintf(fp, "%s { %s = %d, %s = %u, %s = %u, %s = %u, %s = %d }\n",
            map.name, "type", map.def.type, "key_size", 
            map.def.key_size, "value_size", map.def.value_size, 
            "max_entries", map.def.max_entries, "fd", map.fd); 
	}
    fclose(fp);
}

void interpret_bpf_insns(struct bpf_insn ** prog, int prog_len) 
{
    int i;
    printf("eBPF instructions:\n");
    for (i = 0; i < prog_len / sizeof(struct bpf_insn); ++i) {
        struct bpf_insn insn = (*prog)[i];
        // op:8, dst_reg:4, src_reg:4, off:16, imm:32
        printf("%d: op - %02x, src reg - %01x, dst reg - %01x, off - %04x, imm - %08x\t\n", 
            i, insn.code, insn.src_reg, insn.dst_reg, insn.off, insn.imm);
    }
}

void write_insns(struct bpf_insn ** prog, int prog_len, char* full_progname) 
{
    FILE *fp;
    printf("Writing to %s\n", full_progname);

    fp = fopen(full_progname, "w");
    int i;
    struct bpf_insn insn;
    for (i = 0; i < prog_len / sizeof(struct bpf_insn); ++i) {
        insn = (*prog)[i];
        struct bpf_insn test_insn = { insn.code, insn.src_reg, insn.dst_reg, insn.off, insn.imm };
        // op:8, dst_reg:4, src_reg:4, off:16, imm:32
        fwrite(&test_insn, sizeof(struct bpf_insn), 1, fp);
    }
    fclose(fp); 
}

void read_insns(char* full_progname) 
{ 
    FILE *fp;
    printf("Reading from %s\n", full_progname);

    struct bpf_insn input; 
    // Open person.dat for reading 
    fp = fopen (full_progname, "r"); 
    // read file contents till end of file 
    while(fread(&input, sizeof(struct bpf_insn), 1, fp)) 
      printf("op - %02x, src reg - %01x, dst reg - %01x, off - %04x, imm - %08x\t\n", 
        input.code, input.dst_reg, input.src_reg, input.off, input.imm); 
  
    // close file 
    fclose(fp); 
}
void fix_progname(char* progname, char* fixed_progname) 
{
    char buf[50];
    for (int i = 0; i < strlen(progname); ++i) {
        if (progname[i] == '/') buf[i] = '-';
        else buf[i] = progname[i];
    }
    buf[strlen(progname)] = '\0';
    printf("Buf = %s\n", buf);
    strcpy(fixed_progname, buf);
}
 
void prepend_ins_path(char* progname, char* full_progname) 
{
    char buf[60];
    snprintf(buf, 60, "../out/%s.ins", progname); 
    strcpy(full_progname, buf);

}
void prepend_map_path(char* progname, char* full_progname) 
{
    char buf[60];
    snprintf(buf, 60, "../out/%s.maps", progname); 
    strcpy(full_progname, buf);

}