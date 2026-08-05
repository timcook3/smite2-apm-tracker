# Toolchain file for cross-compiling from Linux with MinGW-w64:
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-toolchain.cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Prefer the posix-threads variant, which supports std::thread.
find_program(MINGW_GCC_POSIX x86_64-w64-mingw32-gcc-posix)
find_program(MINGW_GXX_POSIX x86_64-w64-mingw32-g++-posix)
if(MINGW_GCC_POSIX AND MINGW_GXX_POSIX)
    set(CMAKE_C_COMPILER ${MINGW_GCC_POSIX})
    set(CMAKE_CXX_COMPILER ${MINGW_GXX_POSIX})
else()
    set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
    set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
endif()
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
