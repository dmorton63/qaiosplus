add_library(QKernel STATIC
    src/QKKernel.cpp
    src/QKTaskManager.cpp
    src/QKScheduler.cpp
    src/QKInterrupts.cpp
)

target_include_directories(QKernel PUBLIC include)
target_link_libraries(QKernel PUBLIC QCommon)
target_compile_options(QKernel PRIVATE ${KERNEL_COMPILE_FLAGS})
