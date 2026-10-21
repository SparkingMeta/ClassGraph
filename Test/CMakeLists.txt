
cmake_minimum_required(VERSION 3.15)
set(Project_Name Tests)
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

add_executable(${Project_Name}
	${Headers}
	${Sources}
)

target_compile_features(${Project_Name} PUBLIC cxx_std_20)
target_link_libraries(${Project_Name} PRIVATE Core Engine)
target_include_directories(${Project_Name} PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/Include
	${CMAKE_CURRENT_SOURCE_DIR}/../Core/ThirdParty
)

add_test(NAME BlueprintCoreTests COMMAND ${Project_Name})

if(MSVC)
	target_compile_options(${Project_Name} PRIVATE /utf-8)
endif()
