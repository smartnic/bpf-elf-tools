#!/bin/bash

bins="sockex1 sock_example"

echo "Requesting sudo for linux capability permissions to binaries"
for bin in $bins; do
    echo sudo setcap cap_net_admin,cap_net_raw=eip $bin
    sudo setcap cap_net_admin,cap_net_raw=eip $bin
done
