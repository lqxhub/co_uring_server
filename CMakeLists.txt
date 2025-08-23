cmake_minimum_required(VERSION 3.31)
project(uring_server)

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")


find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBURING REQUIRED liburing)

add_executable(uring_server main.cpp
        task.h
        io_uring.cpp
        io_uring.h)


target_include_directories(uring_server PRIVATE ${LIBURING_INCLUDE_DIRS})
target_link_libraries(uring_server PRIVATE ${LIBURING_LIBRARIES})
