cmake_minimum_required(VERSION 3.22)

project(new_form_data_tests)

add_executable(test_form_data
    test_form_data.cpp
)

# Include new headers
target_include_directories(test_form_data PRIVATE
    ${CMAKE_SOURCE_DIR}/new-include
)

if(TARGET unet_form_data)
    target_link_libraries(test_form_data PRIVATE unet_form_data)
endif()

# Simple test run target
add_custom_target(run_form_data_tests
    COMMAND test_form_data
    DEPENDS test_form_data
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
