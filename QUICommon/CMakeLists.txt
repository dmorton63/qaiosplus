add_library(QUICommon STATIC
    src/QCUIStyle.cpp
)

target_include_directories(QUICommon PUBLIC include)
target_link_libraries(QUICommon PUBLIC QCCore)
target_compile_options(QUICommon PRIVATE ${KERNEL_COMPILE_FLAGS})
