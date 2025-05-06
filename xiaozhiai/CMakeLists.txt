cmake_minimum_required(VERSION 3.22)
project(xiaozhiai)


file(GLOB_RECURSE TEST_SRC ${CMAKE_CURRENT_LIST_DIR}/*.cc)
list(APPEND CPP_FILES ${TEST_SRC})

add_executable(${PROJECT_NAME} ${CPP_FILES})

target_link_libraries(${PROJECT_NAME} PRIVATE
    cill
)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)