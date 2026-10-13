# Debian multiarch cross toolchain (x86_64 -> aarch64).
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_LIBRARY_ARCHITECTURE aarch64-linux-gnu)
set(PKG_CONFIG_EXECUTABLE /usr/bin/aarch64-linux-gnu-pkg-config CACHE FILEPATH "")
set(CMAKE_PREFIX_PATH /usr/lib/aarch64-linux-gnu/cmake)
set(QT_HOST_PATH /usr CACHE PATH "")
set(QT_HOST_PATH_CMAKE_DIR /usr/lib/aarch64-linux-gnu/cmake CACHE PATH "")
# Programs (moc, python) come from the host, libraries/headers for the target.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
