#!/bin/bash
# ============================================================================
#  COSMOS HYPERVISOR BUILD SYSTEM
# ============================================================================
#  Compiles the CosmOS Stealth Hypervisor.
#
#  Options:
#    ./build_hypervisor.sh          → Bare Metal Build (for CosmOS ISO)
#    ./build_hypervisor.sh linux    → Linux Build (for testing)
#    ./build_hypervisor.sh clean    → Clean build artifacts
# ============================================================================

set -e

MODE="${1:-baremetal}"
HV_DIR="hypervisor"
BUILD_DIR="hypervisor/build"

# Common compiler flags
COMMON_FLAGS="-m64 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function"
COMMON_FLAGS="$COMMON_FLAGS -fno-exceptions -fno-rtti -fno-stack-protector"
COMMON_FLAGS="$COMMON_FLAGS -fcf-protection=none -Wno-unused-but-set-variable"

echo "╔══════════════════════════════════════════╗"
echo "║   COSMOS HYPERVISOR BUILD SYSTEM         ║"
echo "║   Mode: $MODE"
echo "╚══════════════════════════════════════════╝"

mkdir -p "$BUILD_DIR"

if [ "$MODE" = "clean" ]; then
    echo "[BUILD] Cleaning up..."
    rm -rf "$BUILD_DIR"
    echo "[BUILD] Done!"
    exit 0
fi

if [ "$MODE" = "linux" ]; then
    # ========================================
    # LINUX BUILD (for testing)
    # ========================================
    echo "[BUILD] Linux Mode (Test Environment)"
    
    CXX_FLAGS="$COMMON_FLAGS"
    AS_FLAGS="--64"
    
    echo "[1/10] Compiling cosmos_vmx.S..."
    as $AS_FLAGS -o "$BUILD_DIR/cosmos_vmx.o" "$HV_DIR/cosmos_vmx.S" \
        --noexecstack
    
    echo "[2/10] Compiling cosmos_hypervisor.cpp..."
    g++ $CXX_FLAGS -I"$HV_DIR" -c -o "$BUILD_DIR/cosmos_hypervisor.o" \
        "$HV_DIR/cosmos_hypervisor.cpp"
    
    echo "[3/10] Compiling cosmos_ept.cpp..."
    g++ $CXX_FLAGS -I"$HV_DIR" -c -o "$BUILD_DIR/cosmos_ept.o" \
        "$HV_DIR/cosmos_ept.cpp"
    
    echo "[4/10] Compiling cosmos_launch.cpp..."
    g++ $CXX_FLAGS -I"$HV_DIR" -c -o "$BUILD_DIR/cosmos_launch.o" \
        "$HV_DIR/cosmos_launch.cpp"
    
    echo "[5/10] Compiling cosmos_passthrough.cpp..."
    g++ $CXX_FLAGS -I"$HV_DIR" -c -o "$BUILD_DIR/cosmos_passthrough.o" \
        "$HV_DIR/cosmos_passthrough.cpp"
    
    echo "[6/10] Compiling cosmos_guest_boot.cpp..."
    g++ $CXX_FLAGS -I"$HV_DIR" -c -o "$BUILD_DIR/cosmos_guest_boot.o" \
        "$HV_DIR/cosmos_guest_boot.cpp"
    
    echo "[7/10] Compiling cosmos_io_trap.cpp..."
    g++ $CXX_FLAGS -I"$HV_DIR" -c -o "$BUILD_DIR/cosmos_io_trap.o" \
        "$HV_DIR/cosmos_io_trap.cpp"
    
    echo "[8/10] Compiling cosmos_virtio_blk.cpp..."
    g++ $CXX_FLAGS -I"$HV_DIR" -c -o "$BUILD_DIR/cosmos_virtio_blk.o" \
        "$HV_DIR/cosmos_virtio_blk.cpp"
    
    echo "[9/10] Compiling cosmos_anti_cheat.cpp..."
    g++ $CXX_FLAGS -I"$HV_DIR" -c -o "$BUILD_DIR/cosmos_anti_cheat.o" \
        "$HV_DIR/cosmos_anti_cheat.cpp"
    
    echo "[10/10] Linking hypervisor objects..."
    ld -r -o "$BUILD_DIR/cosmos_hypervisor_all.o" \
        "$BUILD_DIR/cosmos_vmx.o" \
        "$BUILD_DIR/cosmos_hypervisor.o" \
        "$BUILD_DIR/cosmos_ept.o" \
        "$BUILD_DIR/cosmos_launch.o" \
        "$BUILD_DIR/cosmos_passthrough.o" \
        "$BUILD_DIR/cosmos_guest_boot.o" \
        "$BUILD_DIR/cosmos_io_trap.o" \
        "$BUILD_DIR/cosmos_virtio_blk.o" \
        "$BUILD_DIR/cosmos_anti_cheat.o"
    
    echo ""
    echo "═══════════════════════════════════════════"
    echo "  ★ LINUX BUILD SUCCESSFUL ★"
    echo "═══════════════════════════════════════════"
    echo ""
    echo "Exported Symbols:"
    nm "$BUILD_DIR/cosmos_hypervisor_all.o" | grep " T " | grep -i "cosmos_hv" | \
        awk '{print "  " $3}'
    echo ""
    echo "Object Size:"
    size "$BUILD_DIR/cosmos_hypervisor_all.o" | tail -1 | \
        awk '{printf "  Text: %s bytes, Data: %s bytes, BSS: %s bytes\n", $1, $2, $3}'

elif [ "$MODE" = "baremetal" ]; then
    # ========================================
    # BARE METAL BUILD (for CosmOS ISO)
    # ========================================
    echo "[BUILD] Bare-Metal Mode (CosmOS)"
    
    CXX_FLAGS="$COMMON_FLAGS -ffreestanding -mno-red-zone -mcmodel=kernel -U__linux__"
    CXX_FLAGS="$CXX_FLAGS -fno-pic -mcmodel=kernel"
    AS_FLAGS="--64"
    
    echo "[1/10] Compiling cosmos_vmx.S..."
    as $AS_FLAGS -o "$BUILD_DIR/cosmos_vmx.o" "$HV_DIR/cosmos_vmx.S" \
        --noexecstack
    
    echo "[2/10] Compiling cosmos_hypervisor.cpp..."
    g++ $CXX_FLAGS -I"$HV_DIR" -I. -c -o "$BUILD_DIR/cosmos_hypervisor.o" \
        "$HV_DIR/cosmos_hypervisor.cpp" -fpermissive -Wno-int-to-pointer-cast
    
    echo "[3/10] Compiling cosmos_ept.cpp..."
    g++ $CXX_FLAGS -I"$HV_DIR" -I. -c -o "$BUILD_DIR/cosmos_ept.o" \
        "$HV_DIR/cosmos_ept.cpp" -fpermissive -Wno-int-to-pointer-cast
    
    echo "[4/10] Compiling cosmos_launch.cpp..."
    g++ $CXX_FLAGS -I"$HV_DIR" -I. -c -o "$BUILD_DIR/cosmos_launch.o" \
        "$HV_DIR/cosmos_launch.cpp" -fpermissive -Wno-int-to-pointer-cast
    
    echo "[5/10] Compiling cosmos_passthrough.cpp..."
    g++ $CXX_FLAGS -I"$HV_DIR" -I. -c -o "$BUILD_DIR/cosmos_passthrough.o" \
        "$HV_DIR/cosmos_passthrough.cpp" -fpermissive -Wno-int-to-pointer-cast
    
    echo "[6/10] Compiling cosmos_guest_boot.cpp..."
    g++ $CXX_FLAGS -I"$HV_DIR" -I. -c -o "$BUILD_DIR/cosmos_guest_boot.o" \
        "$HV_DIR/cosmos_guest_boot.cpp" -fpermissive -Wno-int-to-pointer-cast
    
    echo "[7/10] Compiling cosmos_io_trap.cpp..."
    g++ $CXX_FLAGS -I"$HV_DIR" -I. -c -o "$BUILD_DIR/cosmos_io_trap.o" \
        "$HV_DIR/cosmos_io_trap.cpp" -fpermissive -Wno-int-to-pointer-cast
    
    echo "[8/10] Compiling cosmos_virtio_blk.cpp..."
    g++ $CXX_FLAGS -I"$HV_DIR" -I. -c -o "$BUILD_DIR/cosmos_virtio_blk.o" \
        "$HV_DIR/cosmos_virtio_blk.cpp" -fpermissive -Wno-int-to-pointer-cast
    
    echo "[9/10] Compiling cosmos_anti_cheat.cpp..."
    g++ $CXX_FLAGS -I"$HV_DIR" -I. -c -o "$BUILD_DIR/cosmos_anti_cheat.o" \
        "$HV_DIR/cosmos_anti_cheat.cpp" -fpermissive -Wno-int-to-pointer-cast
    
    echo "[10/10] Linking Hypervisor module..."
    ld -r -o "$BUILD_DIR/cosmos_hypervisor_all.o" \
        "$BUILD_DIR/cosmos_vmx.o" \
        "$BUILD_DIR/cosmos_hypervisor.o" \
        "$BUILD_DIR/cosmos_ept.o" \
        "$BUILD_DIR/cosmos_launch.o" \
        "$BUILD_DIR/cosmos_passthrough.o" \
        "$BUILD_DIR/cosmos_guest_boot.o" \
        "$BUILD_DIR/cosmos_io_trap.o" \
        "$BUILD_DIR/cosmos_virtio_blk.o" \
        "$BUILD_DIR/cosmos_anti_cheat.o"
    
    echo ""
    echo "═══════════════════════════════════════════"
    echo "  ★ BARE METAL BUILD SUCCESSFUL ★"
    echo "═══════════════════════════════════════════"
    echo ""
    echo "Output: $BUILD_DIR/cosmos_hypervisor_all.o"
    echo ""
    echo "To link into CosmOS build.sh, add:"
    echo "  ld ... $BUILD_DIR/cosmos_hypervisor_all.o ... -o kernel_main.elf"
    echo ""
    echo "Exported Symbols:"
    nm "$BUILD_DIR/cosmos_hypervisor_all.o" | grep " T " | grep -i "cosmos_hv\|cosmos_vm\|cosmos_inv" | \
        awk '{print "  " $3}'
    echo ""
    echo "Object Size:"
    size "$BUILD_DIR/cosmos_hypervisor_all.o" | tail -1 | \
        awk '{printf "  Text: %s bytes, Data: %s bytes, BSS: %s bytes\n", $1, $2, $3}'

else
    echo "Unknown mode: $MODE"
    echo "Usage: ./build_hypervisor.sh [linux|baremetal|clean]"
    exit 1
fi
