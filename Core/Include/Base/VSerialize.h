#pragma once
#include <string>
#include <type_traits>

#include "VCore.h"

namespace CG::Core {

	struct VObject;
	struct VType;

	CORE_API bool ToJson(const VObject* obj, std::string& out);
	CORE_API bool FromJson(const std::string& in, VObject* obj);

	template<typename T>
	bool ToJson(const T& value, std::string& out);

	template<typename T>
	bool FromJson(const std::string& in, T& value);

	#define CLASSGRAPH_JSON_LEAF(T) \
		template<> CORE_API bool ToJson<T>(const T& value, std::string& out); \
		template<> CORE_API bool FromJson<T>(const std::string& in, T& value);

	CLASSGRAPH_JSON_LEAF(bool)
	CLASSGRAPH_JSON_LEAF(int32)
	CLASSGRAPH_JSON_LEAF(int64)
	CLASSGRAPH_JSON_LEAF(uint32)
	CLASSGRAPH_JSON_LEAF(uint64)
	CLASSGRAPH_JSON_LEAF(float)
	CLASSGRAPH_JSON_LEAF(double)
	CLASSGRAPH_JSON_LEAF(std::string)

	#undef CLASSGRAPH_JSON_LEAF

}
