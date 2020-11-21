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
function perform_test {

    python3 patch_elf.py $1 $2 $3 "$4" > /dev/null
    is_patcher_successful $1 >/dev/null
    llvm-objdump-10 -d out.o > /dev/null
    is_output_elf_correct $1
    rm out.o
}

perform_test examples/sockex1_kern.o sample-insns/sockex1.insns socket1 
perform_test examples/sockex1_kern_old.o sample-insns/sockex1.insns socket1 
perform_test examples/sockex2_kern.o sample-insns/sockex2.insns socket2
perform_test examples/bpf_network.o sample-insns/bpf_network.insns from-network "2/1"
perform_test examples/xdp1_kern.o sample-insns/xdp1.insns xdp1
perform_test examples/xdp2_kern.o sample-insns/xdp2.insns xdp1
perform_test examples/xdp_tx_iptunnel_kern.o sample-insns/xdp_tx_iptunnel.insns xdp_tx_iptunnel
perform_test examples/xdp_redirect_kern.o sample-insns/xdp_redirect.insns xdp_redirect
perform_test examples/xdp_redirect_map_kern.o sample-insns/xdp_redirect_map.insns xdp_redirect_map
perform_test examples/xdp_pktcntr.o sample-insns/xdp_pktcntr.insns xdp-pktcntr
perform_test examples/bpf_xdp.o sample-insns/bpf_xdp-from-netdev.insns from-netdev "2/1, 2/3, 2/5, 2/4"

echo "**ALL DONE**"
