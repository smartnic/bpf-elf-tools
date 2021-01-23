#!/bin/bash

supported_progs=("xdp1_kern" "xdp2_kern" "xdp_pktcntr" "xdp_redirect" "xdp_map_access")

benchmarks_dir=/users/mdw362/bpf-benchmarks 
ebpf_samples=$benchmarks_dir/ebpf-samples/ubuntu-20.04-clang9-O2
katran_samples=$benchmarks_dir/katran/ubuntu-20.04-clang9-O2
simple_fw_samples=$benchmarks_dir/simple_fw/ubuntu-20.04-clang9-O2
output_dir=completed-programs

program=""
current_program=""
section_name=""
preceding_sections=""

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

function find_program {
    dir_name=$1

    if [[ $dir_name =~ ${supported_progs[0]} ]]; then
        section_name="xdp1"
        current_program="xdp1_kern"    
        program=$ebpf_samples/$current_program.o
    elif [[ $dir_name =~ ${supported_progs[1]} ]]; then
        section_name="xdp1"
        current_program="xdp2_kern"
        program=$ebpf_samples/$current_program.o
    elif [[ $dir_name =~ ${supported_progs[2]} ]]; then
        section_name="xdp-pktcntr"
        current_program="xdp_pktcntr"
        program=$katran_samples/$current_program.o
    elif [[ $dir_name =~ ${supported_progs[3]} ]]; then
        section_name="xdp_redirect"
        current_program="xdp_redirect_kern"
        program=$ebpf_samples/$current_program.o
    elif [[ $dir_name =~ ${supported_progs[4]} ]]; then
        section_name="xdp_map_acces"
        current_program="xdp_map_access_kern"
        program=$simple_fw_samples/$current_program.o
    else
        section_name=""
        current_program=""
        program=""
    fi
}

count=0
correct=0
for program_dir in superopt-insns/*; do
    find_program $program_dir
    if [[ -z $current_program ]]; then
        continue
    fi
    for i in {1..16}; do
        # Create new program directory if it doesn't already exist
        # The ##*/ part trims the preceding path from the directory
        # name
        mkdir -p $output_dir/${program_dir##*/}/$i
        for j in {1..10}; do
            insns=$program_dir/$i/output$j.insns
            if [ ! -f $insns ]; then
                continue
            fi
            echo "Program dir: $program_dir"
            output_program=$output_dir/${program_dir##*/}/$i/$current_program$j.o
            echo "Output_program $output_program"
            if [[ -z $preceding_sections ]]; then
                count=$((count+1))
                python3 patch_elf.py $program $insns $section_name -o $output_program 
                if [[ $? -ne 0 ]]; then
                    echo -e "${RED}Error${NC}: patch_elf.py $program $insns $section_name -o $output_program failed. Are the paths correct?" 
                else
                    correct=$((correct+1))
                    echo -e "${GREEN}Success${NC}: patch_elf.py $program $insns $section_name -o $output_program" 
                fi
            else
                python3 patch_elf.py $program $insns $section_name -o $output_program -s $preceding_sections 
                if [[ $? -ne 0 ]]; then
                    echo -e "${RED}Error${NC}: patch_elf.py $program $insns $section_name -o $output_program -s $preceding_sections failed. Are the paths correct?" 
                else    
                    correct += 1
                    echo -e "${GREEN}Success${NC}: patch_elf.py $program $insns $section_name -o $output_program -s $preceding_sections" 
                fi
            fi
        done
    done
done

echo -e "$correct / $count files patched"
