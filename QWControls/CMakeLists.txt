add_library(QWControls STATIC
    src/Base/ControlBase.cpp
    src/Containers/Container.cpp
    src/Containers/Frame.cpp
    src/Containers/Panel.cpp
    src/Leaf/Button.cpp
    src/Leaf/Label.cpp
    src/Leaf/ScrollBar.cpp
    src/Leaf/TextBox.cpp
    src/Composite/ComboBox.cpp
    src/Composite/ListView.cpp
)

target_compile_options(QWControls PRIVATE ${KERNEL_COMPILE_FLAGS})
target_include_directories(QWControls PUBLIC include)
target_link_libraries(QWControls PUBLIC QCommon QGraphics QWindowing QEvent QKMemory QWInterfaces)
