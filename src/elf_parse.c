#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libelf.h>
#include <gelf.h>

#include "bpf_load.h"
#include <string.h>

void interpret_maps(struct bpf_map_data** maps, int nr_map);
void interpret_bpf_insns (struct bpf_insn**, int);
void write_insns (struct bpf_insn**, int, char*, char*);
void read_insns(char *); 
void fix_progname(char *, char *);
void prepend_ins_path(char *, char *);
void prepend_map_path(char *, char *);
void write_maps(struct bpf_map_data**, int, char*);

int main (int argc, char ** argv)
{
    if (argc != 4) {
        printf("Usage: elf_parse </path/to/prog.o> <progname> <output name>\n");
        return 1;
    }

    char* filename = argv[1];
    char* progname = argv[2];
    char* output_name = argv[3];

    char output_with_extension[50];
    char fixed_name[50];
    struct bpf_insn* prog_insns = '\0';
    struct bpf_map_data* map_data = '\0';
    int prog_len, ret, nr_map;


    if ((ret = get_prog(filename, progname, strlen(progname),  &prog_insns, &map_data, &prog_len, &nr_map))) {
        if ((ret = load_prog_without_maps(filename, progname, strlen(progname), &prog_len, &prog_insns))) {
            printf("Program load failed with error code: %d\n", ret);
            return ret;
        }
    }
    if (prog_insns != '\0') {
        interpret_bpf_insns(&prog_insns, prog_len);
        fix_progname(output_name, fixed_name);
        prepend_ins_path(fixed_name, output_with_extension);
        write_insns(&prog_insns, prog_len, fixed_name, output_with_extension);
    }
    else {
        printf("program insns could not be detected. Is the program name correct?\n");
        return 1;
    }
    if (map_data != '\0') {
        fix_progname(output_name, fixed_name);
        prepend_map_path(fixed_name, output_with_extension);
        interpret_maps(&map_data, nr_map);
        write_maps(&map_data, nr_map, output_with_extension);
    }
    else {
        fix_progname(output_name, fixed_name);
        prepend_map_path(fixed_name, output_with_extension);
        write_maps('\0', 0, output_with_extension);
        return 1;        
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

    if (maps != '\0' && nr_map > 0) {
        int i;
        for (i = 0; i < nr_map; i++) {
            struct bpf_map_data map = (*maps)[i];
            fprintf(fp, "%s { %s = %d, %s = %u, %s = %u, %s = %u, %s = %d }\n",
                map.name, "type", map.def.type, "key_size", 
                map.def.key_size, "value_size", map.def.value_size, 
                "max_entries", map.def.max_entries, "fd", map.fd); 
        }
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
        printf("%d: {%d, %d, %d, %d, %d}\n", 
            i, insn.code, insn.src_reg, insn.dst_reg, insn.off, insn.imm);
    }
}

void write_insns(struct bpf_insn ** prog, int prog_len, char* progname, char* full_progname) 
{
    FILE *bytecode_fp;
    FILE *insns_fp;
    char insns_output[60];
    snprintf(insns_output, 60, "%s.txt", progname); 

    printf("Writing bytecode to %s\n", full_progname);
    printf("Writing instruction vectors to %s\n", insns_output);

    bytecode_fp = fopen(full_progname, "w");
    insns_fp = fopen(insns_output, "w");
    int i;
    struct bpf_insn insn;
    for (i = 0; i < prog_len / sizeof(struct bpf_insn); ++i) {
        insn = (*prog)[i];
        struct bpf_insn test_insn = { insn.code, insn.src_reg, insn.dst_reg, insn.off, insn.imm };
        // op:8, dst_reg:4, src_reg:4, off:16, imm:32
        fwrite(&test_insn, sizeof(struct bpf_insn), 1, bytecode_fp);
        fprintf(insns_fp, "{%d, %d, %d, %d, %d}, \n", insn.code, insn.dst_reg, insn.src_reg, insn.off, insn.imm);
    }
    fclose(bytecode_fp); 
    fclose(insns_fp);
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
    printf("buf = %s\n", buf);
    strcpy(fixed_progname, buf);
}
 
void prepend_ins_path(char* progname, char* full_progname) 
{
    char buf[60];
    snprintf(buf, 60, "%s.insns", progname); 
    strcpy(full_progname, buf);

}
void prepend_map_path(char* progname, char* full_progname) 
{
    char buf[60];
    snprintf(buf, 60, "%s.maps", progname); 
    strcpy(full_progname, buf);

}
