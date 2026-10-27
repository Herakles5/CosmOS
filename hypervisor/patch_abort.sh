#!/bin/bash
sed -i 's/void cosmos_vmexit_handler/int cosmos_vmexit_handler/' /root/coding/MeinOS/hypervisor/cosmos_hypervisor.cpp
sed -i 's/extern "C" void cosmos_vmexit_handler/extern "C" int cosmos_vmexit_handler/' /root/coding/MeinOS/hypervisor/cosmos_hypervisor.cpp
