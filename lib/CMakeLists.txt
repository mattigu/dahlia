
add_library(dahlia_lib STATIC
    src/CharReader.cpp
    src/Position.cpp
)

target_include_directories(dahlia_lib
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)
