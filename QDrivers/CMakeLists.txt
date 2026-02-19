add_library(QDrivers STATIC
    src/QDrvTimer.cpp
    src/QDrvBGA.cpp
    src/QDrvVmwareSVGA.cpp
)

target_include_directories(QDrivers PUBLIC include)
target_link_libraries(QDrivers PUBLIC QCommon QKernel QArch)
target_compile_options(QDrivers PRIVATE ${KERNEL_COMPILE_FLAGS})
