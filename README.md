
# BPF loader

BPF program loader using the code found in the libbpf repository (https://github.com/libbpf/libbpf). 

Installation:

```cd src```
```make```
```gcc staticobjs/* -lelf -lz -o elf_parse```

Usage:

```./elf_parse <path_to_file> <progname>```

The output will result in a .ins file containing the BPF instructions and a .maps file containing the BPF maps defined. Example BPF programs can be found in https://github.com/smartnic/bpf-benchmarks/tree/master/ebpf-samples.

