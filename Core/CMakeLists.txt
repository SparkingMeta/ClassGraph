cmake_minimum_required(VERSION 3.15)
project(Core)

set(Base_Header
	./Include/Base/VCore.h
	./Include/Base/VClass.h
	./Include/Base/VObject.h
	./Include/Base/VTraits.h
	./Include/Base/VType.h
	./Include/Base/VRegister.h
	./Include/Base/VSystem.h
	./Include/Base/VGuid.h
	./Include/Base/VMeta.h
	./Include/Base/VSerialize.h
)
set(Base_Src
	./Source/Base/VClass.cpp
	./Source/Base/VObject.cpp
	./Source/Base/VType.cpp
	./Source/Base/VRegister.cpp
	./Source/Base/VSystem.cpp
	./Source/Base/VGuid.cpp
	./Source/Base/VMeta.cpp
	./Source/Base/VSerialize.cpp
)

set(Abstract_Header
	./Include/Abstract/ACaller.h
	./Include/Abstract/AFunction.h
	./Include/Abstract/AOperator.h
	./Include/Abstract/AController.h
	./Include/Abstract/AFrame.h
	./Include/Abstract/AProperty.h
	./Include/Abstract/AContext.h
	./Include/Abstract/AVariant.h
	./Include/Abstract/AAsset.h
	./Include/Abstract/ABlueprintObject.h
	./Include/Abstract/ANode.h
	./Include/Abstract/APin.h
	./Include/Abstract/ALine.h
	./Include/Abstract/ATask.h
	./Include/Abstract/AFlow.h
	./Include/Abstract/ABlueprint.h
	./Include/Abstract/AGraph.h
	./Include/Abstract/AScript.h
	./Include/Abstract/AScheduler.h
)

set(Abstract_Src
	./Source/Abstract/ACaller.cpp
	./Source/Abstract/AOperator.cpp
	./Source/Abstract/AController.cpp
)

set(Realistic_Header
	./Include/Realistic/VContext.h
	./Include/Realistic/VFrame.h
	./Include/Realistic/VFunction.h
	./Include/Realistic/VVarFunction.h
	./Include/Realistic/VOperator.h
	./Include/Realistic/VController.h
	./Include/Realistic/VProperty.h
	./Include/Realistic/VAsset.h
	./Include/Realistic/VBlueprintObject.h
	./Include/Realistic/VNode.h
	./Include/Realistic/VVariant.h
	./Include/Realistic/VPin.h
	./Include/Realistic/VLine.h
	./Include/Realistic/VTask.h
	./Include/Realistic/VFlow.h
	./Include/Realistic/VScript.h
	./Include/Realistic/VGraph.h
	./Include/Realistic/VScheduler.h
)

set(Realistic_Src
	./Source/Realistic/VContext.cpp
	./Source/Realistic/VFrame.cpp
	./Source/Realistic/VFunction.cpp
	./Source/Realistic/VVarFunction.cpp
	./Source/Realistic/VOperator.cpp
	./Source/Realistic/VController.cpp
	./Source/Realistic/VProperty.cpp
	./Source/Realistic/VAsset.cpp
	./Source/Realistic/VBlueprintObject.cpp
	./Source/Realistic/VNode.cpp
	./Source/Realistic/VVariant.cpp
	./Source/Realistic/VPin.cpp
	./Source/Realistic/VLine.cpp
	./Source/Realistic/VTask.cpp
	./Source/Realistic/VFlow.cpp
	./Source/Realistic/VScript.cpp
	./Source/Realistic/VGraph.cpp
	./Source/Realistic/VScheduler.cpp
)

set(CodeGen_Header
	./Include/Codegen/CNode.h
	./Include/Codegen/CContext.h
	./Include/Codegen/CGraph.h
	./Include/Codegen/CHandler.h
	./Include/Codegen/CCompiler.h
	./Include/Codegen/CEmitter.h
)

set(CodeGen_Src
	./Source/Codegen/CNode.cpp
	./Source/Codegen/CContext.cpp
	./Source/Codegen/CGraph.cpp
	./Source/Codegen/CHandler.cpp
	./Source/Codegen/CCompiler.cpp
	./Source/Codegen/CEmitter.cpp
)

source_group("Base/Header" FILES ${Base_Header})
source_group("Base/Src" FILES ${Base_Src})

source_group("Abstract/Header" FILES ${Abstract_Header})
source_group("Abstract/Src" FILES ${Abstract_Src})

source_group("Realistic/Header" FILES ${Realistic_Header})
source_group("Realistic/Src" FILES ${Realistic_Src})

source_group("CodeGen/Header" FILES ${CodeGen_Header})
source_group("CodeGen/Src" FILES ${CodeGen_Src})

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_library(Core SHARED
	${Base_Header}
	${Base_Src}
	${Abstract_Header}
	${Abstract_Src}
	${Realistic_Header}
	${Realistic_Src}
	${CodeGen_Header}
	${CodeGen_Src}
)

target_compile_definitions(Core PRIVATE CORE_DLL_EXPORTS)
target_compile_features(Core PUBLIC cxx_std_20)
target_include_directories(Core PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/Include
	${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty
)
target_link_libraries(Core PUBLIC Share)

if(NOT CMAKE_GENERATOR_PLATFORM AND CMAKE_VS_PLATFORM_NAME)
	set(_classgraph_plat "${CMAKE_VS_PLATFORM_NAME}")
elseif(CMAKE_GENERATOR_PLATFORM)
	set(_classgraph_plat "${CMAKE_GENERATOR_PLATFORM}")
else()
	set(_classgraph_plat "x64")
endif()

# Build-time paths owned by Core. Host paths stay in App.
file(GENERATE
	OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/classgraph_core_paths.h"
	CONTENT "#pragma once
#define CLASSGRAPH_GENERATED_ROOT R\"CLASSGRAPH(${CMAKE_CURRENT_SOURCE_DIR}/Include)CLASSGRAPH\"
#define CLASSGRAPH_CORE_INCLUDE R\"CLASSGRAPH(${CMAKE_CURRENT_SOURCE_DIR}/Include)CLASSGRAPH\"
#define CLASSGRAPH_CORE_LIB R\"CLASSGRAPH($<TARGET_LINKER_FILE:Core>)CLASSGRAPH\"
#define CLASSGRAPH_CMAKE_COMMAND R\"CLASSGRAPH(${CMAKE_COMMAND})CLASSGRAPH\"
#define CLASSGRAPH_CMAKE_GENERATOR R\"CLASSGRAPH(${CMAKE_GENERATOR})CLASSGRAPH\"
#define CLASSGRAPH_CMAKE_PLATFORM R\"CLASSGRAPH(${_classgraph_plat})CLASSGRAPH\"
#define CLASSGRAPH_BUILD_CONFIG R\"CLASSGRAPH($<CONFIG>)CLASSGRAPH\"
"
)
target_include_directories(Core PRIVATE
	"${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
)
