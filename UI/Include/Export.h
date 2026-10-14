#pragma once

#if defined(_WIN32)
#if defined(MACRO_UI_EXPORTS)
#define MACRO_UI_API __declspec(dllexport)
#else
#define MACRO_UI_API __declspec(dllimport)
#endif
#else
#if defined(MACRO_UI_EXPORTS)
#define MACRO_UI_API __attribute__((visibility("default")))
#else
#define MACRO_UI_API
#endif
#endif
