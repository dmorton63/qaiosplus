# QGraphics - Core graphics abstractions
# Header-only interface library for now
# Future: add QGPainterSoftware.cpp when needed

add_library(QGraphics INTERFACE)

target_include_directories(QGraphics INTERFACE include)
target_link_libraries(QGraphics INTERFACE QCommon)
