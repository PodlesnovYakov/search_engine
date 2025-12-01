cmake_minimum_required(VERSION 3.25)
project(InformationRetrievalSystem CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")


if(APPLE)
  execute_process(
    COMMAND xcrun --sdk macosx --show-sdk-path
    OUTPUT_VARIABLE CMAKE_OSX_SYSROOT
  )
  string(STRIP "${CMAKE_OSX_SYSROOT}" CMAKE_OSX_SYSROOT)
endif()

# Если у вас есть папка cmake/ с этим файлом, раскомментируйте
# include(cmake/BuildFlags.cmake)

# --- ИЩЕМ OPENSSL ---
# Эта команда найдет установленные в системе библиотеки SSL и Crypto
find_package(OpenSSL REQUIRED)

# --- Сборка библиотеки ---
add_library(search_lib
    lib/src/Tokenizer.cpp
    lib/src/Index.cpp
    lib/src/SearchEngine.cpp
)
target_include_directories(search_lib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/include"
)

# --- Сборка сервера ---
add_executable(search_server server/main.cpp)

# Указываем, где искать httplib.h и json.hpp
target_include_directories(search_server PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/server/third_party"
)

# --- ЛИНКУЕМ СЕРВЕР С БИБЛИОТЕКАМИ ---
# Теперь мы линкуем не только с нашей search_lib, но и с OpenSSL
target_link_libraries(search_server PRIVATE
    search_lib
    OpenSSL::SSL
    OpenSSL::Crypto
)