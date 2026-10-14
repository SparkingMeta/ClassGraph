set(App_Sources Source/main.cpp)

add_executable(App ${App_Sources})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${App_Sources})

target_link_libraries(App PRIVATE
  UI
  Router
)
target_compile_features(App PRIVATE cxx_std_20)

# Host-owned paths only. Core paths come from MakeCoreHostConfig().
file(GENERATE
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/classgraph_host_paths.h"
  CONTENT "#pragma once
#define CLASSGRAPH_TYPE_OUT_DIR R\"CLASSGRAPH($<TARGET_FILE_DIR:App>/types)CLASSGRAPH\"
#define CLASSGRAPH_ASSET_ROOT R\"CLASSGRAPH(${CMAKE_SOURCE_DIR}/Content/Blueprints)CLASSGRAPH\"
"
)

target_include_directories(App PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
)

if(MSVC)
  target_compile_options(App PRIVATE /utf-8)
endif()

if(WIN32)
  find_package(Qt5 QUIET COMPONENTS Core)
  find_program(WINDEPLOYQT_EXECUTABLE
    NAMES windeployqt
    HINTS "${_qt5Core_install_prefix}/bin"
  )
  if(WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET App POST_BUILD
      COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations
              "$<TARGET_FILE:UI>"
      COMMENT "Deploy Qt runtime for UI"
    )
  endif()
endif()
