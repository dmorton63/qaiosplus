add_library(QKMemory STATIC
    src/QKMemPMM.cpp
    src/QKMemVMM.cpp
    src/QKMemHeap.cpp
    src/QKMemPaging.cpp
    src/QKMemTranslator.cpp
)

target_include_directories(QKMemory PUBLIC include)
target_link_libraries(QKMemory PUBLIC QCommon)
target_compile_options(QKMemory PRIVATE ${KERNEL_COMPILE_FLAGS})
