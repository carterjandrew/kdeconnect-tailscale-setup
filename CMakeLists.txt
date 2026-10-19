cmake_minimum_required(VERSION 3.22)

project(kdeconnect-tailscale-setup VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core DBus Network)

add_library(kdeconnect_tailscale_core
    src/core.cpp
    src/core.h
    src/kdeconnectclient.cpp
    src/kdeconnectclient.h
    src/state.cpp
    src/state.h
    src/tailscaleclient.cpp
    src/tailscaleclient.h
)
target_link_libraries(kdeconnect_tailscale_core PUBLIC Qt6::Core Qt6::DBus Qt6::Network)
target_include_directories(kdeconnect_tailscale_core PUBLIC src)

add_executable(kdeconnect-tailscale src/main.cpp)
target_link_libraries(kdeconnect-tailscale PRIVATE kdeconnect_tailscale_core)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    add_executable(test_core tests/test_core.cpp)
    target_link_libraries(test_core PRIVATE kdeconnect_tailscale_core Qt6::Test)
    add_test(NAME core COMMAND test_core)
endif()

install(TARGETS kdeconnect-tailscale RUNTIME DESTINATION bin)
