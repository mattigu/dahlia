include(FetchContent)

FetchContent_Declare(
   magic_enum
   GIT_REPOSITORY https://github.com/Neargye/magic_enum
   GIT_TAG v0.9.7
)
FetchContent_MakeAvailable(magic_enum)

add_library(dahlia_lib STATIC
    src/CharReader.cpp
    src/Position.cpp
    src/Lexer.cpp
    src/Token.cpp
    src/Ast.cpp
    src/Interpreter.cpp
    src/Stack.cpp
    src/Value.cpp
    src/Builtins.cpp
)

target_link_libraries(dahlia_lib
    PRIVATE magic_enum::magic_enum
)

target_include_directories(dahlia_lib
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)
