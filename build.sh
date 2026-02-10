#!/bin/bash
#
# QAIOS Build Script
# Builds the kernel and creates a bootable ISO
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Project directories
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
ISO_DIR="${PROJECT_DIR}/iso"
LIMINE_DIR="${PROJECT_DIR}/limine"

# Output files
KERNEL_ELF="${BUILD_DIR}/kernel/qaios.elf"
ISO_FILE="${BUILD_DIR}/qaios-limine.iso"

echo -e "${CYAN}========================================${NC}"
echo -e "${CYAN}       QAIOS Build System${NC}"
echo -e "${CYAN}========================================${NC}"

# Parse arguments
CLEAN=false
VERBOSE=false
RUN_QEMU=false
JOBS=$(nproc 2>/dev/null || echo 4)

while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -r|--run)
            RUN_QEMU=true
            shift
            ;;
        -j*)
            JOBS="${1#-j}"
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  -c, --clean     Clean build (remove build directory)"
            echo "  -v, --verbose   Verbose build output"
            echo "  -r, --run       Run in QEMU after building"
            echo "  -j<N>           Use N parallel jobs (default: $(nproc))"
            echo "  -h, --help      Show this help"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

# Step 1: Clean if requested
if [ "$CLEAN" = true ]; then
    echo -e "${YELLOW}[1/5] Cleaning build directory...${NC}"
    rm -rf "${BUILD_DIR}"
    echo -e "${GREEN}      Done.${NC}"
else
    echo -e "${YELLOW}[1/5] Skipping clean (use -c to clean)${NC}"
fi

# Step 2: Create build directory and configure
echo -e "${YELLOW}[2/5] Configuring with CMake...${NC}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

if [ ! -f "Makefile" ] || [ "$CLEAN" = true ]; then
    cmake .. || { echo -e "${RED}CMake configuration failed!${NC}"; exit 1; }
fi
echo -e "${GREEN}      Done.${NC}"

# Step 3: Build
echo -e "${YELLOW}[3/5] Building kernel (using ${JOBS} jobs)...${NC}"
if [ "$VERBOSE" = true ]; then
    make -j${JOBS} VERBOSE=1 || { echo -e "${RED}Build failed!${NC}"; exit 1; }
else
    make -j${JOBS} || { echo -e "${RED}Build failed!${NC}"; exit 1; }
fi
echo -e "${GREEN}      Done.${NC}"

# Step 4: Copy kernel to ISO directory
echo -e "${YELLOW}[4/5] Preparing ISO contents...${NC}"
if [ ! -f "${KERNEL_ELF}" ]; then
    echo -e "${RED}Kernel ELF not found at ${KERNEL_ELF}${NC}"
    exit 1
fi
cp "${KERNEL_ELF}" "${ISO_DIR}/boot/"
echo -e "${GREEN}      Copied kernel to iso/boot/${NC}"

# Step 5: Create ISO
echo -e "${YELLOW}[5/5] Creating bootable ISO...${NC}"
cd "${PROJECT_DIR}"

# Check for xorriso
if ! command -v xorriso &> /dev/null; then
    echo -e "${RED}xorriso not found! Install with: sudo apt install xorriso${NC}"
    exit 1
fi

xorriso -as mkisofs \
    -b boot/limine/limine-bios-cd.bin \
    -no-emul-boot \
    -boot-load-size 4 \
    -boot-info-table \
    --protective-msdos-label \
    "${ISO_DIR}" \
    -o "${ISO_FILE}" \
    2>&1 | grep -v "^xorriso" || true

# Install Limine BIOS stages (optional, may fail on CD image)
if [ -x "${LIMINE_DIR}/limine" ]; then
    "${LIMINE_DIR}/limine" bios-install "${ISO_FILE}" 2>/dev/null || true
fi

echo -e "${GREEN}      Done.${NC}"

# Summary
echo ""
echo -e "${CYAN}========================================${NC}"
echo -e "${GREEN}Build complete!${NC}"
echo -e "${CYAN}========================================${NC}"
echo -e "  Kernel: ${KERNEL_ELF}"
echo -e "  ISO:    ${ISO_FILE}"
echo -e "  Size:   $(du -h "${ISO_FILE}" | cut -f1)"
echo ""

# Run QEMU if requested
if [ "$RUN_QEMU" = true ]; then
    echo -e "${YELLOW}Launching QEMU...${NC}"
    echo -e "${CYAN}(Click inside QEMU window for keyboard focus)${NC}"
    echo -e "${CYAN}(Press Ctrl+Q in OS to shutdown)${NC}"
    echo -e "${CYAN}(Serial log: ${BUILD_DIR}/serial.log)${NC}"
    echo ""
    # Use xHCI controller with USB tablet for absolute mouse positioning
    qemu-system-x86_64 \
        -cdrom "${ISO_FILE}" \
        -m 256M \
        -device qemu-xhci,id=xhci \
        -device usb-tablet,bus=xhci.0 \
        -serial file:"${BUILD_DIR}/serial.log"
    echo ""
    echo -e "${CYAN}=== Serial output (last 30 lines) ===${NC}"
    tail -30 "${BUILD_DIR}/serial.log"
fi
