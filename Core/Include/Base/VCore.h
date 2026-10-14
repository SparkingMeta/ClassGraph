#pragma once
#include <iostream>

#ifdef _WIN32
#include <objbase.h>
#include <combaseapi.h>

#ifdef CORE_DLL_EXPORTS
#define CORE_API __declspec(dllexport)
#else
#define CORE_API __declspec(dllimport)
#endif
#else
#define CORE_API
#endif

#define VCLASS() 

#define BODY_MACRO_COMBINE_INNER(A,B,C,D) A##B##C##D
#define BODY_MACRO_COMBINE(A,B,C,D) BODY_MACRO_COMBINE_INNER(A,B,C,D)

#define GENERATED_BODY(...) BODY_MACRO_COMBINE(CURRENT_FILE_ID,_,__LINE__,_GENERATED_BODY)

#define VMETHOD_FLAG() 

#define CONSTRUCT_VCLASS(name) Z_Construct_VClass_##name()

inline constexpr int kInputFlag = 0;
inline constexpr int kOutputFlag = 1;
inline constexpr int kArgsUpperLimit = 100;

typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t		uint8;
typedef uint16_t	uint16;
typedef uint32_t	uint32;
typedef uint64_t	uint64;

using MethodKey = std::string;
using ClassKey = std::string;

struct Struct {};

struct Result : Struct {
	int code{};
	int index{};
	bool flag{};
};

struct Vec2i {
	int x{ 0 }, y{ 0 };
};

struct Vec2f {
	float x{ 0.f }, y{ 0.f };
};

struct Vec3i {
	int x{ 0 }, y{ 0 }, z{ 0 };
};

struct Vec3f {
	float x{ 0.f }, y{ 0.f }, z{ 0.f };
};

template<typename T>
inline size_t ToHash(size_t seed, T* ptr) {
	seed ^= std::hash<T*>{}(ptr) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	return seed;
}

using Construct = void(*)(void*);
using Destruct = void(*)(void*);

using CopyConstruct = void(*)(void*, const void*);
using MoveConstruct = void(*)(void*, void*);

using CopyAssign = void(*)(void*, const void*);
using MoveAssign = void(*)(void*, void*);

using Serialize = bool(*)(const void*, std::string&);
using Deserialize = bool(*)(void*, const std::string&);
