cmake_minimum_required(VERSION 3.25)
project(InformationRetrievalSystem CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# Зависимости
find_package(OpenSSL REQUIRED)

# Библиотека
add_library(search_lib
    lib/src/Tokenizer.cpp
    lib/src/Index.cpp
    lib/src/SearchEngine.cpp
)
target_include_directories(search_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/lib/include")

# Индексатор
add_executable(indexer indexer/main.cpp)
target_link_libraries(indexer PRIVATE search_lib)

# Сервер
add_executable(search_server server/main.cpp)
target_include_directories(search_server PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/server/third_party")
target_link_libraries(search_server PRIVATE search_lib OpenSSL::SSL OpenSSL::Crypto)