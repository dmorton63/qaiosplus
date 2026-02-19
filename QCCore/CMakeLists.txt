add_library(QCCore STATIC
    src/QCString.cpp
    src/QCBuiltins.cpp
    src/QCLogger.cpp
    src/QCxxAbi.cpp
    src/QCMemUtil.c
)

target_include_directories(QCCore PUBLIC include)
target_compile_options(QCCore PRIVATE ${KERNEL_COMPILE_FLAGS})
