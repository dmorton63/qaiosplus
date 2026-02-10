add_library(QEvent STATIC
    src/QKEventTypes.cpp
    src/QKEventQueue.cpp
    src/QKEventListener.cpp
    src/QKEventManager.cpp
)

target_include_directories(QEvent PUBLIC include)
target_link_libraries(QEvent PUBLIC QCommon)
target_compile_options(QEvent PRIVATE ${KERNEL_COMPILE_FLAGS})
