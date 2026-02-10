add_library(QNetwork STATIC
    src/QNetStack.cpp
    src/QNetEthernet.cpp
    src/QNetIP.cpp
    src/QNetTCP.cpp
    src/QNetUDP.cpp
    src/QNetSocket.cpp
)

target_include_directories(QNetwork PUBLIC include)
target_link_libraries(QNetwork PUBLIC QCommon QKMemory)
target_compile_options(QNetwork PRIVATE ${KERNEL_COMPILE_FLAGS})
