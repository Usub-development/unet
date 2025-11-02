cmake_minimum_required(VERSION 3.30)
project(upqWebserverTest)

set(CMAKE_CXX_STANDARD 23)

include(FetchContent)

FetchContent_Declare(
        server
        GIT_REPOSITORY https://github.com/Usub-development/webserver.git
        GIT_TAG uvent302
        OVERRIDE_FIND_PACKAGE
)
FetchContent_Declare(
        uvent
        GIT_REPOSITORY https://github.com/Usub-development/uvent.git
        GIT_TAG main
)
FetchContent_Declare(
        upq
        GIT_REPOSITORY https://github.com/Usub-development/upq.git
        GIT_TAG main
        OVERRIDE_FIND_PACKAGE
)

FetchContent_MakeAvailable(server)
FetchContent_MakeAvailable(uvent)
FetchContent_MakeAvailable(upq)

add_executable(upqWebserverTest main.cpp
        main.cpp
        main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE
        usub::server
        usub::uvent
        usub::upq
)