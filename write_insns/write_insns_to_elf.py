
#!/usr/bin/python3

from elftools.elf.elffile import ELFFile
import array
import sys

sections_to_extract = ['socket2', 'maps', 'license', '.eh_frame', \
          '.symtab', '.relsocket2', '.rel.eh_frame', '.strtab']

def patch_insns(output_file, new_insns_file_name):

    new_insns_file = open(new_insns_file_name, 'rb') 
    new_insns = new_insns_file.read()
    print('Patching in new insns:')
    print(new_insns)
    byte_array = bytearray(new_insns)
    output_file.write(byte_array)

def read_elf_sections(elf_file_name, new_insns_file_name, section_to_replace):
    offset = 0
    with open(elf_file_name, 'rb') as elf_file:
        output_file = open('./out.o', 'ab')
        elf = ELFFile(elf_file)
        for section_name in sections_to_extract:

            section = elf.get_section_by_name(section_name) 
            print('Section: ', section_name)
            if section_name == section_to_replace:
                patch_insns(output_file, new_insns_file_name)
            else: 
                raw_bytes = section.data()
                byte_array = bytearray(raw_bytes)
                output_file.write(byte_array)

            offset += section['sh_size']

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

def main():

    # Bytes in instructions must be equivalent to the number 
    # of bytes in the section of the initial .o file

    if len(sys.argv) != 4:
        print('Usage: <ELF file> <file w/ new insns> <name of section>')
        sys.exit(1)

    read_elf_file_header(sys.argv[1])
    read_elf_sections(sys.argv[1], sys.argv[2], sys.argv[3])

if __name__ == '__main__':
    main()

