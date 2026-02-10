cmake_minimum_required(VERSION 3.20)
project(QAIOSPlus VERSION 1.0.0 LANGUAGES C CXX ASM)

# Enable NASM for assembly files
enable_language(ASM_NASM)
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)
set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> -f ${CMAKE_ASM_NASM_OBJECT_FORMAT} -o <OBJECT> <SOURCE>")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 17)

# Freestanding kernel flags (only for C/C++, not assembly)
set(KERNEL_COMPILE_FLAGS
    $<$<COMPILE_LANGUAGE:CXX,C>:-ffreestanding>
    $<$<COMPILE_LANGUAGE:CXX,C>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    $<$<COMPILE_LANGUAGE:CXX,C>:-nostdlib>
    $<$<COMPILE_LANGUAGE:CXX,C>:-mno-red-zone>
    $<$<COMPILE_LANGUAGE:CXX,C>:-mcmodel=kernel>
    $<$<COMPILE_LANGUAGE:CXX,C>:-fno-pic>
    $<$<COMPILE_LANGUAGE:CXX,C>:-fno-pie>
)

# Disable PIC/PIE globally for kernel code
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)

# Add subdirectories
add_subdirectory(QCommon)
add_subdirectory(QKernel)
add_subdirectory(QKMemory)
add_subdirectory(QArch)
add_subdirectory(QEvent)
add_subdirectory(QDrivers)
# add_subdirectory(QUSB)  # Replaced by kernel/drivers/UHCI and XHCI
add_subdirectory(QFileSystem)
add_subdirectory(QNetwork)
add_subdirectory(QWindowing)
add_subdirectory(QWControls)
add_subdirectory(QPR)
add_subdirectory(QQuantum)
add_subdirectory(kernel)
