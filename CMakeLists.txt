cmake_minimum_required(VERSION 3.13.4)

project(converter)

add_executable(${PROJECT_NAME} converter.cpp sdf.cpp)
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "../")


set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

#find_package(nlohmann_json 3.2.0 REQUIRED)
#target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)

find_package(nlohmann_json_schema_validator REQUIRED)
target_link_libraries(${PROJECT_NAME} nlohmann_json_schema_validator)

find_package(LibYANG REQUIRED)
set(LIBS ${LIBS} ${LIBYANG_LIBRARIES})
include_directories(${LIBYANG_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${LIBS})
