
# BPF ELF patcher

Creates a new ELF file with new instructions patched into a section of interest. Sections are copied into the new ELF section-by-section until the specified section of interest is encountered. Then, the instructions from a provided input file will be substituted instead.

### Setup

Install pyelftools

    pip3 install pyelftools

### Usage


    python3 patch_elf.py -h
    
    usage: patch_elf.py [-h] [-s SECTIONS] [-o OUTPUT]
                        elf_file new_insns section_name

    ELF patcher

    positional arguments:
      elf_file              ELF input file
      new_insns             File with new insns to patch in
      section_name          ELF section to patch

    optional arguments:
      -h, --help            show this help message and exit
      -s SECTIONS, --sections SECTIONS
                            Comma-separated list of ELF secitons preceding section
                            of interest
      -o OUTPUT, --output OUTPUT
                            Output file name



Example using sockex1:

    python3 patch_elf.py examples/sockex1_kern.o sample-insns/sockex1.insns socket1

Example using sockex2:

    python3 patch_elf.py examples/sockex2_kern.o sample-insns/sockex2.insns socket2

Example using bpf_network:
    
    python3 patch_elf.py examples/bpf_network.o sample-insns/bpf_network.insns from-network -s "2/1"

To compare the differences for sockex1, run:

    llvm-objdump-10 -d examples/sockex1_kern.o

and

    llvm-objdump-10 -d out.o


To patch in new insns manually, run:

    llvm-objcopy --dump-section=socket1=sample-insns/sockex1.insns examples/sockex1_kern.o

Then edit

    hexedit sample-insns/sockex1.insns


