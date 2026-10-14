cmake_minimum_required(VERSION 3.15)
project(Test)

set(Test_Header
	./Include/Test/TestUtil.h
	./Include/Test/TestCases.h
)

set(Test_Src
	./Source/main.cpp
	./Source/Test/TestUtil.cpp
	./Source/Test/TestFlowMeta.cpp
	./Source/Test/TestVTest.cpp
	./Source/Test/TestMyBranch.cpp
	./Source/Test/TestBranch.cpp
	./Source/Test/TestNodesCodegen.cpp
	./Source/Test/TestBlueprintRun.cpp
	./Source/Test/TestVTestInterop.cpp
	./Source/Test/TestSerialize.cpp
)

# Optional codegen outputs under Core/Include/user (produced by TestVTest).
set(Generated_User_Dir "${CMAKE_SOURCE_DIR}/Core/Include/user")
set(Generated_Src)
foreach(_gen VTest.gen.cpp VTest.cpp)
	if(EXISTS "${Generated_User_Dir}/${_gen}")
		list(APPEND Generated_Src "${Generated_User_Dir}/${_gen}")
	endif()
endforeach()

source_group("Header" FILES ${Test_Header})
source_group("Source" FILES ${Test_Src})
source_group("Generated" FILES ${Generated_Src})

add_executable(Test
	${Test_Header}
	${Test_Src}
	${Generated_Src}
)

target_compile_features(Test PRIVATE cxx_std_20)
target_link_libraries(Test PRIVATE Core)
target_include_directories(Test PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/Include
)

if(MSVC)
	target_compile_options(Test PRIVATE /utf-8)
endif()
