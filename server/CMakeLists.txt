cmake_minimum_required(VERSION 3.27)
project(server)

set(CMAKE_CXX_STANDARD 20)

include_directories(
        server
        utils
        session
#        /Users/kirillzhukov/Documents/libs/boringssl/install/include
        /usr/local/include/ngtcp2
)

add_executable(server main.cpp
        server/handler/Server.h
        server/handler/Server.cpp
        server/data/types.h
        server/data/HttpRequest.cpp
        server/data/HttpResponse.cpp
        utils/configuration/ConfigReader.h
        utils/logging/logger.h
        utils/configuration/ConfigReader.cpp
        utils/request/requestTypeStr.cpp
        utils/toml/toml.cpp
        server/loop/loop.h
        server/handler/BasicEndpointHandler.cpp
        server/loop/loop.cpp
        server/handler/Server.cpp
        server/http1/http1.h
        server/http1/http1.cpp
        server/http1/StatusCodeHandler.cpp
        server/http1/StatusCodeHandler.h
        utils/socket/socket.h
        utils/socket/socket.cpp
        utils/globals.h
        utils/globals.cpp
        utils/parser/Http1Parser.cpp
        utils/parser/Http1Parser.h
)

find_package(OpenSSL REQUIRED)
find_package(Libevent REQUIRED)

target_link_libraries(${PROJECT_NAME}
        OpenSSL::SSL
        libevent::core
        libevent::extra
        libevent::openssl
        libevent::pthreads
        /usr/local/lib/libnghttp2.a
#        /Users/kirillzhukov/Documents/libs/boringssl/install/lib/libcrypto.a
#        /Users/kirillzhukov/Documents/libs/boringssl/install/lib/libssl.a
#        /usr/local/lib/libngtcp2.dylib
#        /usr/local/lib/libngtcp2_crypto_boringssl.a
)