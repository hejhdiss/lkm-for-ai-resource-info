# NeuroShell LKM

Linux kernel module exposing hardware introspection data
via /sys/kernel/neuroshell/.

## Build
sudo apt install build-essential linux-headers-$(uname -r)
make

## Load
sudo insmod neuroshell.ko

## Unload
sudo rmmod neuroshell


## Tested
- Vmware 17.
- Xubuntu 24.04.2
