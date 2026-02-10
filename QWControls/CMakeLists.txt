add_library(QWControls STATIC
    src/QWCtrlBase.cpp
    src/QWCtrlContainer.cpp
    src/QWCtrlFrame.cpp
    src/QWCtrlPanel.cpp
    src/QWCtrlButton.cpp
    src/QWCtrlLabel.cpp
    src/QWCtrlListView.cpp
    src/QWCtrlTextBox.cpp
    src/QWCtrlScrollBar.cpp
    src/QWCtrlComboBox.cpp
)

target_include_directories(QWControls PUBLIC include)
target_link_libraries(QWControls PUBLIC QCommon QGraphics QWindowing QEvent QKMemory)
target_compile_options(QWControls PRIVATE ${KERNEL_COMPILE_FLAGS})
