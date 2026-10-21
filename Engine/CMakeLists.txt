cmake_minimum_required(VERSION 3.15)
set(Project_Name Engine)
project(${Project_Name})

file(GLOB_RECURSE Headers CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.h"
)
file(GLOB_RECURSE Sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
)

source_group("Header" FILES ${Headers})
source_group("Source" FILES ${Sources})

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_library(${Project_Name} SHARED
	${Headers}
	${Sources}
)

target_compile_definitions(${Project_Name} PRIVATE ENGINE_DLL_EXPORTS)
target_compile_features(${Project_Name} PUBLIC cxx_std_20)
target_include_directories(${Project_Name} PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/Include
)

target_link_libraries(${Project_Name} PUBLIC Share)
target_link_libraries(${Project_Name} PRIVATE Core)
target_include_directories(${Project_Name} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>")

if(NOT CMAKE_GENERATOR_PLATFORM AND CMAKE_VS_PLATFORM_NAME)
	set(_classgraph_plat "${CMAKE_VS_PLATFORM_NAME}")
elseif(CMAKE_GENERATOR_PLATFORM)
	set(_classgraph_plat "${CMAKE_GENERATOR_PLATFORM}")
else()
	set(_classgraph_plat "x64")
endif()


file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/core_paths.h"
	CONTENT "#pragma once
#define CLASSGRAPH_GENERATED_ROOT R\"CLASSGRAPH(${CMAKE_CURRENT_SOURCE_DIR}/../Core/Include)CLASSGRAPH\"
#define CLASSGRAPH_CORE_INCLUDE R\"CLASSGRAPH(${CMAKE_CURRENT_SOURCE_DIR}/../Core/Include)CLASSGRAPH\"
#define CLASSGRAPH_SHARE_INCLUDE R\"CLASSGRAPH(${CMAKE_CURRENT_SOURCE_DIR}/../Share/Include)CLASSGRAPH\"
#define CLASSGRAPH_CORE_LIB R\"CLASSGRAPH($<TARGET_LINKER_FILE:Core>)CLASSGRAPH\"
#define CLASSGRAPH_CMAKE_COMMAND R\"CLASSGRAPH(${CMAKE_COMMAND})CLASSGRAPH\"
#define CLASSGRAPH_CMAKE_GENERATOR R\"CLASSGRAPH(${CMAKE_GENERATOR})CLASSGRAPH\"
#define CLASSGRAPH_CMAKE_GENERATOR_INSTANCE R\"CLASSGRAPH(${CMAKE_GENERATOR_INSTANCE})CLASSGRAPH\"
#define CLASSGRAPH_CMAKE_GENERATOR_TOOLSET R\"CLASSGRAPH(${CMAKE_GENERATOR_TOOLSET})CLASSGRAPH\"
#define CLASSGRAPH_CMAKE_PLATFORM R\"CLASSGRAPH(${_classgraph_plat})CLASSGRAPH\"
#define CLASSGRAPH_BUILD_CONFIG R\"CLASSGRAPH($<CONFIG>)CLASSGRAPH\"
"
)

if(MSVC)
	target_compile_options(${Project_Name} PRIVATE /utf-8)
endif()
