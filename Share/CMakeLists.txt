find_package(Threads REQUIRED)

file(GLOB_RECURSE Share_Sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
)

file(GLOB_RECURSE Share_Headers CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.h"
)

add_library(Share SHARED
  ${Share_Headers}
  ${Share_Sources}
)

source_group("Header" FILES ${Share_Headers})
source_group("Source" FILES ${Share_Sources})

target_compile_definitions(Share PRIVATE MACRO_SHARED_EXPORTS)
target_compile_features(Share PUBLIC cxx_std_20)

target_include_directories(Share
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Include>
    $<INSTALL_INTERFACE:Include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_link_libraries(Share PUBLIC Threads::Threads)

if(MSVC)
  target_compile_options(Share PRIVATE /utf-8)
endif()

set_target_properties(Share PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
  POSITION_INDEPENDENT_CODE ON
)
