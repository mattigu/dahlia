add_executable(dahlia
    main.cpp
)

target_link_libraries(dahlia
    PRIVATE
        dahlia_lib
)