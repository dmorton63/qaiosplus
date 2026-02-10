# Kernel Drivers - CMakeLists.txt

add_subdirectory(PS2)
add_subdirectory(UHCI)
add_subdirectory(XHCI)

add_library(QKDrivers STATIC
    QKDrvManager.cpp
)

target_include_directories(QKDrivers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(QKDrivers PUBLIC
    QKDrvPS2
    QKDrvUHCI
    QKDrvXHCI
    QCommon
    QArch
)
target_compile_options(QKDrivers PRIVATE ${KERNEL_COMPILE_FLAGS})
