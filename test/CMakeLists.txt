include(FetchContent)

FetchContent_Declare(
   doctest
   GIT_REPOSITORY https://github.com/doctest/doctest
   GIT_TAG v2.5.1
)
FetchContent_MakeAvailable(doctest)

add_executable(dahlia_test
    main.cpp
    CharReaderTest.cpp
    LexerTest.cpp
    ParserTest.cpp
)

target_link_libraries(dahlia_test
    PRIVATE doctest::doctest
    PRIVATE dahlia_lib)

add_test(NAME dahlia_test COMMAND dahlia_test)

target_include_directories(dahlia_test PRIVATE
        ${CMAKE_SOURCE_DIR}/lib
        ${CMAKE_SOURCE_DIR}/lib/include)
