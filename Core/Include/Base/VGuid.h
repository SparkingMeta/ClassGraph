#pragma once

#include "VCore.h"

std::string GenerateGUID(const std::string& guid = "");

struct SGuid : Struct {
	// 生成新 GUID
	static SGuid New();
	// 转字符串
	std::string ToString() const;
	// 从字符串解析
	static bool FromString(const std::string& str, SGuid& out);
	bool IsNil() const;

	bool operator==(const SGuid& other) const { return std::memcmp(&data, &other.data, sizeof(data)) == 0; }
	bool operator!=(const SGuid& other) const { return !(*this == other); }

private:
	::GUID data;   // 直接存系统 GUID

};