#include "Base/VSerialize.h"

#include "Base/VClass.h"
#include "Base/VObject.h"
#include "Base/VType.h"
#include "Abstract/AProperty.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace CG::Core;

namespace {

	json ValueToJson(const VType* type, const void* ptr)
	{
		if (!type || !ptr || !type->serialize_)
			throw std::runtime_error("serialize missing");

		std::string text;
		if (!type->serialize_(const_cast<void*>(ptr), text))
			throw std::runtime_error("serialize failed");

		return json::parse(text);
	}

	void JsonToValue(const VType* type, void* ptr, const json& j)
	{
		if (!type || !ptr || !type->deserialize_)
			throw std::runtime_error("deserialize missing");

		const std::string text = j.dump();
		if (!type->deserialize_(ptr, text))
			throw std::runtime_error("deserialize failed");
	}

	json ObjectToJson(const VObject* obj)
	{
		json j = json::object();
		if (!obj)
			return j;

		const VClass* cls = obj->GetClass();
		if (!cls)
			return j;

		for (const auto& prop : cls->GetProperties()) {
			if (!prop || !prop->type_)
				continue;
			const void* field = reinterpret_cast<const char*>(obj) + prop->offset_;
			j[prop->GetName()] = ValueToJson(prop->type_, field);
		}
		return j;
	}

	void JsonToObject(VObject* obj, const json& j)
	{
		if (!obj || !j.is_object())
			throw std::runtime_error("object json required");

		const VClass* cls = obj->GetClass();
		if (!cls)
			throw std::runtime_error("class missing");

		for (auto it = j.begin(); it != j.end(); ++it) {
			auto prop = cls->GetProperty(it.key());
			if (!prop || !prop->type_)
				continue;
			void* field = reinterpret_cast<char*>(obj) + prop->offset_;
			JsonToValue(prop->type_, field, it.value());
		}
	}

	template<typename T>
	bool LeafToJson(const T& value, std::string& out)
	{
		try {
			out = json(value).dump();
			return true;
		}
		catch (...) {
			return false;
		}
	}

	template<typename T>
	bool LeafFromJson(const std::string& in, T& value)
	{
		try {
			value = json::parse(in).get<T>();
			return true;
		}
		catch (...) {
			return false;
		}
	}

}

bool CG::Core::ToJson(const VObject* obj, std::string& out)
{
	try {
		out = ObjectToJson(obj).dump();
		return true;
	}
	catch (...) {
		return false;
	}
}

bool CG::Core::FromJson(const std::string& in, VObject* obj)
{
	try {
		JsonToObject(obj, json::parse(in));
		return true;
	}
	catch (...) {
		return false;
	}
}

template<>
bool CG::Core::ToJson<bool>(const bool& value, std::string& out) { return LeafToJson(value, out); }
template<>
bool CG::Core::FromJson<bool>(const std::string& in, bool& value) { return LeafFromJson(in, value); }

template<>
bool CG::Core::ToJson<int32>(const int32& value, std::string& out) { return LeafToJson(value, out); }
template<>
bool CG::Core::FromJson<int32>(const std::string& in, int32& value) { return LeafFromJson(in, value); }

template<>
bool CG::Core::ToJson<int64>(const int64& value, std::string& out) { return LeafToJson(value, out); }
template<>
bool CG::Core::FromJson<int64>(const std::string& in, int64& value) { return LeafFromJson(in, value); }

template<>
bool CG::Core::ToJson<uint32>(const uint32& value, std::string& out) { return LeafToJson(value, out); }
template<>
bool CG::Core::FromJson<uint32>(const std::string& in, uint32& value) { return LeafFromJson(in, value); }

template<>
bool CG::Core::ToJson<uint64>(const uint64& value, std::string& out) { return LeafToJson(value, out); }
template<>
bool CG::Core::FromJson<uint64>(const std::string& in, uint64& value) { return LeafFromJson(in, value); }

template<>
bool CG::Core::ToJson<float>(const float& value, std::string& out) { return LeafToJson(value, out); }
template<>
bool CG::Core::FromJson<float>(const std::string& in, float& value) { return LeafFromJson(in, value); }

template<>
bool CG::Core::ToJson<double>(const double& value, std::string& out) { return LeafToJson(value, out); }
template<>
bool CG::Core::FromJson<double>(const std::string& in, double& value) { return LeafFromJson(in, value); }

template<>
bool CG::Core::ToJson<std::string>(const std::string& value, std::string& out) { return LeafToJson(value, out); }
template<>
bool CG::Core::FromJson<std::string>(const std::string& in, std::string& value) { return LeafFromJson(in, value); }
