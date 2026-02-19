add_library(QWindowing STATIC
    src/QWWindow.cpp
    src/QWWindowManager.cpp
    src/QWFramebuffer.cpp
    src/QWFramebufferBackend.cpp
    src/QWCompositor.cpp
    src/QWFramebufferPresentBackend.cpp
    src/QWVmwareSVGAPresentBackend.cpp
    src/QWMessageBus.cpp
    src/QWRenderer.cpp
    src/QWStyleRenderer.cpp
    src/QWStyleTypes.cpp
    src/QWSurfaceBackend.cpp
    src/QWStyleSystem.cpp
)

target_include_directories(QWindowing PUBLIC include)
target_link_libraries(QWindowing PUBLIC
    QCommon
    QGraphics
    QDrivers
    QEvent
    QKMemory
    QWControls
    QWInterfaces)
target_compile_options(QWindowing PRIVATE ${KERNEL_COMPILE_FLAGS})
