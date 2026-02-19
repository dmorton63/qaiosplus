add_library(QDesktop STATIC
    src/QDAccent.cpp
    src/QDDesktop.cpp
    src/QDTheme.cpp
    src/QDTerminal.cpp
    src/QDShutdownDialog.cpp
    src/QDSetupWizard.cpp
    src/QDLoginDialog.cpp
    src/QDCommandProcessor.cpp

)

target_include_directories(QDesktop PUBLIC
    include
    ${CMAKE_SOURCE_DIR}/QKernel/include)
target_link_libraries(QDesktop PUBLIC QCommon QKernel QGraphics QWindowing QWControls QFileSystem)
target_compile_options(QDesktop PRIVATE ${KERNEL_COMPILE_FLAGS})
