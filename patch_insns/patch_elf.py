
#!/usr/bin/python3

from elftools.elf.elffile import ELFFile
import argparse
import array
import sys


def clear_reloc_insns(byte_array):
    """ Clears the src reg that is set by the text-extractor relocation """
    # 24 is opcode of lddw
    lddw_opcode = 24
    # 15 is decimal for 00001111; eliminate the 4 src reg bits
    hide_src_bitmask = 15
    # 240 is decimal for 11110000; eliminate the 4 dst reg bits
    hide_dst_bitmask = 240
    # src reg is 1 for ldmapfd; check for 16 since the binary representation
    # is 00010000
    ldmapfd_src_reg = 16 
    # Index of src/dst reg
    src_clear_idx = -1
    # Index range from imm value of lddw insn all the way through
    # the entirety of the next insn
    insn_clear_idxs = (-1, -1) 
    for i,byte in enumerate(byte_array):
        if i == src_clear_idx:
            # If src reg is not 1, it's not ldmapfd -- don't undo anything 
            if byte_array[i] & hide_dst_bitmask != ldmapfd_src_reg:
                src_clear_idx = -1
                insn_clear_idxs = (-1, -1)
                continue
            byte_array[i] &= hide_src_bitmask
        elif i >= insn_clear_idxs[0] and i < insn_clear_idxs[1]:
            byte_array[i] = 0
            continue
        if i % 8 == 0:
            if byte == lddw_opcode:
                src_clear_idx = i + 1
                insn_clear_idxs = (i + 4, i + 16)

def patch_insns(output_file, new_insns_file_name, remove_reloc):

    new_insns_file = open(new_insns_file_name, 'rb') 
    new_insns = new_insns_file.read()
    byte_array = bytearray(new_insns)
    if remove_reloc:
        clear_reloc_insns(byte_array)
    output_file.write(byte_array)

def read_elf_sections(elf_file_name, new_insns_file_name, section_to_replace, 
                    preceding_sections, output_file_name, remove_reloc):

    offset = 0

    with open(elf_file_name, 'rb') as elf_file:

        output_file = open(output_file_name, 'ab')
        elf = ELFFile(elf_file)
        
        section_of_interest = elf.get_section_by_name(section_to_replace)
        if section_of_interest is not None:
            # Usually the program will be in the first section 
            # of the binary. If not, add preceding section code
            for section_name in preceding_sections:
                section = elf.get_section_by_name(section_name)
                if section is not None:
                    offset += section['sh_size']
                    raw_bytes = section.data()
                    byte_array = bytearray(raw_bytes)
                    output_file.write(byte_array)
                else:
                    print(section_name, ' is not a section in this ELF')
                    sys.exit(1)

            offset += section_of_interest['sh_size']
            patch_insns(output_file, new_insns_file_name, remove_reloc)
        else:
            print('Section ', section_to_replace, ' could not be found. New insns not patched into resulting ELF')
            sys.exit(1)

        elf._parse_elf_header()

        write_remaining_bytes(elf_file, output_file, offset)
        output_file.close()


def write_remaining_bytes(elf_file, output_file, offset):

    index = 0
    byte = 0x0
    # Add the last bytes at the end of the object file
    while byte != b"":
        byte = elf_file.read(1)
        if index >= offset:
            output_file.write(byte)
        index += 1  

#Read first 64 bytes of the ELF which contains the
#ELF file header (magic number, format, version, etc.)
def read_elf_file_header(elf_file_name, output_file_name):

    byte_list = []

    with open(output_file_name, 'wb') as output_file:
        output_file.write(b"")

    output_file = open(output_file_name, 'wb')

    with open(elf_file_name, "rb") as elf_file:
        byte = 0x0
        index = 0
        while byte != b"" and index < 64:
            byte = elf_file.read(1)
            output_file.write(byte)
            index += 1
    output_file.close()

# Converts preceding section name string to list
def parse_section_string(section_string):
    if section_string == '':
        return []
    preceding_sections = section_string.split(',')
    for i in range(0, len(preceding_sections)):
        preceding_sections[i] = preceding_sections[i].strip()
    return preceding_sections
   
def main():

    argv = sys.argv
    parser = argparse.ArgumentParser(description='ELF patcher')
    parser.add_argument(
            'elf_file', 
            type=str,
            help='ELF input file')
    parser.add_argument(
            'new_insns', 
            type=str,
            help='File with new insns to patch in')
    parser.add_argument(
            'section_name', 
            type=str,
            help='ELF section to patch')
    parser.add_argument(
            '-s',
            '--sections',
            type=str,
            default='',
            help='Comma-separated list of ELF secitons preceding section of interest')
    parser.add_argument(
            '-o',
            '--output',
            type=str,
            default='out.o',
            help='Output file name')
    parser.add_argument(
            '--remove-reloc',
            action='store_true',
            help='Set to remove relocations')

    raw_args = parser.parse_args(argv[1:])
    '''
     NOTE:  Bytes in instructions must be equivalent to the number 
            of bytes in the section of the initial .o file
    '''
    preceding_sections = parse_section_string(raw_args.sections)

    read_elf_file_header(raw_args.elf_file, raw_args.output)
    read_elf_sections(raw_args.elf_file, raw_args.new_insns, raw_args.section_name, 
                        preceding_sections, raw_args.output, raw_args.remove_reloc)

if __name__ == '__main__':
    main()

