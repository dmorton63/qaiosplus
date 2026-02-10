add_library(QWindowing STATIC
    src/QWWindow.cpp
    src/QWWindowManager.cpp
    src/QWFramebuffer.cpp
    src/QWCompositor.cpp
    src/QWRenderer.cpp
)

target_include_directories(QWindowing PUBLIC include)
target_link_libraries(QWindowing PUBLIC QCommon QGraphics QEvent QKMemory)
target_compile_options(QWindowing PRIVATE ${KERNEL_COMPILE_FLAGS})
