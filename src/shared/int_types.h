#ifndef _INT_TYPES_H
#define _INT_TYPES_H

#ifdef _MSC_VER

typedef unsigned __int8 uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;

#else

#include <stdint.h>

#endif //_MSC_VER

#endif //_INT_TYPES_H