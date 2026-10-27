sed -i 's/20ULL \* 1024 \* 1024 \* 1024/512ULL * 1024 * 1024 * 1024/' /root/coding/MeinOS/hypervisor/cosmos_ept.cpp
sed -i 's/20GB/512GB/' /root/coding/MeinOS/hypervisor/cosmos_ept.cpp
