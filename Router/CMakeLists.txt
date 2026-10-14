cmake_minimum_required(VERSION 3.15)

set(Project_Name "Router")

project(${Project_Name})

file(GLOB_RECURSE Sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
)

file(GLOB_RECURSE Headers CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.h"
)

source_group("Header" FILES ${Headers})
source_group("Source" FILES ${Sources})


set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_library(${Project_Name} SHARED
	${Headers}
	${Sources}
)

target_compile_definitions(${Project_Name} PRIVATE ROUTER_DLL_EXPORTS)
target_compile_features(${Project_Name} PUBLIC cxx_std_20)
target_include_directories(${Project_Name} PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/Include
)
target_include_directories(${Project_Name} PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/Source
	$<TARGET_PROPERTY:Core,BINARY_DIR>/$<CONFIG>
)
target_link_libraries(${Project_Name} PUBLIC Core)

if(MSVC)
	target_compile_options(${Project_Name} PRIVATE /utf-8)
endif()
