# Installation

Currently **Webserver** supports **Linux**.
Windows support is not yet available.

## Prerequisites
- **C++23** capable compiler
  - GCC ≥ 12
  - Clang ≥ 15
- **CMake** ≥ 3.22
- (**Uvent**)[https://github.com/Usub-development/uvent.git]

Optional:
- **OpenSSL** (for TLS support)

---

## Build from Source

Clone the repository:
```bash
git clone https://github.com/Usub-development/webserver.git
cd webserver
````

Configure and build:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cd build
make
make install
```

This places the library under `/usr/local/lib` and headers under `/usr/local/include`. You can always specify install path using `-DCMAKE_INSTALL_PREFIX`

---

## Using with CMake

### Option 1: System Install

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_app LANGUAGES CXX)

find_package(webserver REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE webserver)
```

### Option 2: FetchContent (Recommended for development)

You can embed **Webserver** directly into your project without system install:

```cmake
include(FetchContent)

FetchContent_Declare(
    webserver
    GIT_REPOSITORY https://github.com/Usub-development/webserver.git
    GIT_TAG main # or TAG, (vMAJOR.MINOR.REVISION)
)

FetchContent_MakeAvailable(webserver)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE webserver)
```

This way, CMake will automatically fetch and build Webserver during your project’s build.

---

## Next steps

See the [Quick Start](quick-start.md).