

from elftools.elf.elffile import ELFFile
import array
import sys

sections_to_extract = ['socket2', 'maps', 'license', '.eh_frame', '.symtab', '.relsocket2', '.rel.eh_frame', '.strtab']
def read_elf_sections():
    offset = 0
    with open('./sockex2_kern.o', 'rb') as f:
        f2 = open('./test_bytes.o', 'ab')
        elf = ELFFile(f)
        print('num sections: ' , elf.num_sections())
        for section_name in sections_to_extract:
            print('Getting ', section_name)
            section = elf.get_section_by_name(section_name) 
            ops = section.data()
            byte_array = bytearray(ops)
            f2.write(byte_array)
            print(ops)
            print (hex(section['sh_addr']), section.name, section['sh_size'])

            print('length of sec: ', len(byte_array))
            offset += section['sh_size']

        print('elf header: ', elf._parse_elf_header())

        print('Offset = ', offset)
        index = 0
        byte = 0x0
        while byte != b"":
            byte = f.read(1)
            if index >= offset:
                f2.write(byte)
            index += 1


#Read first 64 bytes of the ELF which contains the
#ELF file header (magic number, format, version, etc.)
def read_elf_file_header():

    byte_list = []

    with open("test_bytes.o", "wb") as f2:
        f2.write(b"")

    with open("sockex2_kern.o", "rb") as f:
        byte = 0x0
        index = 0
        while byte != b"" and index < 64:
            with open("test_bytes.o", "ab") as f2:
                byte = f.read(1)
                f2.write(byte)
            index += 1

def main():
   
    read_elf_file_header()
    read_elf_sections()

if __name__ == '__main__':
    main()

