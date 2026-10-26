#!/bin/sh
# Prepara la red y el bus CAN virtual dentro de la VM
ip link set up lo
ip link set up eth0 2>/dev/null && ip addr add 10.0.2.15/24 dev eth0
insmod /root/can-dev.ko
insmod /root/vcan.ko
ip link add dev vcan0 type vcan
ip link set up vcan0
echo "p3-setup: lo, eth0 (si existe) y vcan0 listos"
