
# disas1.py

from elftools.elf.elffile import ELFFile
from capstone import *

def read_elf():
    with open('./sockex2_kern.o', 'rb') as f:
        e = ELFFile(f)
        for section in e.iter_sections():
            print('SECTION: ', section.name)
            ops = section.data()
            print(ops)

def write_to_file():
    byteList = [0xb4, 0x16, 6, 2, 0x00]
    byteArray = bytearray(byteList)
    with open('test_bytes.o', 'wb') as f:
        f.write(byteArray)     

def main():
   
#    write_to_file() 
    read_elf()

if __name__ == '__main__':
    main()

