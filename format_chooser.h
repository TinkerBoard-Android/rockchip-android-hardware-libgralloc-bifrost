/*
 * Copyright (C) 2014 ARM Limited. All rights reserved.
 *
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FORMAT_CHOOSER_H_
#define FORMAT_CHOOSER_H_

#include <hardware/hardware.h>

#define ARM_P010            0x4000000

#define GRALLOC_ARM_INTFMT_EXTENSION_BIT_START     32

/* This format will use AFBC */
#define	    GRALLOC_ARM_INTFMT_AFBC                 (1ULL << (GRALLOC_ARM_INTFMT_EXTENSION_BIT_START+0))

/* This format uses AFBC split block mode */
#define	    GRALLOC_ARM_INTFMT_AFBC_SPLITBLK        (1ULL << (GRALLOC_ARM_INTFMT_EXTENSION_BIT_START+1))

/* 10bit or 16bit, "extended" YUV format bit */
#define     GRALLOC_ARM_INTFMT_EXTENDED_YUV         (1ULL << (GRALLOC_ARM_INTFMT_EXTENSION_BIT_START+2))

/* This format uses AFBC wide block mode */
#define	    GRALLOC_ARM_INTFMT_AFBC_WIDEBLK         (1ULL << (GRALLOC_ARM_INTFMT_EXTENSION_BIT_START+3))

/* 8bit or 10bit, Compressed ARM YUV formats */
#define     GRALLOC_ARM_INTFMT_ARM_AFBC_YUV         (1ULL << (GRALLOC_ARM_INTFMT_EXTENSION_BIT_START+4))

/* Internal format masks */
#define	    GRALLOC_ARM_INTFMT_FMT_MASK             0x00000000ffffffffULL
#define	    GRALLOC_ARM_INTFMT_EXT_MASK             0xffffffff00000000ULL

/* Android pixel format private range start from 0x100 */
#define     GRALLOC_ANDROID_PRIVATE_RANGE_BASE_AFBC            0x100
#define     GRALLOC_ANDROID_PRIVATE_RANGE_BASE_AFBC_SPLITBLK   0x200
#define     GRALLOC_ANDROID_PRIVATE_RANGE_BASE_YUVEXT          0x400
#define     GRALLOC_ANDROID_PRIVATE_RANGE_BASE_AFBC_WIDEBLK    0x800

#define     GRALLOC_PRIVATE_FORMAT_WRAPPER_AFBC(x)             (GRALLOC_ANDROID_PRIVATE_RANGE_BASE_AFBC + (x))
#define     GRALLOC_PRIVATE_FORMAT_WRAPPER_AFBC_SPLITBLK(x)    (GRALLOC_ANDROID_PRIVATE_RANGE_BASE_AFBC_SPLITBLK + (x))
#define     GRALLOC_PRIVATE_FORMAT_WRAPPER_YUVEXT(x)           (GRALLOC_ANDROID_PRIVATE_RANGE_BASE_YUVEXT + (x))
#define     GRALLOC_PRIVATE_FORMAT_WRAPPER_AFBC_WIDEBLK(x)     (GRALLOC_ANDROID_PRIVATE_RANGE_BASE_AFBC_WIDEBLK + (x))

/* Index of the internal formats */
typedef enum
{
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_FIRST,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_RGBA_8888=GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_FIRST,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_RGBX_8888,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_RGB_888,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_RGB_565,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_BGRA_8888,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_sRGB_A_8888,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_sRGB_X_8888,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_YV12,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_Y8,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_Y16,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_NV12,

	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_RGBA_8888_AFBC,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_RGBX_8888_AFBC,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_RGB_888_AFBC,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_RGB_565_AFBC,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_BGRA_8888_AFBC,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_YUV420_8BIT_AFBC,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_YUV422_8BIT_AFBC,

	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_AFBC_SPLITBLK_FIRST,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_RGBA_8888_AFBC_SPLITBLK=GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_AFBC_SPLITBLK_FIRST,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_RGBX_8888_AFBC_SPLITBLK,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_RGB_888_AFBC_SPLITBLK,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_RGB_565_AFBC_SPLITBLK,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_BGRA_8888_AFBC_SPLITBLK,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_YUV420_8BIT_AFBC_SPLITBLK,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_AFBC_SPLITBLK_LAST=GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_YUV420_8BIT_AFBC_SPLITBLK,

	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_AFBC_WIDEBLK_FIRST,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_RGBA_8888_AFBC_SPLITBLK_WIDEBLK=GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_AFBC_WIDEBLK_FIRST,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_RGBX_8888_AFBC_SPLITBLK_WIDEBLK,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_RGB_888_AFBC_SPLITBLK_WIDEBLK,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_RGB_565_AFBC_WIDEBLK,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_BGRA_8888_AFBC_SPLITBLK_WIDEBLK,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_YUV420_8BIT_AFBC_WIDEBLK,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_YUV422_8BIT_AFBC_WIDEBLK,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_AFBC_WIDEBLK_LAST=GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_YUV422_8BIT_AFBC_WIDEBLK,

	/* Extended YUV formats */
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_Y0L2,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_P010,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_P210,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_Y210,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_Y410,

	/* Compressed extended YUV formats */
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_YUV420_10BIT_AFBC,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_YUV422_10BIT_AFBC,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_YUV420_10BIT_AFBC_WIDEBLK,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_YUV422_10BIT_AFBC_WIDEBLK,

#if PLATFORM_SDK_VERSION >= 23
	/* Camera specific HAL formats */
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_RAW16,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_RAW12,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_RAW10,
	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_BLOB,
#endif /* PLATFORM_SDK_VERSION >= 23 */

	/* Add more internal formats here */

	GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_LAST
} gralloc_arm_internal_index_format;

typedef enum
{
	/* Having an invalid format catches lists which are initialized with not all entries. */
	GRALLOC_ARM_HAL_FORMAT_INDEXED_INVALID,
	GRALLOC_ARM_HAL_FORMAT_INDEXED_RGBA_8888,
	GRALLOC_ARM_HAL_FORMAT_INDEXED_RGBX_8888,
	GRALLOC_ARM_HAL_FORMAT_INDEXED_RGB_888,
	GRALLOC_ARM_HAL_FORMAT_INDEXED_RGB_565,
	GRALLOC_ARM_HAL_FORMAT_INDEXED_BGRA_8888,
	GRALLOC_ARM_HAL_FORMAT_INDEXED_sRGB_A_8888,
	GRALLOC_ARM_HAL_FORMAT_INDEXED_sRGB_X_8888,
	GRALLOC_ARM_HAL_FORMAT_INDEXED_YV12,
	GRALLOC_ARM_HAL_FORMAT_INDEXED_NV12,
	GRALLOC_ARM_HAL_FORMAT_INDEXED_NV21,
	GRALLOC_ARM_HAL_FORMAT_INDEXED_Y8,
	GRALLOC_ARM_HAL_FORMAT_INDEXED_Y16,
	GRALLOC_ARM_HAL_FORMAT_INDEXED_YCbCr_420_888,

	/* Compressed 8BIT YUV format */
	GRALLOC_ARM_HAL_FORMAT_INDEXED_YUV420_8BIT_AFBC,
	GRALLOC_ARM_HAL_FORMAT_INDEXED_YUV422_8BIT_AFBC,

	/* Extended YUV formats */
	GRALLOC_ARM_HAL_FORMAT_INDEXED_Y0L2,
	GRALLOC_ARM_HAL_FORMAT_INDEXED_P010,
	GRALLOC_ARM_HAL_FORMAT_INDEXED_P210,
	GRALLOC_ARM_HAL_FORMAT_INDEXED_Y210,
	GRALLOC_ARM_HAL_FORMAT_INDEXED_Y410,

	/* Compressed extended YUV formats */
	GRALLOC_ARM_HAL_FORMAT_INDEXED_YUV420_10BIT_AFBC,
	GRALLOC_ARM_HAL_FORMAT_INDEXED_YUV422_10BIT_AFBC,

#if PLATFORM_SDK_VERSION >= 23
	/* Camera specific HAL formats */
	GRALLOC_ARM_HAL_FORMAT_INDEXED_RAW16,
	GRALLOC_ARM_HAL_FORMAT_INDEXED_RAW12,
	GRALLOC_ARM_HAL_FORMAT_INDEXED_RAW10,
	GRALLOC_ARM_HAL_FORMAT_INDEXED_BLOB,
#endif /*  PLATFORM_SDK_VERSION >= 23 */

	GRALLOC_ARM_HAL_FORMAT_INDEXED_LAST
} gralloc_arm_hal_index_format;

#define MAX_COMPATIBLE 3
#define DEFAULT_WEIGHT_SUPPORTED         50
#define DEFAULT_WEIGHT_MOST_PREFERRED    100
#define DEFAULT_WEIGHT_UNSUPPORTED       -1

#define GRALLOC_ANDROID_PRIVATE_IN_RANGE_OF_AFBC(x) \
                        (((x) > GRALLOC_ANDROID_PRIVATE_RANGE_BASE_AFBC && \
                        (x) <= (GRALLOC_ANDROID_PRIVATE_RANGE_BASE_AFBC + 0xff)) || \
                        ((x) == (GRALLOC_ANDROID_PRIVATE_RANGE_BASE_AFBC + HAL_PIXEL_FORMAT_YV12)))
#define GRALLOC_ANDROID_PRIVATE_IN_RANGE_OF_AFBC_SPLITBLK(x) \
                        (((x) > GRALLOC_ANDROID_PRIVATE_RANGE_BASE_AFBC_SPLITBLK && \
                        (x) <= (GRALLOC_ANDROID_PRIVATE_RANGE_BASE_AFBC_SPLITBLK + 0xff)) || \
                        ((x) == ( GRALLOC_ANDROID_PRIVATE_RANGE_BASE_AFBC_SPLITBLK + HAL_PIXEL_FORMAT_YV12)))
#define GRALLOC_ANDROID_PRIVATE_IN_RANGE_OF_AFBC_WIDEBLK(x) \
            (((x) > GRALLOC_ANDROID_PRIVATE_RANGE_BASE_AFBC_WIDEBLK && \
            (x) <= (GRALLOC_ANDROID_PRIVATE_RANGE_BASE_AFBC_WIDEBLK + 0xff)) || \
            ((x) == ( GRALLOC_ANDROID_PRIVATE_RANGE_BASE_AFBC_WIDEBLK + HAL_PIXEL_FORMAT_YV12)))
#define GRALLOC_ANDROID_PRIVATE_IN_RANGE_OF_BASE_YUVEXT(x) \
                        (((x & GRALLOC_ARM_INTFMT_FMT_MASK) >= \
                                (GRALLOC_ARM_HAL_FORMAT_INDEXED_Y0L2 + GRALLOC_ANDROID_PRIVATE_RANGE_BASE_YUVEXT)) && \
                        ((x & GRALLOC_ARM_INTFMT_FMT_MASK) <= \
                                (GRALLOC_ARM_HAL_FORMAT_INDEXED_YUV422_10BIT_AFBC + GRALLOC_ANDROID_PRIVATE_RANGE_BASE_YUVEXT)))

/* Select the underlying format used to implement the HAL_PIXEL_FORMAT_YCbCr_420_888 flexible YUV format. */
#define GRALLOC_MAPPED_HAL_PIXEL_FORMAT_YCbCr_420_888 GRALLOC_ARM_HAL_FORMAT_INDEXED_NV12

typedef struct
{
	/* The internal extended format exported outside of gralloc */
	uint64_t internal_extended_format;

	/* Swizzled versions of the requested format for this internal format */
	gralloc_arm_hal_index_format comp_format_list[MAX_COMPATIBLE];
} internal_fmt_info;


struct hwblk
{
	uint32_t usage;
	int16_t weights[GRALLOC_ARM_HAL_FORMAT_INDEXED_LAST][GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_LAST];
};


typedef struct
{
	void (*blk_init)(struct hwblk *format_matrix, int16_t **array);
	struct hwblk hwblkconf;
} blkinit;


extern void initialize_blk_conf();
extern const internal_fmt_info translate_internal_indexed[GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_LAST];
extern blkinit blklist[];
extern uint32_t blklist_array_size;

#ifdef __cplusplus
extern "C"
{
#endif
	uint64_t gralloc_select_format(int req_format, int usage, int buffer_size);
	int gralloc_get_internal_format(int hal_format);
	void *gralloc_get_internal_info(int *blkconf_size, int *gpu_conf);
#ifdef __cplusplus
}
#endif

#endif /* FORMAT_CHOOSER_H_ */
