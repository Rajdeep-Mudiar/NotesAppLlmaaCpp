cmake_minimum_required(VERSION 3.20)

project(second_brain_notes_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(second_brain_server
    server.cpp
    core/ai_engine.cpp
    services/notes_service.cpp
    services/ai_service.cpp
)

target_include_directories(second_brain_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../llama.cpp/vendor
)

if (WIN32)
    target_link_libraries(second_brain_server PRIVATE ws2_32)
endif()