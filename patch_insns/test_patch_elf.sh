#!/bin/bash

function is_patcher_successful {
    if [[ $? -gt 0 ]]
    then
        echo "$1 failed to run patcher"
        exit
    fi
}

function is_output_elf_correct {
    if [[ $? -gt 0 ]]
    then
        echo "$1 resulted in invalid ELF output"
        exit
    fi
}
#
# $1 -> input file
# $2 -> insns file
# $3 -> section
# $4 -> output file
# $5 -> preceding sections
function perform_test {

    if [[ $# -eq 4 ]]
    then
        python3 patch_elf.py $1 $2 $3 -o $4 > /dev/null
    else
        python3 patch_elf.py $1 $2 $3 -o $4 -s "$5" > /dev/null
    fi
    is_patcher_successful $1 
    llvm-objdump-10 -d $4 > /dev/null
    is_output_elf_correct $1
#    rm $3_patched.o
}

mkdir results
perform_test examples/sockex1_kern.o sample-insns/sockex1.insns socket1 results/sockex1_kern_patched.o
perform_test examples/sockex1_kern_old.o sample-insns/sockex1.insns socket1 results/sockex1_kern_old_patched.o
perform_test examples/sockex2_kern.o sample-insns/sockex2.insns socket2 results/sockex2_kern_patched.o
perform_test examples/bpf_network.o sample-insns/bpf_network.insns from-network results/bpf_network_patched.o "2/1"
perform_test examples/xdp1_kern.o sample-insns/xdp1.insns xdp1 results/xdp1_kern_patched.o 
perform_test examples/xdp2_kern.o sample-insns/xdp2.insns xdp1 results/xdp2_kern_patched.o 
perform_test examples/xdp_tx_iptunnel_kern.o sample-insns/xdp_tx_iptunnel.insns xdp_tx_iptunnel results/xdp_tx_iptunnel_kern_patched.o 
perform_test examples/xdp_redirect_kern.o sample-insns/xdp_redirect.insns xdp_redirect results/xdp_redirect_kern_patched.o 
perform_test examples/xdp_redirect_map_kern.o sample-insns/xdp_redirect_map.insns xdp_redirect_map results/xdp_redirect_map_kern_patched.o 
perform_test examples/xdp_pktcntr.o sample-insns/xdp_pktcntr.insns  xdp-pktcntr results/xdp_pktcntr_patched.o 
perform_test examples/bpf_xdp.o sample-insns/bpf_xdp-from-netdev.insns from-netdev results/bpf_xdp_patched.o "2/1, 2/3, 2/5, 2/4"

echo "**ALL DONE**"
