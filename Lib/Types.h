#pragma once

/**
 * @version 170416
 *
 * @brief   Common types.
 */

#ifndef TYPES_H
#define TYPES_H

// System definition files.
//
#include <stdint.h>

#define KB 1024
#define MB 1024 * 1024
#define GB 1024 * 1024 * 1024

typedef uint8_t             uint8;
typedef int8_t              sint8;
typedef uint16_t            uint16;
typedef int16_t             sint16;
typedef uint32_t            uint32;
typedef int32_t             sint32;
typedef uint64_t            uint64;
typedef int64_t             sint64;

typedef float               float32;
typedef double              float64;

typedef uint8_t             boolean;

#ifndef TRUE
#define true                1
#define TRUE                1
#endif

#ifndef FALSE
#define false               0
#define FALSE               0
#endif

#ifndef MIN
#define MIN(a, b) (a < b) ? a : b
#endif

#ifndef MAX
#define MAX(a, b) (a > b) ? a : b
#endif

#endif
