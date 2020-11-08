
# BPF ELF patcher

Creates a new ELF file with new instructions patched into a section of interest.

Example using sockex1:

    python3 patch_elf.py examples/sockex1_kern.o sample-insns/sockex1.insns socket1

To compare the differences, run:

    llvm-objdump-10 -d examples/sockex1_kern.o

and

    llvm-objdump-10 -d out.o


To patch in new insns manually, run:

    llvm-objcopy --dump-section=socket1=sample-insns/sockex1.insns examples/sockex1_kern.o

Then edit

    hexedit sample-insns/sockex1.insns


