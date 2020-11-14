
#!/usr/bin/python3

from elftools.elf.elffile import ELFFile
import array
import sys


def patch_insns(output_file, new_insns_file_name):

    new_insns_file = open(new_insns_file_name, 'rb') 
    new_insns = new_insns_file.read()
    print('Patching in new insns:')
    print(new_insns)
    byte_array = bytearray(new_insns)
    output_file.write(byte_array)

def read_elf_sections(elf_file_name, new_insns_file_name, section_to_replace, preceding_sections):

    offset = 0

    with open(elf_file_name, 'rb') as elf_file:

        output_file = open('./out.o', 'ab')
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
            patch_insns(output_file, new_insns_file_name)
        else:
            print('Section ', section_to_replace, ' could not be found. New insns not patched into resulting ELF')

        print('elf header: ', elf._parse_elf_header())
        print('Offset = ', offset)

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
def read_elf_file_header(elf_file_name):

    byte_list = []

    with open('out.o', 'wb') as output_file:
        output_file.write(b"")

    output_file = open('out.o', 'wb')

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
    print(section_string)
    preceding_sections = section_string.split(',')
    for i in range(0, len(preceding_sections)):
        preceding_sections[i] = preceding_sections[i].strip()
    print(preceding_sections)
    return preceding_sections
   
def main():

    # Bytes in instructions must be equivalent to the number 
    # of bytes in the section of the initial .o file
    if len(sys.argv) != 4 and len(sys.argv) != 5:
        print('Usage: <ELF file> <file w/ new insns> <name of section> <preceding section names (OPTIONAL)>')
        sys.exit(1)

    preceding_sections = []
    if len(sys.argv) == 5:
        preceding_sections = parse_section_string(sys.argv[4])

    read_elf_file_header(sys.argv[1])
    read_elf_sections(sys.argv[1], sys.argv[2], sys.argv[3], preceding_sections)

if __name__ == '__main__':
    main()

