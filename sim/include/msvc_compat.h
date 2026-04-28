#pragma once

#ifdef _MSC_VER
#ifdef __cplusplus
#include <cstring>
#else
#include <string.h>
#endif
#include <intrin.h>

#ifndef memcpy_P
#define memcpy_P(dest, src, n) memcpy((dest), (src), (n))
#endif

#ifndef __builtin_bswap16
#define __builtin_bswap16 _byteswap_ushort
#endif

#ifndef uint
typedef unsigned int uint;
#endif
#endif
