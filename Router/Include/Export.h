#pragma once

#if defined(_WIN32)
#if defined(ROUTER_DLL_EXPORTS)
#define ROUTER_API __declspec(dllexport)
#else
#define ROUTER_API __declspec(dllimport)
#endif
#else
#define ROUTER_API
#endif
