cmake_minimum_required(VERSION 3.15)
project(UI)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

file(GLOB_RECURSE UI_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
)

file(GLOB_RECURSE UI_HEADERS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.h"
)

source_group("Header" FILES ${UI_HEADERS})
source_group("Source" FILES ${UI_SOURCES})

find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)

add_library(UI SHARED
	${UI_SOURCES}
	${UI_HEADERS}
)

target_compile_definitions(UI PRIVATE MACRO_UI_EXPORTS)
target_compile_features(UI PUBLIC cxx_std_20)

target_include_directories(UI PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/Include
)
target_link_libraries(UI PUBLIC Router Qt5::Core Qt5::Gui Qt5::Widgets)

if(MSVC)
	target_compile_options(UI PRIVATE /utf-8)
endif()

set_target_properties(UI PROPERTIES
	CXX_VISIBILITY_PRESET hidden
	VISIBILITY_INLINES_HIDDEN ON
	POSITION_INDEPENDENT_CODE ON
)
