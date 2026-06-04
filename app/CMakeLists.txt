add_executable(dahlia
    main.cpp
)


include(FetchContent)

FetchContent_Declare(
   CLI11
   GIT_REPOSITORY https://github.com/CLIUtils/CLI11
   GIT_TAG v2.6.2
)
FetchContent_MakeAvailable(CLI11)


target_link_libraries(dahlia
    PRIVATE
        dahlia_lib
        CLI11::CLI11
)
