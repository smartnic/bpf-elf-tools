// SPDX-License-Identifier: GPL-2.0
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libelf.h>
#include <gelf.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <linux/bpf.h>
#include <linux/filter.h>
#include <linux/perf_event.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/types.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <poll.h>
#include <ctype.h>
#include <assert.h>
#include "bpf.h"
#include "bpf_load.h"
#include "perf-sys.h"

#define DEBUGFS "/sys/kernel/debug/tracing/"

static char license[128];
static int kern_version;
static bool processed_sec[128];
char bpf_log_buf[BPF_LOG_BUF_SIZE];
int map_fd[MAX_MAPS];
int prog_fd[MAX_PROGS];
int event_fd[MAX_PROGS];
int prog_cnt;
int prog_array_fd = -1;

struct bpf_map_data map_data[MAX_MAPS];
int map_data_count;
int MAP_FD = 0;

static int write_kprobe_events(const char *val)
{
	int fd, ret, flags;

	if (val == NULL)
		return -1;
	else if (val[0] == '\0')
		flags = O_WRONLY | O_TRUNC;
	else
		flags = O_WRONLY | O_APPEND;

	fd = open(DEBUGFS "kprobe_events", flags);

	ret = write(fd, val, strlen(val));
	close(fd);

	return ret;
}


static int load_maps(struct bpf_map_data *maps, int nr_maps,
		     fixup_map_cb fixup_map)
{
	int i;

	for (i = 0; i < nr_maps; i++) {
		if (fixup_map) {
			fixup_map(&maps[i], i);
			/* Allow userspace to assign map FD prior to creation */
			if (maps[i].fd != -1) {
				map_fd[i] = maps[i].fd;
				continue;
			}
		}

        map_fd[i] = MAP_FD++;
		if (map_fd[i] < 0) {
			printf("failed to create map %d (%s): %d %s\n",
			       i, maps[i].name, errno, strerror(errno));
			return 1;
		}
		maps[i].fd = map_fd[i];

		if (maps[i].def.type == BPF_MAP_TYPE_PROG_ARRAY)
			prog_array_fd = map_fd[i];
	}
	return 0;
}

static int get_sec(Elf *elf, int i, GElf_Ehdr *ehdr, char **shname,
		   GElf_Shdr *shdr, Elf_Data **data)
{
	Elf_Scn *scn;

	scn = elf_getscn(elf, i);
	if (!scn)
		return 1;

	if (gelf_getshdr(scn, shdr) != shdr)
		return 2;

	*shname = elf_strptr(elf, ehdr->e_shstrndx, shdr->sh_name);
	if (!*shname || !shdr->sh_size)
		return 3;

	*data = elf_getdata(scn, 0);
	if (!*data || elf_getdata(scn, *data) != NULL)
		return 4;

	return 0;
}

static int parse_relo_and_apply(Elf_Data *data, Elf_Data *symbols,
				GElf_Shdr *shdr, struct bpf_insn *insn,
				struct bpf_map_data *maps, int nr_maps)
{
	int i, nrels;

	nrels = shdr->sh_size / shdr->sh_entsize;

	for (i = 0; i < nrels; i++) {
		GElf_Sym sym;
		GElf_Rel rel;
		unsigned int insn_idx;
		bool match = false;
//		int j,
		int  map_idx;

		gelf_getrel(data, i, &rel);

		insn_idx = rel.r_offset / sizeof(struct bpf_insn);

		gelf_getsym(symbols, GELF_R_SYM(rel.r_info), &sym);

		if (insn[insn_idx].code != (BPF_LD | BPF_IMM | BPF_DW)) {
			printf("invalid relo for insn[%d].code 0x%x\n",
			       insn_idx, insn[insn_idx].code);
			return 1;
		}
		insn[insn_idx].src_reg = BPF_PSEUDO_MAP_FD;

		/* Match FD relocation against recorded map_data[] offset */
		for (map_idx = 0; map_idx < nr_maps; map_idx++) {
			if (maps[map_idx].elf_offset == sym.st_value) {
				match = true;
				break;
			}
		}
		if (match) {
			insn[insn_idx].imm = maps[map_idx].fd;
		} else {
			printf("invalid relo for insn[%d] no map_data match\n",
			       insn_idx);
			return 1;
		}
	}

	return 0;
}

static int cmp_symbols(const void *l, const void *r)
{
	const GElf_Sym *lsym = (const GElf_Sym *)l;
	const GElf_Sym *rsym = (const GElf_Sym *)r;

	if (lsym->st_value < rsym->st_value)
		return -1;
	else if (lsym->st_value > rsym->st_value)
		return 1;
	else
		return 0;
}

static int load_elf_maps_section(struct bpf_map_data *maps, int maps_shndx,
				 Elf *elf, Elf_Data *symbols, int strtabidx)
{
	int map_sz_elf, map_sz_copy;
	bool validate_zero = false;
	Elf_Data *data_maps;
	int i, nr_maps;
	GElf_Sym *sym;
	Elf_Scn *scn;
	//int copy_sz;

	if (maps_shndx < 0)
		return -EINVAL;
	if (!symbols)
		return -EINVAL;

	/* Get data for maps section via elf index */
	scn = elf_getscn(elf, maps_shndx);
	if (scn)
		data_maps = elf_getdata(scn, NULL);
	if (!scn || !data_maps) {
		printf("Failed to get Elf_Data from maps section %d\n",
		       maps_shndx);
		return -EINVAL;
	}

	/* For each map get corrosponding symbol table entry */
	sym = calloc(MAX_MAPS+1, sizeof(GElf_Sym));
	for (i = 0, nr_maps = 0; i < symbols->d_size / sizeof(GElf_Sym); i++) {
		assert(nr_maps < MAX_MAPS+1);
		if (!gelf_getsym(symbols, i, &sym[nr_maps]))
			continue;
		if (sym[nr_maps].st_shndx != maps_shndx)
			continue;
		/* Only increment iif maps section */
		nr_maps++;
	}

	/* Align to map_fd[] order, via sort on offset in sym.st_value */
	qsort(sym, nr_maps, sizeof(GElf_Sym), cmp_symbols);

	/* Keeping compatible with ELF maps section changes
	 * ------------------------------------------------
	 * The program size of struct bpf_load_map_def is known by loader
	 * code, but struct stored in ELF file can be different.
	 *
	 * Unfortunately sym[i].st_size is zero.  To calculate the
	 * struct size stored in the ELF file, assume all struct have
	 * the same size, and simply divide with number of map
	 * symbols.
	 */
	map_sz_elf = data_maps->d_size / nr_maps;
	map_sz_copy = sizeof(struct bpf_load_map_def);
	if (map_sz_elf < map_sz_copy) {
		/*
		 * Backward compat, loading older ELF file with
		 * smaller struct, keeping remaining bytes zero.
		 */
		map_sz_copy = map_sz_elf;
	} else if (map_sz_elf > map_sz_copy) {
		/*
		 * Forward compat, loading newer ELF file with larger
		 * struct with unknown features. Assume zero means
		 * feature not used.  Thus, validate rest of struct
		 * data is zero.
		 */
		validate_zero = true;
	}

	/* Memcpy relevant part of ELF maps data to loader maps */
	for (i = 0; i < nr_maps; i++) {
		struct bpf_load_map_def *def;
		unsigned char *addr, *end;
		const char *map_name;
		size_t offset;

		map_name = elf_strptr(elf, strtabidx, sym[i].st_name);
		maps[i].name = strdup(map_name);
		if (!maps[i].name) {
			printf("strdup(%s): %s(%d)\n", map_name,
			       strerror(errno), errno);
			free(sym);
			return -errno;
		}

		/* Symbol value is offset into ELF maps section data area */
		offset = sym[i].st_value;
		def = (struct bpf_load_map_def *)(data_maps->d_buf + offset);
		maps[i].elf_offset = offset;
		memset(&maps[i].def, 0, sizeof(struct bpf_load_map_def));
		memcpy(&maps[i].def, def, map_sz_copy);

		/* Verify no newer features were requested */
		if (validate_zero) {
			addr = (unsigned char *) def + map_sz_copy;
			end  = (unsigned char *) def + map_sz_elf;
			for (; addr < end; addr++) {
				if (*addr != 0) {
					free(sym);
					return -EFBIG;
				}
			}
		}
	}

	free(sym);
	return nr_maps;
}

static int do_load_bpf_file(const char *path, fixup_map_cb fixup_map)
{
	int fd, i, ret, maps_shndx = -1, strtabidx = -1;
	Elf *elf;
	GElf_Ehdr ehdr;
	GElf_Shdr shdr, shdr_prog;
	Elf_Data *data, *data_prog, *data_maps = NULL, *symbols = NULL;
	char *shname, *shname_prog;
	int nr_maps = 0;

	/* reset global variables */
	kern_version = 0;
	memset(license, 0, sizeof(license));
	memset(processed_sec, 0, sizeof(processed_sec));

	if (elf_version(EV_CURRENT) == EV_NONE)
		return 1;

	fd = open(path, O_RDONLY, 0);
	if (fd < 0)
		return 1;

	elf = elf_begin(fd, ELF_C_READ, NULL);

	if (!elf)
		return 1;

	if (gelf_getehdr(elf, &ehdr) != &ehdr)
		return 1;

	/* clear all kprobes */
	i = write_kprobe_events("");

	/* scan over all elf sections to get license and map info */
	for (i = 1; i < ehdr.e_shnum; i++) {

		if (get_sec(elf, i, &ehdr, &shname, &shdr, &data))
			continue;

		if (0) /* helpful for llvm debugging */
			printf("section %d:%s data %p size %zd link %d flags %d\n",
			       i, shname, data->d_buf, data->d_size,
			       shdr.sh_link, (int) shdr.sh_flags);

		if (strcmp(shname, "license") == 0) {
			processed_sec[i] = true;
			memcpy(license, data->d_buf, data->d_size);
		} else if (strcmp(shname, "version") == 0) {
			processed_sec[i] = true;
			if (data->d_size != sizeof(int)) {
				printf("invalid size of version section %zd\n",
				       data->d_size);
				return 1;
			}
			memcpy(&kern_version, data->d_buf, sizeof(int));
		} else if (strcmp(shname, "maps") == 0) {
			int j;

			maps_shndx = i;
			data_maps = data;
			for (j = 0; j < MAX_MAPS; j++)
				map_data[j].fd = -1;
		} else if (shdr.sh_type == SHT_SYMTAB) {
			strtabidx = shdr.sh_link;
			symbols = data;
		}
	}

	ret = 1;

	if (!symbols) {
		goto done;
	}

	if (data_maps) {
		nr_maps = load_elf_maps_section(map_data, maps_shndx,
						elf, symbols, strtabidx);
		if (nr_maps < 0) {
			printf("Error: Failed loading ELF maps (errno:%d):%s\n",
			       nr_maps, strerror(-nr_maps));
			goto done;
		}
		if (load_maps(map_data, nr_maps, fixup_map)) {
			goto done;
        }
		map_data_count = nr_maps;

		processed_sec[maps_shndx] = true;
	}

	/* process all relo sections, and rewrite bpf insns for maps */
	for (i = 1; i < ehdr.e_shnum; i++) {
		if (processed_sec[i])
			continue;

		if (get_sec(elf, i, &ehdr, &shname, &shdr, &data))
			continue;

		if (shdr.sh_type == SHT_REL) {
			struct bpf_insn *insns;

			/* locate prog sec that need map fixup (relocations) */
			if (get_sec(elf, shdr.sh_info, &ehdr, &shname_prog,
				    &shdr_prog, &data_prog))
				continue;

			if (shdr_prog.sh_type != SHT_PROGBITS ||
			    !(shdr_prog.sh_flags & SHF_EXECINSTR))
				continue;

			insns = (struct bpf_insn *) data_prog->d_buf;
			processed_sec[i] = true; /* relo section */

			if (parse_relo_and_apply(data, symbols, &shdr, insns,
						 map_data, nr_maps)) {
				continue;
			}
		}
	}

	/* load programs */
	for (i = 1; i < ehdr.e_shnum; i++) {

		if (processed_sec[i])
			continue;

		if (get_sec(elf, i, &ehdr, &shname, &shdr, &data))
			continue;
        
		if (memcmp(shname, "kprobe/", 7) == 0 ||
		    memcmp(shname, "kretprobe/", 10) == 0 ||
		    memcmp(shname, "tracepoint/", 11) == 0 ||
		    memcmp(shname, "raw_tracepoint/", 15) == 0 ||
		    memcmp(shname, "xdp", 3) == 0 ||
		    memcmp(shname, "perf_event", 10) == 0 ||
		    memcmp(shname, "socket", 6) == 0 ||
		    memcmp(shname, "cgroup/", 7) == 0 ||
		    memcmp(shname, "sockops", 7) == 0 ||
		    memcmp(shname, "sk_skb", 6) == 0 ||
		    memcmp(shname, "sk_msg", 6) == 0) {
            ret = 0;
			if (ret != 0) {
				goto done;
            }
		}
	}

done:
	close(fd);
	return ret;
}
int get_prog(const char *path,  char *progname, int progname_len,
		struct bpf_insn** prog_insns, struct bpf_map_data** t_map_data, int* prog_len, int* t_nr_maps)
{
	int fd, i, ret, maps_shndx = -1, strtabidx = -1;
	Elf *elf;
	GElf_Ehdr ehdr;
	GElf_Shdr shdr, shdr_prog;
	Elf_Data *data, *data_prog, *data_maps = NULL, *symbols = NULL;
	char *shname, *shname_prog;
	int nr_maps = 0;

	/* reset global variables */
	kern_version = 0;
	memset(license, 0, sizeof(license));
	memset(processed_sec, 0, sizeof(processed_sec));

	if (elf_version(EV_CURRENT) == EV_NONE) {
		return 1;
    }

	fd = open(path, O_RDONLY, 0);
	if (fd < 0) {
		return 1;
    }

	elf = elf_begin(fd, ELF_C_READ, NULL);

	if (!elf) {
		return 1;
    }

	if (gelf_getehdr(elf, &ehdr) != &ehdr) {
		return 1;
    }

	/* clear all kprobes */
	i = write_kprobe_events("");
	/* scan over all elf sections to get license and map info */
	for (i = 1; i < ehdr.e_shnum; i++) {

		if (get_sec(elf, i, &ehdr, &shname, &shdr, &data))
			continue;

		if (0) /* helpful for llvm debugging */
			printf("section %d:%s data %p size %zd link %d flags %d\n",
			       i, shname, data->d_buf, data->d_size,
			       shdr.sh_link, (int) shdr.sh_flags);

		if (strcmp(shname, "license") == 0) {
			processed_sec[i] = true;
			memcpy(license, data->d_buf, data->d_size);
		} else if (strcmp(shname, "version") == 0) {
			processed_sec[i] = true;
			if (data->d_size != sizeof(int)) {
				printf("invalid size of version section %zd\n",
				       data->d_size);
				return 1;
			}
			memcpy(&kern_version, data->d_buf, sizeof(int));
		} else if (strcmp(shname, "maps") == 0) {
			int j;
			maps_shndx = i;
			data_maps = data;
			for (j = 0; j < MAX_MAPS; j++)
				map_data[j].fd = -1;
		} else if (shdr.sh_type == SHT_SYMTAB) {
			strtabidx = shdr.sh_link;
			symbols = data;
		}
	}

	ret = 1;

	if (!symbols) {
		printf("missing SHT_SYMTAB section\n");
		goto done;
	}

	if (data_maps) {
		nr_maps = load_elf_maps_section(map_data, maps_shndx,
						elf, symbols, strtabidx);
		if (nr_maps < 0) {
			printf("Error: Failed loading ELF maps (errno:%d):%s\n",
			       nr_maps, strerror(-nr_maps));
			goto done;
		}
		if (load_maps(map_data, nr_maps, NULL)) // fixup_map))
			goto done;
		else {
			*t_nr_maps = nr_maps;
			*t_map_data = map_data;	
		}
		map_data_count = nr_maps;

		processed_sec[maps_shndx] = true;
	}

	/* process all relo sections, and rewrite bpf insns for maps */
	for (i = 1; i < ehdr.e_shnum; i++) {
		if (processed_sec[i])
			continue;

		if (get_sec(elf, i, &ehdr, &shname, &shdr, &data))
			continue;

		if (0) { /* helpful for llvm debugging */
			printf("section %d:%s data %p size %zd link %d flags %d, shdrtype %d\n",
			       i, shname, data->d_buf, data->d_size,
			       shdr.sh_link, (int) shdr.sh_flags, shdr.sh_type);

        }


		if (shdr.sh_type == SHT_REL) {
			struct bpf_insn *insns;

			/* locate prog sec that need map fixup (relocations) */
			if (get_sec(elf, shdr.sh_info, &ehdr, &shname_prog,
				    &shdr_prog, &data_prog)) {
				continue;
            }

			if (shdr_prog.sh_type != SHT_PROGBITS ||
			    !(shdr_prog.sh_flags & SHF_EXECINSTR)) {

				continue;
            }

			insns = (struct bpf_insn *) data_prog->d_buf;
			processed_sec[i] = true; /* relo section */

			if (parse_relo_and_apply(data, symbols, &shdr, insns,
						 map_data, nr_maps)) {
				continue;
			}
			if (memcmp(shname_prog, progname, progname_len) == 0) {
				*prog_len = data_prog->d_size;
				*prog_insns = insns;
				return 0;

			}
		}


	}

	/* process all relo sections, and rewrite bpf insns for maps */
	/* load programs */
	for (i = 1; i < ehdr.e_shnum; i++) {

		if (processed_sec[i])
			continue;

		if (get_sec(elf, i, &ehdr, &shname, &shdr, &data))
			continue;
        
		if (memcmp(shname, "kprobe/", 7) == 0 ||
		    memcmp(shname, "kretprobe/", 10) == 0 ||
		    memcmp(shname, "tracepoint/", 11) == 0 ||
		    memcmp(shname, "raw_tracepoint/", 15) == 0 ||
		    memcmp(shname, "xdp", 3) == 0 ||
		    memcmp(shname, "perf_event", 10) == 0 ||
		    memcmp(shname, "socket", 6) == 0 ||
		    memcmp(shname, "cgroup/", 7) == 0 ||
		    memcmp(shname, "sockops", 7) == 0 ||
		    memcmp(shname, "sk_skb", 6) == 0 ||
		    memcmp(shname, "sk_msg", 6) == 0) {
			if (ret != 0)
				goto done;
		}
	}
done:
	close(fd);
	return ret;
}

int load_bpf_file(char *path)
{
	return do_load_bpf_file(path, NULL);
}

int load_bpf_file_fixup_map(const char *path, fixup_map_cb fixup_map)
{
	return do_load_bpf_file(path, fixup_map);
}





int load_prog_without_maps(char *path, char *progname,
             int progname_len, int *prog_len,
             struct bpf_insn** prog )
{
	int fd, i;
	Elf *elf;
	GElf_Ehdr ehdr;
	GElf_Shdr shdr;
	Elf_Data *data;
	char *shname; 

	if (elf_version(EV_CURRENT) == EV_NONE)
		return 1;

	fd = open(path, O_RDONLY, 0);
	if (fd < 0)
		return 1;

	elf = elf_begin(fd, ELF_C_READ, NULL);

	if (!elf)
		return 1;

	if (gelf_getehdr(elf, &ehdr) != &ehdr)
		return 1;

	// load programs that don't use maps 
	for (i = 1; i < ehdr.e_shnum; i++) {

		if (processed_sec[i])
			continue;

		if (get_sec(elf, i, &ehdr, &shname, &shdr, &data))
			continue;

    if (memcmp(shname, progname, progname_len) == 0) {

      *prog_len = data->d_size;
      *prog = data->d_buf;
      return 0;
    }
	}
	close(fd);
	return 0;
}



