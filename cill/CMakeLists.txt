cmake_minimum_required(VERSION 3.22)
project(cill)


set(CILL_INC ${CMAKE_CURRENT_LIST_DIR})
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

file(GLOB_RECURSE CILL_CXX_SRC ${CMAKE_CURRENT_LIST_DIR}/*.cc)
file(GLOB_RECURSE CILL_C_SRC ${CMAKE_CURRENT_LIST_DIR}/*.c)
list(APPEND CPP_FILES ${CILL_CXX_SRC} ${CILL_C_SRC})

add_library(${PROJECT_NAME} STATIC 
    ${CPP_FILES}
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CILL_INC}/thirdparty/spdlog/include
    ${CILL_INC}/thirdparty/nlohmann_json/include
    ${CILL_INC}/websocket/include
    ${CILL_INC}/alsaaudio/include
    ${CILL_INC}/opus/include
    ${CILL_INC}/filestream/include
    ${CILL_INC}/http/include
    ${CILL_INC}/json/include
    ${CILL_INC}/log/include
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    ${CURL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    m
    rt
    dl
    opus
    asound
    Threads::Threads
    websockets
    ssl
)
