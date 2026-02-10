add_library(QArch STATIC
    src/QArchGDT.cpp
    src/QArchIDT.cpp
    src/QArchPCI.cpp
    src/QArchCPU.cpp
)

target_include_directories(QArch PUBLIC include)
target_link_libraries(QArch PUBLIC QCommon)
target_compile_options(QArch PRIVATE ${KERNEL_COMPILE_FLAGS})

# Add NASM assembly separately so kernel compile flags don't apply
set_source_files_properties(src/QArchPort.asm PROPERTIES LANGUAGE ASM_NASM)
target_sources(QArch PRIVATE src/QArchPort.asm)
