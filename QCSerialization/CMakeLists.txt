add_library(QCSerialization STATIC
    src/QCJson.cpp
    src/QCJsonOverlay.cpp
)

target_include_directories(QCSerialization PUBLIC include)
target_link_libraries(QCSerialization PUBLIC QCCore)
target_compile_options(QCSerialization PRIVATE ${KERNEL_COMPILE_FLAGS})
