add_library(QCommon STATIC
    src/QCString.cpp
    src/QCBuiltins.cpp
    src/QCLogger.cpp
    src/QCxxAbi.cpp
    src/QCMemUtil.c
    src/QCUIStyle.cpp
)

target_include_directories(QCommon PUBLIC include)
target_compile_options(QCommon PRIVATE ${KERNEL_COMPILE_FLAGS})
