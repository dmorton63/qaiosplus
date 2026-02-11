# QGraphics - Core graphics abstractions

add_library(QGraphics STATIC
	src/QG/PainterSurface.cpp
)

target_include_directories(QGraphics PUBLIC include)
target_link_libraries(QGraphics PUBLIC QCommon)
target_compile_options(QGraphics PRIVATE ${KERNEL_COMPILE_FLAGS})
