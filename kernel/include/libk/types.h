#ifndef TYPES_H
#define TYPES_H

// Standard integer types
#ifndef __STDINT_H
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;
typedef uint32_t uintptr_t;
#endif

// Size types (32-bit)
typedef unsigned int size_t;
typedef signed int ssize_t;

// Boolean
typedef uint8_t bool;
#define true 1
#define false 0

// NULL pointer
#ifndef NULL
#define NULL ((void *)0)
#endif

#endif // TYPES_H
