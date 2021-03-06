/***************************************************************************

    mamecore.h

    General core utilities and macros used throughout MAME.

    Copyright (c) 1996-2007, Nicola Salmoria and the MAME Team.
    Visit http://mamedev.org for licensing and usage restrictions.

***************************************************************************/

#pragma once

#ifndef __MAMECORE_H__
#define __MAMECORE_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "osdcomm.h"
#include "bitmap.h"
#include "coreutil.h"
#include "corestr.h"


/***************************************************************************
    COMPILER-SPECIFIC NASTINESS
***************************************************************************/

/* Suppress warnings about redefining the macro 'PPC' on LinuxPPC. */
#ifdef PPC
#undef PPC
#endif

/* Suppress warnings about redefining the macro 'ARM' on ARM. */
#ifdef ARM
#undef ARM
#endif



/***************************************************************************
    COMMON TYPES
***************************************************************************/

/* genf is a type that can be used for function pointer casting in a way
   that doesn't confuse some compilers */
typedef void genf(void);


/* FPTR is a type that can be used to cast a pointer to a scalar */
/* 64-bit platforms should define PTR64 */
#ifdef PTR64
typedef UINT64 FPTR;
#else
typedef UINT32 FPTR;
#endif


/* These are forward struct declarations that are used to break
   circular dependencies in the code */
typedef struct _running_machine running_machine;
typedef struct _mame_display mame_display;
typedef struct _game_driver game_driver;
typedef struct _machine_config machine_config;
typedef struct _rom_load_data rom_load_data;
typedef struct _osd_create_params osd_create_params;
typedef struct _gfx_element gfx_element;
typedef struct _input_port_entry input_port_entry;
typedef struct _input_port_default_entry input_port_default_entry;
typedef struct _mame_file mame_file;


/* pen_t is used to represent pixel values in bitmaps */
typedef UINT32 pen_t;

/* stream_sample_t is used to represent a single sample in a sound stream */
typedef INT32 stream_sample_t;

/* mame_bitmap is just a bitmap_t */
typedef bitmap_t mame_bitmap;



/***************************************************************************
 * Union of UINT8, UINT16 and UINT32 in native endianess of the target
 * This is used to access bytes and words in a machine independent manner.
 * The upper bytes h2 and h3 normally contain zero (16 bit CPU cores)
 * thus PAIR.d can be used to pass arguments to the memory system
 * which expects 'int' really.
***************************************************************************/
typedef union
{
#ifdef LSB_FIRST
	struct { UINT8 l,h,h2,h3; } b;
	struct { UINT16 l,h; } w;
#else
	struct { UINT8 h3,h2,h,l; } b;
	struct { UINT16 h,l; } w;
#endif
	UINT32 d;
} PAIR;


/***************************************************************************
 * Union of UINT8, UINT16, UINT32, and UINT64 in native endianess of
 * the target.  This is used to access bytes and words in a machine
 * independent manner.
***************************************************************************/
typedef union
{
#ifdef LSB_FIRST
	struct { UINT8 l,h,h2,h3,h4,h5,h6,h7; } b;
	struct { UINT16 l,h,h2,h3; } w;
	struct { UINT32 l,h; } d;
#else
	struct { UINT8 h7,h6,h5,h4,h3,h2,h,l; } b;
	struct { UINT16 h3,h2,h,l; } w;
	struct { UINT32 h,l; } d;
#endif
	UINT64 lw;
} PAIR64;



/***************************************************************************
    COMMON CONSTANTS
***************************************************************************/

/* this is not part of the C/C++ standards and is not present on */
/* strict ANSI compilers or when compiling under GCC with -ansi */
#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif


/* orientation of bitmaps */
#define	ORIENTATION_FLIP_X				0x0001	/* mirror everything in the X direction */
#define	ORIENTATION_FLIP_Y				0x0002	/* mirror everything in the Y direction */
#define ORIENTATION_SWAP_XY				0x0004	/* mirror along the top-left/bottom-right diagonal */

#define	ROT0							0
#define	ROT90							(ORIENTATION_SWAP_XY | ORIENTATION_FLIP_X)	/* rotate clockwise 90 degrees */
#define	ROT180							(ORIENTATION_FLIP_X | ORIENTATION_FLIP_Y)	/* rotate 180 degrees */
#define	ROT270							(ORIENTATION_SWAP_XY | ORIENTATION_FLIP_Y)	/* rotate counter-clockwise 90 degrees */


/* giant global string buffer */
#define GIANT_STRING_BUFFER_SIZE		65536



/***************************************************************************
    COMMON MACROS
***************************************************************************/

/* Standard MAME assertion macros */
#undef assert
#undef assert_always

#ifdef MAME_DEBUG
#define assert(x)	do { if (!(x)) fatalerror("assert: %s:%d: %s", __FILE__, __LINE__, #x); } while (0)
#define assert_always(x, msg) do { if (!(x)) fatalerror("Fatal error: %s\nCaused by assert: %s:%d: %s", msg, __FILE__, __LINE__, #x); } while (0)
#else
#define assert(x)
#define assert_always(x, msg) do { if (!(x)) fatalerror("Fatal error: %s (%s:%d)", msg, __FILE__, __LINE__); } while (0)
#endif


/* macros to convert radians to degrees and degrees to radians */
#define RADIAN_TO_DEGREE(x)   ((180.0 / M_PI) * (x))
#define DEGREE_TO_RADIAN(x)   ((M_PI / 180.0) * (x))


/* U64 and S64 are used to wrap long integer constants. */
#ifdef __GNUC__
#define U64(val) val##ULL
#define S64(val) val##LL
#else
#define U64(val) val
#define S64(val) val
#endif


/* Useful macros to deal with bit shuffling encryptions */
#define BIT(x,n) (((x)>>(n))&1)

#define BITSWAP8(val,B7,B6,B5,B4,B3,B2,B1,B0) \
		((BIT(val,B7) << 7) | \
		 (BIT(val,B6) << 6) | \
		 (BIT(val,B5) << 5) | \
		 (BIT(val,B4) << 4) | \
		 (BIT(val,B3) << 3) | \
		 (BIT(val,B2) << 2) | \
		 (BIT(val,B1) << 1) | \
		 (BIT(val,B0) << 0))

#define BITSWAP16(val,B15,B14,B13,B12,B11,B10,B9,B8,B7,B6,B5,B4,B3,B2,B1,B0) \
		((BIT(val,B15) << 15) | \
		 (BIT(val,B14) << 14) | \
		 (BIT(val,B13) << 13) | \
		 (BIT(val,B12) << 12) | \
		 (BIT(val,B11) << 11) | \
		 (BIT(val,B10) << 10) | \
		 (BIT(val, B9) <<  9) | \
		 (BIT(val, B8) <<  8) | \
		 (BIT(val, B7) <<  7) | \
		 (BIT(val, B6) <<  6) | \
		 (BIT(val, B5) <<  5) | \
		 (BIT(val, B4) <<  4) | \
		 (BIT(val, B3) <<  3) | \
		 (BIT(val, B2) <<  2) | \
		 (BIT(val, B1) <<  1) | \
		 (BIT(val, B0) <<  0))

#define BITSWAP24(val,B23,B22,B21,B20,B19,B18,B17,B16,B15,B14,B13,B12,B11,B10,B9,B8,B7,B6,B5,B4,B3,B2,B1,B0) \
		((BIT(val,B23) << 23) | \
		 (BIT(val,B22) << 22) | \
		 (BIT(val,B21) << 21) | \
		 (BIT(val,B20) << 20) | \
		 (BIT(val,B19) << 19) | \
		 (BIT(val,B18) << 18) | \
		 (BIT(val,B17) << 17) | \
		 (BIT(val,B16) << 16) | \
		 (BIT(val,B15) << 15) | \
		 (BIT(val,B14) << 14) | \
		 (BIT(val,B13) << 13) | \
		 (BIT(val,B12) << 12) | \
		 (BIT(val,B11) << 11) | \
		 (BIT(val,B10) << 10) | \
		 (BIT(val, B9) <<  9) | \
		 (BIT(val, B8) <<  8) | \
		 (BIT(val, B7) <<  7) | \
		 (BIT(val, B6) <<  6) | \
		 (BIT(val, B5) <<  5) | \
		 (BIT(val, B4) <<  4) | \
		 (BIT(val, B3) <<  3) | \
		 (BIT(val, B2) <<  2) | \
		 (BIT(val, B1) <<  1) | \
		 (BIT(val, B0) <<  0))

#define BITSWAP32(val,B31,B30,B29,B28,B27,B26,B25,B24,B23,B22,B21,B20,B19,B18,B17,B16,B15,B14,B13,B12,B11,B10,B9,B8,B7,B6,B5,B4,B3,B2,B1,B0) \
		((BIT(val,B31) << 31) | \
		 (BIT(val,B30) << 30) | \
		 (BIT(val,B29) << 29) | \
		 (BIT(val,B28) << 28) | \
		 (BIT(val,B27) << 27) | \
		 (BIT(val,B26) << 26) | \
		 (BIT(val,B25) << 25) | \
		 (BIT(val,B24) << 24) | \
		 (BIT(val,B23) << 23) | \
		 (BIT(val,B22) << 22) | \
		 (BIT(val,B21) << 21) | \
		 (BIT(val,B20) << 20) | \
		 (BIT(val,B19) << 19) | \
		 (BIT(val,B18) << 18) | \
		 (BIT(val,B17) << 17) | \
		 (BIT(val,B16) << 16) | \
		 (BIT(val,B15) << 15) | \
		 (BIT(val,B14) << 14) | \
		 (BIT(val,B13) << 13) | \
		 (BIT(val,B12) << 12) | \
		 (BIT(val,B11) << 11) | \
		 (BIT(val,B10) << 10) | \
		 (BIT(val, B9) <<  9) | \
		 (BIT(val, B8) <<  8) | \
		 (BIT(val, B7) <<  7) | \
		 (BIT(val, B6) <<  6) | \
		 (BIT(val, B5) <<  5) | \
		 (BIT(val, B4) <<  4) | \
		 (BIT(val, B3) <<  3) | \
		 (BIT(val, B2) <<  2) | \
		 (BIT(val, B1) <<  1) | \
		 (BIT(val, B0) <<  0))


#define mame_stricmp		core_stricmp
#define mame_strnicmp		core_strnicmp
#define mame_strdup		core_strdup
#define mame_strwildcmp	core_strwildcmp


/***************************************************************************
    FUNCTION PROTOTYPES
***************************************************************************/

/* Used by assert(), so definition here instead of mame.h */
DECL_NORETURN void CLIB_DECL fatalerror(const char *text, ...) ATTR_PRINTF(1,2) ATTR_NORETURN;
DECL_NORETURN void CLIB_DECL fatalerror_exitcode(int exitcode, const char *text, ...) ATTR_PRINTF(2,3) ATTR_NORETURN;



/***************************************************************************
    INLINE FUNCTIONS
***************************************************************************/

/* convert a series of 32 bits into a float */
INLINE float u2f(UINT32 v)
{
	union {
		float ff;
		UINT32 vv;
	} u;
	u.vv = v;
	return u.ff;
}


/* convert a float into a series of 32 bits */
INLINE UINT32 f2u(float f)
{
	union {
		float ff;
		UINT32 vv;
	} u;
	u.ff = f;
	return u.vv;
}


/* convert a series of 64 bits into a double */
INLINE double u2d(UINT64 v)
{
	union {
		double dd;
		UINT64 vv;
	} u;
	u.vv = v;
	return u.dd;
}


/* convert a double into a series of 64 bits */
INLINE UINT64 d2u(double d)
{
	union {
		double dd;
		UINT64 vv;
	} u;
	u.dd = d;
	return u.vv;
}



/***************************************************************************
    INLINE MATH HELPERS
***************************************************************************/

/* If the OSD layer wants to override these in osd_cpu.h, they can by #defining */
/* the function name to itself or to point to something else */


/* return the number of leading zero bits in a 32-bt value */
#ifndef count_leading_zeros
INLINE UINT32 count_leading_zeros(UINT32 val)
{
	UINT32 count;
	for (count = 0; (INT32)val >= 0; count++) val <<= 1;
	return count;
}
#endif


/* return the number of leading one bits in a 32-bt value */
#ifndef count_leading_ones
INLINE UINT32 count_leading_ones(UINT32 val)
{
	UINT32 count;
	for (count = 0; (INT32)val < 0; count++) val <<= 1;
	return count;
}
#endif


/* perform a 32x32 multiply to 64-bit precision and then shift */
#ifndef fixed_mul_shift
INLINE INT32 fixed_mul_shift(INT32 val1, INT32 val2, UINT8 shift)
{
	return (INT32)(((INT64)val1 * (INT64)val2) >> shift);
}
#endif


/* prevent the use of rand() -- use mame_rand() instead */
#define rand


#endif	/* __MAMECORE_H__ */
