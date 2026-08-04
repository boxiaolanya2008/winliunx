function(set_project_warnings target)
    if(NOT MSVC)
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wconversion
            -Wcast-align
            -Wwrite-strings
            -Wstrict-prototypes
            -Wmissing-prototypes
            -Wpointer-arith
            -Werror=implicit-function-declaration
            -Werror=return-type
        )
    else()
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /external:W3
        )
    endif()
endfunction()
