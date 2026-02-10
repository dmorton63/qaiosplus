add_library(QFileSystem STATIC
    src/QFSVFS.cpp
    src/QFSFile.cpp
    src/QFSDirectory.cpp
    src/QFSFAT32.cpp
    src/QFSCache.cpp
    src/QFSPath.cpp
)

target_include_directories(QFileSystem PUBLIC include)
target_link_libraries(QFileSystem PUBLIC QCommon QKMemory)
target_compile_options(QFileSystem PRIVATE ${KERNEL_COMPILE_FLAGS})
