#pragma once

#ifdef _MSC_VER


#define HECK_DLLIMPORT __declspec(dllimport)
#define HECK_DLLEXPORT __declspec(dllexport)
#define HECK_VECTORCALL __vectorcall
#else
#define HECK_DLLIMPORT [[gnu::visibility("default")]]
#define HECK_DLLEXPORT [[gnu::visibility("default")]]
#define HECK_VECTORCALL
#endif

#if !defined(__clang__) and !defined(__GNUC__)
#define HECK_NOINLINE [[msvc::noinline]]
#define HECK_FORCEINLINE [[msvc::forceinline]] inline

#define HECK_NOINLINE_PRE HECK_NOINLINE
#define HECK_FORCEINLINE_PRE HECK_FORCEINLINE

#else
#define HECK_NOINLINE [[gnu::noinline]]
#define HECK_FORCEINLINE [[gnu::always_inline]] inline
// Under VS clang-cl, _MSC_VER is still defined and the MSVC std lib is used, but the SIMD intrinsics come from clang.
#define HECK_USING_GCC_SIMD

#define HECK_NOINLINE_PRE HECK_NOINLINE
#define HECK_FORCEINLINE_PRE HECK_FORCEINLINE

#endif


// What a mess... https://stackoverflow.com/questions/65509613/forcing-inlining-of-lambda-in-msvc-c/79686946#79686946
// Each compiler wants it it's own way.
#ifndef HECK_NOINLINE_PRE
#define HECK_NOINLINE_PRE
#endif
#ifndef HECK_FORCEINLINE_PRE
#define HECK_FORCEINLINE_PRE
#endif

#ifndef HECK_NOINLINE_POST
#define HECK_NOINLINE_POST
#endif
#ifndef HECK_FORCEINLINE_POST
#define HECK_FORCEINLINE_POST
#endif