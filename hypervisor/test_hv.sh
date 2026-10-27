#!/bin/bash
echo "[*] Checking if KVM is loaded..."
lsmod | grep kvm

echo "[*] Unloading old module..."
rmmod cosmos_hv3 2>/dev/null || true

echo "[*] Unloading KVM (if present)..."
rmmod kvm_intel 2>/dev/null || true
rmmod kvm 2>/dev/null || true

echo "[*] Clearing dmesg buffer..."
dmesg -c > /dev/null

echo "[*] Loading CosmOS Stealth Hypervisor LKM..."
insmod /root/coding/MeinOS/hypervisor/cosmos_hv3.ko

echo "[*] Module load result: $?"
echo "[*] Recent dmesg logs:"
dmesg | tail -n 20
