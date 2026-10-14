#include "Base/VGuid.h"

std::string GenerateGUID(const std::string& guid)
{
	SGuid sguid;
	if (!SGuid::FromString(guid, sguid)) {
		return SGuid::New().ToString();
	}
	return guid;
}


SGuid SGuid::New() {
	SGuid g;
	(void)CoCreateGuid(&g.data);
	return g;
}

std::string SGuid::ToString() const {
	wchar_t* wstr = nullptr;
	(void)StringFromCLSID(data, &wstr);
	char buffer[64];
	size_t converted = 0;
	wcstombs_s(&converted, buffer, sizeof(buffer), wstr, _TRUNCATE);
	CoTaskMemFree(wstr);
	return std::string(buffer);
}

bool SGuid::FromString(const std::string& str, SGuid& out) {
	std::wstring wstr(str.begin(), str.end());
	wstr.push_back(L'\0');
	return CLSIDFromString(wstr.c_str(), &out.data) == S_OK;
}

bool SGuid::IsNil() const {
	static const uint8_t zero[16] = { 0 };
	return std::memcmp(&data, zero, sizeof(data)) == 0;
}