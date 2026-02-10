add_library(QWControls STATIC
    src/QWCtrlButton.cpp
    src/QWCtrlLabel.cpp
    src/QWCtrlListView.cpp
    src/QWCtrlTextBox.cpp
)

target_include_directories(QWControls PUBLIC include)
target_link_libraries(QWControls PUBLIC QCommon QWindowing QEvent QKMemory)
target_compile_options(QWControls PRIVATE ${KERNEL_COMPILE_FLAGS})
