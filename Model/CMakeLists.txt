cmake_minimum_required(VERSION 3.15)
set(project_name Model)
project(${project_name})


file(GLOB_RECURSE Headers CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.h"
)
file(GLOB_RECURSE Sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
)

source_group("Header" FILES ${Headers})
source_group("Source" FILES ${Sources})

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_library(${project_name} SHARED
	${Headers}
	${Sources}
)

target_compile_definitions(${project_name} PRIVATE META_DLL_EXPORTS)
target_compile_features(${project_name} PUBLIC cxx_std_20)
target_include_directories(${project_name} PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/Include
)
target_include_directories(${project_name} PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/Source
	${CMAKE_CURRENT_SOURCE_DIR}/../Core/ThirdParty
)
target_link_libraries(${project_name} PUBLIC Share)
if(WIN32)
	target_link_libraries(${project_name} PRIVATE ole32)
endif()

if(MSVC)
	target_compile_options(${project_name} PRIVATE /utf-8)
endif()
