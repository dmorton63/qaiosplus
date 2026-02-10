add_library(QDesktop STATIC
    src/QDAccent.cpp
    src/QDDesktop.cpp
)

target_include_directories(QDesktop PUBLIC include)
target_link_libraries(QDesktop PUBLIC QCommon QGraphics QWindowing QWControls)
target_compile_options(QDesktop PRIVATE ${KERNEL_COMPILE_FLAGS})
