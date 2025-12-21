cmake_minimum_required(VERSION 3.22..3.29) #3.24
project(server VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} -O3")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3")

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0 -fpic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -fpic")

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

#set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer" CACHE STRING "Debug flags" FORCE)
#set(CMAKE_C_FLAGS_DEBUG "-g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer" CACHE STRING "Debug flags" FORCE)
#set(CMAKE_EXE_LINKER_FLAGS_DEBUG "-fsanitize=address,undefined" CACHE STRING "Linker flags" FORCE)
#set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "-fsanitize=address,undefined" CACHE STRING "Linker flags" FORCE)

# ========================================
# ======= BUILDING LIBRARY =============
# ========================================

# Find uvent
include(FetchContent)

FetchContent_Declare(
    uvent 
    GIT_REPOSITORY https://github.com/Usub-development/uvent.git
    GIT_TAG main
)

FetchContent_MakeAvailable(uvent)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

file(GLOB_RECURSE MYLIBRARY_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")

add_library(server

    # Components/Compression
    src/Components/Compression/gzip.cpp

    # Components/Encodings
    src/Components/Encodings/PercentEncoded.cpp

    # Components/Headers
    src/Protocols/HTTP/Headers.cpp

    # Components/URL
    src/Components/URL/QueryParams.cpp
    src/Components/URL/URL.cpp

    #Components/DataTypes
    src/Components/DataTypes/Multipart/MultipartFormData.cpp

    # Protocols/HTTP
    src/Protocols/HTTP/EndpointHandler.cpp
    src/Protocols/HTTP/RouterCommon.cpp
    src/Protocols/HTTP/RadixRouter.cpp
    
    src/Protocols/HTTP/headers_lookup.cpp
    src/Protocols/HTTP/HTTP1.cpp
    src/Protocols/HTTP/Message.cpp
    src/Protocols/HTTP/Middlewares.cpp

    # Protocols/websocket
    # Protocols/websocket/Websocket.cpp
    # Protocols/websocket/WebsocketBase.cpp

    # server
    # server/client_handler.cpp
    src/server/server.cpp

    # utils
    src/utils/utils.cpp
    src/utils/configuration/ConfigReader.cpp
    src/utils/crypto/SHA1.cpp
    # utils/ssl/data.cpp
    src/utils/toml/toml.cpp
    src/utils/HTTPUtils/HTTPUtils.cpp
)
add_library(usub::server ALIAS server)


if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug mode enabled")
    #    add_definitions(-DUVENT_DEBUG)
    #
    #    include(FetchContent)
    #    FetchContent_Declare(
    #            spdlog
    #            GIT_REPOSITORY https://github.com/gabime/spdlog.git
    #            GIT_TAG v1.14.1
    #            FIND_PACKAGE_ARGS
    #    )
    #    FetchContent_MakeAvailable(spdlog)
endif ()


#add_dependencies(${CURRENT_PROJECT_NAME} run_tests)
FetchContent_Declare(
    ZLIB
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG v1.3.1.2
    FIND_PACKAGE_ARGS
)

FetchContent_MakeAvailable(ZLIB)


if (CMAKE_BUILD_TYPE STREQUAL "Debug")

    target_link_libraries(server PRIVATE
            ZLIB::ZLIB
            usub::uvent
            #            spdlog::spdlog
    )

else()
    target_link_libraries(server PUBLIC
            ZLIB::ZLIB
            usub::uvent
    )
endif ()

# --------------------
# INSTALL
# -------------------

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

target_include_directories(server 
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/server>
)

configure_package_config_file(
    cmake/serverConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/serverConfig.cmake
    INSTALL_DESTINATION lib/cmake/server
)

install(TARGETS server
    EXPORT serverTargets
    #LIBRARY DESTINATION lib
    #ARCHIVE DESTINATION lib
    #RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/server
    INCLUDES DESTINATION include/server
)

foreach(header ${MYLIBRARY_HEADERS})
    get_filename_component(header_path ${header} DIRECTORY)
    file(RELATIVE_PATH header_relative_path "${CMAKE_CURRENT_SOURCE_DIR}/include" ${header_path})
    install(FILES ${header} DESTINATION include/server/${header_relative_path})
endforeach()

install(EXPORT serverTargets
    FILE serverConfig.cmake
    NAMESPACE usub::
    DESTINATION lib/cmake/server
)


# ========================================
# ======= BUILDING EXAMPLES =============
# ========================================

option(BUILD_EXAMPLES "Build the Server example executables" OFF)

if(BUILD_EXAMPLES)
    add_executable(SimpleServer examples/SimpleServer.cpp)
    add_executable(SimpleClient examples/SimpleClient.cpp)

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_libraries(SimpleServer server ZLIB::ZLIB pthread usub::uvent) # spdlog::spdlog
        target_link_libraries(SimpleClient server ZLIB::ZLIB pthread usub::uvent) # spdlog::spdlog
    else()
        target_link_libraries(SimpleServer server ZLIB::ZLIB pthread usub::uvent)
        target_link_libraries(SimpleClient server ZLIB::ZLIB pthread usub::uvent)
    endif()
endif()

# ========================================
# ======= BUILDING TESTS =============
# ========================================

# PunycodeEncoding tests

option(BUILD_TESTS "Build and run tests" OFF)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
