#pragma once

#if defined(_WIN32)
#if defined(MACRO_SHARED_EXPORTS)
#define MACRO_SHARED_API __declspec(dllexport)
#else
#define MACRO_SHARED_API __declspec(dllimport)
#endif
#else
#if defined(MACRO_SHARED_EXPORTS)
#define MACRO_SHARED_API __attribute__((visibility("default")))
#else
#define MACRO_SHARED_API
#endif
#endif
