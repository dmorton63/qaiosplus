add_subdirectory(drivers)

add_executable(qaios_kernel
    QKMain.cpp
    QKConsole.cpp
    Boot/QKBoot.cpp
    Boot/Arch/ArchInit.cpp
    Boot/Acpi/AcpiTables.cpp
    Boot/Config/StartupConfig.cpp
    Boot/Ramdisk/RamdiskMount.cpp
    Boot/Desktop/DesktopSession.cpp
    Boot/Memory/AddressMapping.cpp
    Boot/Memory/EarlyMemory.cpp
    Boot/Tpm/TpmSecureStore.cpp
    Boot/Limine/LimineRequests.cpp
    Boot/Limine/LimineModules.cpp
    Debug/Serial/SerialDebug.cpp
    Debug/Panic.cpp
    Debug/Terminal/LimineTerminal.cpp
)

# Add assembly with custom compile flags
set_source_files_properties(QKBoot.asm PROPERTIES LANGUAGE ASM_NASM)
target_sources(qaios_kernel PRIVATE QKBoot.asm)

target_include_directories(qaios_kernel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/limine
)

target_link_libraries(qaios_kernel PRIVATE
    QCommon
    QKernel
    QKMemory
    QArch
    QEvent
    QDrivers
    QFileSystem
    QWindowing
    QWControls
    QDesktop
    QKDrivers
)

target_compile_options(qaios_kernel PRIVATE ${KERNEL_COMPILE_FLAGS})

# Linker script
set_target_properties(qaios_kernel PROPERTIES
    LINK_FLAGS "-T ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld -nostdlib -static"
    OUTPUT_NAME "qaios.elf"
)

# Custom target to create bootable image
add_custom_command(TARGET qaios_kernel POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:qaios_kernel> ${CMAKE_BINARY_DIR}/qaios.bin
    COMMENT "Creating bootable binary image"
)
