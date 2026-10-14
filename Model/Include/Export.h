#pragma once

#if defined(_WIN32)
#if defined(META_DLL_EXPORTS)
#define META_API __declspec(dllexport)
#else
#define META_API __declspec(dllimport)
#endif
#else
#define META_API
#endif
