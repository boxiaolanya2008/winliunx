add_executable(winx_tests
    test_backend.c
    ../src/core/backend.c
    ../src/core/pipeline.c
    ../src/core/executor.c
)

target_include_directories(winx_tests PRIVATE ../src)
set_project_warnings(winx_tests)

if(WIN32)
    target_link_libraries(winx_tests PRIVATE user32 gdi32 comdlg32 ole32)
endif()

add_test(NAME winx_backend COMMAND winx_tests)
