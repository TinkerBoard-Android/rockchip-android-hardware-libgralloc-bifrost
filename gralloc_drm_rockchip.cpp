#define LOG_TAG "GRALLOC-ROCKCHIP"

#define ENABLE_DEBUG_LOG
#include <log/custom_log.h>


#include <cutils/log.h>
#include <stdlib.h>
#include <errno.h>
#include <drm.h>

extern "C" {
#include <rockchip/rockchip_drmif.h>
}

#include "gralloc_helper.h"
#include "gralloc_drm.h"
#include "gralloc_drm_priv.h"
#if RK_DRM_GRALLOC
#include <cutils/properties.h>
#if MALI_AFBC_GRALLOC == 1
#include <inttypes.h>
#include "gralloc_buffer_priv.h"
#include "mali_gralloc_formats.h"
#include "mali_gralloc_usages.h"
#endif //end of MALI_AFBC_GRALLOC
#endif //end of RK_DRM_GRALLOC
#include <stdbool.h>

#define UNUSED(...) (void)(__VA_ARGS__)

/* memory type definitions. */
enum drm_rockchip_gem_mem_type {
	/* Physically Continuous memory and used as default. */
	ROCKCHIP_BO_CONTIG	= 0 << 0,
	/* Physically Non-Continuous memory. */
	ROCKCHIP_BO_NONCONTIG	= 1 << 0,
	/* non-cachable mapping and used as default. */
	ROCKCHIP_BO_NONCACHABLE	= 0 << 1,
	/* cachable mapping. */
	ROCKCHIP_BO_CACHABLE	= 1 << 1,
	/* write-combine mapping. */
	ROCKCHIP_BO_WC		= 1 << 2,
	ROCKCHIP_BO_MASK	= ROCKCHIP_BO_NONCONTIG | ROCKCHIP_BO_CACHABLE |
					ROCKCHIP_BO_WC
};

struct rockchip_info {
	struct gralloc_drm_drv_t base;

	struct rockchip_device *rockchip;
	int fd;
};

struct rockchip_buffer {
	struct gralloc_drm_bo_t base;

	struct rockchip_bo *bo;
};

#if RK_DRM_GRALLOC

#define RK_GRALLOC_VERSION "1.0.6"
#define ARM_RELEASE_VER "r13p0-00rel0"

#if RK_DRM_GRALLOC_DEBUG
#ifndef AWAR
#define AWAR(fmt, args...) __android_log_print(ANDROID_LOG_WARN, "[Gralloc-Warning]", "%s:%d " fmt,__func__,__LINE__,##args)
#endif
#ifndef AINF
#define AINF(fmt, args...) __android_log_print(ANDROID_LOG_INFO, "[Gralloc]", fmt,##args)
#endif
#ifndef ADBG
#define ADBG(fmt, args...) __android_log_print(ANDROID_LOG_DEBUG, "[Gralloc-DEBUG]", fmt,##args)
#endif

#else

#ifndef AWAR
#define AWAR(fmt, args...)
#endif
#ifndef AINF
#define AINF(fmt, args...)
#endif
#ifndef ADBG
#define ADBG(fmt, args...)
#endif

#endif //end of RK_DRM_GRALLOC_DEBUG

#ifndef AERR
#define AERR(fmt, args...) __android_log_print(ANDROID_LOG_ERROR, "[Gralloc-ERROR]", "%s:%d " fmt,__func__,__LINE__,##args)
#endif
#ifndef AERR_IF
#define AERR_IF( eq, fmt, args...) if ( (eq) ) AERR( fmt, args )
#endif

#define ODD_ALIGN(x, align)		(((x) % ((align) * 2) == 0) ? ((x) + (align)) : (x))
#define GRALLOC_ODD_ALIGN( value, base )   ODD_ALIGN(GRALLOC_ALIGN(value, base), base)

static void drm_gem_rockchip_free(struct gralloc_drm_drv_t *drv,
		struct gralloc_drm_bo_t *bo);

/*---------------------------------------------------------------------------*/

#define AFBC_PIXELS_PER_BLOCK 16
#define AFBC_HEADER_BUFFER_BYTES_PER_BLOCKENTRY 16

#define AFBC_BODY_BUFFER_BYTE_ALIGNMENT 1024
#define AFBC_NORMAL_WIDTH_ALIGN 16
#define AFBC_NORMAL_HEIGHT_ALIGN 16
#define AFBC_WIDEBLK_WIDTH_ALIGN 32
#define AFBC_WIDEBLK_HEIGHT_ALIGN 16
// Regarding Tiled Headers AFBC mode, both header and body buffer should aligned to 4KB
// and in non-wide mode (16x16), the width and height should be both rounded up to 128
// in wide mode (32x8) the width should be rounded up to 256, the height should be rounded up to 64
#define AFBC_TILED_HEADERS_BASIC_WIDTH_ALIGN 128
#define AFBC_TILED_HEADERS_BASIC_HEIGHT_ALIGN 128
#define AFBC_TILED_HEADERS_WIDEBLK_WIDTH_ALIGN 256
#define AFBC_TILED_HEADERS_WIDEBLK_HEIGHT_ALIGN 64

// This value is platform specific and should be set according to hardware YUV planes restrictions.
// Please note that EGL winsys platform config file needs to use the same value when importing buffers.
#define YUV_MALI_PLANE_ALIGN 128

// Default YUV stride aligment in Android
#define YUV_ANDROID_PLANE_ALIGN 16

/*
 * Type of allocation
 */
typedef enum AllocType
{
	UNCOMPRESSED = 0,
	AFBC,
	/* AFBC_WIDEBLK mode requires buffer to have 32 * 16 pixels alignment */
	AFBC_WIDEBLK,
	/* AN AFBC buffer with additional padding to ensure a 64-bte alignment
	 * for each row of blocks in the header */
	AFBC_PADDED,
	/* AFBC_TILED_HEADERS_AFBC_BASIC mode requires buffer to have 128*128 pixels alignment(16x16 superblocks) */
	AFBC_TILED_HEADERS_BASIC,
	/* AFBC_TILED_HEADERS_AFBC_WIDEBLK mode requires buffer to have 256*64 pixels alignment(32x8 superblocks) */
	AFBC_TILED_HEADERS_WIDEBLK,
}   AllocType;

/*
 * Computes the strides and size for an RGB buffer
 *
 * width               width of the buffer in pixels
 * height              height of the buffer in pixels
 * pixel_size          size of one pixel in bytes
 *
 * pixel_stride (out)  stride of the buffer in pixels
 * byte_stride  (out)  stride of the buffer in bytes
 * size         (out)  size of the buffer in bytes
 * type         (in)   if buffer should be allocated for afbc
 */
static void get_rgb_stride_and_size(int width, int height, int pixel_size, int *pixel_stride, int *byte_stride,
                                    size_t *size, AllocType type)
{
	int stride;

	stride = width * pixel_size;

	/* Align the lines to 64 bytes.
	 * It's more efficient to write to 64-byte aligned addresses because it's the burst size on the bus */
	stride = GRALLOC_ALIGN(stride, 64);

	if (size != NULL)
	{
		*size = stride *height;
	}

	if (byte_stride != NULL)
	{
		*byte_stride = stride;
	}

	if (pixel_stride != NULL)
	{
		*pixel_stride = stride / pixel_size;
	}

	if (type != UNCOMPRESSED)
	{
		int w_aligned;
		int h_aligned = GRALLOC_ALIGN(height, AFBC_NORMAL_HEIGHT_ALIGN);
		int nblocks;
		int buffer_byte_alignment = AFBC_BODY_BUFFER_BYTE_ALIGNMENT;

		if (type == AFBC_TILED_HEADERS_BASIC)
		{
			w_aligned = GRALLOC_ALIGN(width, AFBC_TILED_HEADERS_BASIC_WIDTH_ALIGN);
			h_aligned = GRALLOC_ALIGN(height, AFBC_TILED_HEADERS_BASIC_HEIGHT_ALIGN);
			buffer_byte_alignment = 4 * AFBC_BODY_BUFFER_BYTE_ALIGNMENT;
		}
		else if (type == AFBC_TILED_HEADERS_WIDEBLK)
		{
			w_aligned = GRALLOC_ALIGN(width, AFBC_TILED_HEADERS_WIDEBLK_WIDTH_ALIGN);
			h_aligned = GRALLOC_ALIGN(height, AFBC_TILED_HEADERS_WIDEBLK_HEIGHT_ALIGN);
			buffer_byte_alignment = 4 * AFBC_BODY_BUFFER_BYTE_ALIGNMENT;
		}
		else if (type == AFBC_PADDED)
		{
			w_aligned = GRALLOC_ALIGN(width, 64);
		}
		else if (type == AFBC_WIDEBLK)
		{
			w_aligned = GRALLOC_ALIGN(width, AFBC_WIDEBLK_WIDTH_ALIGN);
			h_aligned = GRALLOC_ALIGN(height, AFBC_WIDEBLK_HEIGHT_ALIGN);
		}
		else
		{
			w_aligned = GRALLOC_ALIGN(width, AFBC_NORMAL_WIDTH_ALIGN);
		}

		stride = w_aligned * pixel_size;
		stride = GRALLOC_ALIGN(stride, 64);

		if (byte_stride != NULL)
		{
			*byte_stride = stride;
		}

		if (pixel_stride != NULL)
		{
			*pixel_stride = stride / pixel_size;
		}

		nblocks = w_aligned / AFBC_PIXELS_PER_BLOCK * h_aligned / AFBC_PIXELS_PER_BLOCK;

		if (size != NULL)
		{
			*size = stride *h_aligned +
			        GRALLOC_ALIGN(nblocks * AFBC_HEADER_BUFFER_BYTES_PER_BLOCKENTRY, buffer_byte_alignment);
		}
	}
}

/*
 * Computes the strides and size for an AFBC 8BIT YUV 4:2:0 buffer
 *
 * width                Public known width of the buffer in pixels
 * height               Public known height of the buffer in pixels
 *
 * pixel_stride   (out) stride of the buffer in pixels
 * byte_stride    (out) stride of the buffer in bytes
 * size           (out) size of the buffer in bytes
 * type                 if buffer should be allocated for a certain afbc type
 * internalHeight (out) The internal height, which may be greater than the public known height.
 */
static bool get_afbc_yuv420_8bit_stride_and_size(int width, int height, int *pixel_stride, int *byte_stride,
                                                 size_t *size, AllocType type, int *internalHeight)
{
	int yuv420_afbc_luma_stride, yuv420_afbc_chroma_stride;
	int buffer_byte_alignment = AFBC_BODY_BUFFER_BYTE_ALIGNMENT;

	*internalHeight = height;

#if MALI_VIDEO_VERSION != 0

	/* If we have a greater internal height than public we set the internalHeight. This
	 * implies that cropping will be applied of internal dimensions to fit the public one.
	 *
	 * NOTE: This should really only be done when the producer is determined to be VPU decoder.
	 */
	*internalHeight += AFBC_PIXELS_PER_BLOCK;
#endif

	/* The actual height used in size calculation must include the possible extra row. But
	 * it must also be AFBC-aligned. Only the extra row-padding should be reported back in
	 * internalHeight. This as only this row needs to be considered when cropping. */

	if (type == UNCOMPRESSED)
	{
		AERR(" Buffer must be allocated with AFBC mode for internal pixel format YUV420_8BIT_AFBC!");
		return false;
	}
	else if (type == AFBC_TILED_HEADERS_BASIC)
	{
		width = GRALLOC_ALIGN(width, AFBC_TILED_HEADERS_BASIC_WIDTH_ALIGN);
		height = GRALLOC_ALIGN(*internalHeight, AFBC_TILED_HEADERS_BASIC_HEIGHT_ALIGN);
		buffer_byte_alignment = 4 * AFBC_BODY_BUFFER_BYTE_ALIGNMENT;
	}
	else if (type == AFBC_TILED_HEADERS_WIDEBLK)
	{
		width = GRALLOC_ALIGN(width, AFBC_TILED_HEADERS_WIDEBLK_WIDTH_ALIGN);
		height = GRALLOC_ALIGN(*internalHeight, AFBC_TILED_HEADERS_WIDEBLK_HEIGHT_ALIGN);
		buffer_byte_alignment = 4 * AFBC_BODY_BUFFER_BYTE_ALIGNMENT;
	}
	else if (type == AFBC_PADDED)
	{
		AERR("GRALLOC_USAGE_PRIVATE_2 (64byte header row alignment for AFBC) is not supported for YUV");
		return false;
	}
	else if (type == AFBC_WIDEBLK)
	{
		width = GRALLOC_ALIGN(width, AFBC_WIDEBLK_WIDTH_ALIGN);
		height = GRALLOC_ALIGN(*internalHeight, AFBC_WIDEBLK_HEIGHT_ALIGN);
	}
	else
	{
		width = GRALLOC_ALIGN(width, AFBC_NORMAL_WIDTH_ALIGN);
		height = GRALLOC_ALIGN(*internalHeight, AFBC_NORMAL_HEIGHT_ALIGN);
	}

	yuv420_afbc_luma_stride = width;
	yuv420_afbc_chroma_stride = GRALLOC_ALIGN(yuv420_afbc_luma_stride / 2, 16); /* Horizontal downsampling*/

	if (size != NULL)
	{
		int nblocks = width / AFBC_PIXELS_PER_BLOCK * height / AFBC_PIXELS_PER_BLOCK;
		/* Simplification of (height * luma-stride + 2 * (height /2 * chroma_stride) */
		*size = (yuv420_afbc_luma_stride + yuv420_afbc_chroma_stride) * height +
		        GRALLOC_ALIGN(nblocks * AFBC_HEADER_BUFFER_BYTES_PER_BLOCKENTRY, buffer_byte_alignment);
	}

	if (byte_stride != NULL)
	{
		*byte_stride = yuv420_afbc_luma_stride;
	}

	if (pixel_stride != NULL)
	{
		*pixel_stride = yuv420_afbc_luma_stride;
	}

	return true;
}

/*
 * Computes the strides and size for an YV12 buffer
 *
 * width                  Public known width of the buffer in pixels
 * height                 Public known height of the buffer in pixels
 *
 * pixel_stride     (out) stride of the buffer in pixels
 * byte_stride      (out) stride of the buffer in bytes
 * size             (out) size of the buffer in bytes
 * type             (in)  if buffer should be allocated for a certain afbc type
 * internalHeight   (out) The internal height, which may be greater than the public known height.
 * stride_alignment (in)  stride aligment value in bytes.
 */
static bool get_yv12_stride_and_size(int width, int height, int *pixel_stride, int *byte_stride, size_t *size,
                                     AllocType type, int *internalHeight, int stride_alignment)
{
	int luma_stride;

	if (type != UNCOMPRESSED)
	{
		return get_afbc_yuv420_8bit_stride_and_size(width, height, pixel_stride, byte_stride, size, type,
		                                            internalHeight);
	}

	/* 4:2:0 formats must have buffers with even height and width as the clump size is 2x2 pixels.
	 * Width will be even stride aligned anyway so just adjust height here for size calculation. */
	height = GRALLOC_ALIGN(height, 2);

	luma_stride = GRALLOC_ALIGN(width, stride_alignment);

	if (size != NULL)
	{
		int chroma_stride = GRALLOC_ALIGN(luma_stride / 2, stride_alignment);
		/* Simplification of ((height * luma_stride ) + 2 * ((height / 2) * chroma_stride)). */
		*size = height *(luma_stride + chroma_stride);
	}

	if (byte_stride != NULL)
	{
		*byte_stride = luma_stride;
	}

	if (pixel_stride != NULL)
	{
		*pixel_stride = luma_stride;
	}

	return true;
}

/*
 * Computes the strides and size for an 8 bit YUYV 422 buffer
 *
 * width                  Public known width of the buffer in pixels
 * height                 Public known height of the buffer in pixels
 *
 * pixel_stride     (out) stride of the buffer in pixels
 * byte_stride      (out) stride of the buffer in bytes
 * size             (out) size of the buffer in bytes
 */
static bool get_yuv422_8bit_stride_and_size(int width, int height, int *pixel_stride, int *byte_stride, size_t *size)
{
	int local_byte_stride, local_pixel_stride;

	/* 4:2:2 formats must have buffers with even width as the clump size is 2x1 pixels.
	 * This is taken care of by the even stride alignment. */

	local_pixel_stride = GRALLOC_ALIGN(width, YUV_MALI_PLANE_ALIGN);
	local_byte_stride = GRALLOC_ALIGN(width * 2, YUV_MALI_PLANE_ALIGN); /* 4 bytes per 2 pixels */

	if (size != NULL)
	{
		*size = local_byte_stride *height;
	}

	if (byte_stride != NULL)
	{
		*byte_stride = local_byte_stride;
	}

	if (pixel_stride != NULL)
	{
		*pixel_stride = local_pixel_stride;
	}

	return true;
}

/*
 * Computes the strides and size for an AFBC 8BIT YUV 4:2:2 buffer
 *
 * width               width of the buffer in pixels
 * height              height of the buffer in pixels
 *
 * pixel_stride (out)  stride of the buffer in pixels
 * byte_stride  (out)  stride of the buffer in bytes
 * size         (out)  size of the buffer in bytes
 * type                if buffer should be allocated for a certain afbc type
 */
static bool get_afbc_yuv422_8bit_stride_and_size(int width, int height, int *pixel_stride, int *byte_stride,
                                                 size_t *size, AllocType type)
{
	int yuv422_afbc_luma_stride;
	int buffer_byte_alignment = AFBC_BODY_BUFFER_BYTE_ALIGNMENT;

	if (type == UNCOMPRESSED)
	{
		AERR(" Buffer must be allocated with AFBC mode for internal pixel format YUV422_8BIT_AFBC!");
		return false;
	}
	else if (type == AFBC_TILED_HEADERS_BASIC)
	{
		width = GRALLOC_ALIGN(width, AFBC_TILED_HEADERS_BASIC_WIDTH_ALIGN);
		height = GRALLOC_ALIGN(height, AFBC_TILED_HEADERS_BASIC_HEIGHT_ALIGN);
		buffer_byte_alignment = 4 * AFBC_BODY_BUFFER_BYTE_ALIGNMENT;
	}
	else if (type == AFBC_TILED_HEADERS_WIDEBLK)
	{
		width = GRALLOC_ALIGN(width, AFBC_TILED_HEADERS_WIDEBLK_WIDTH_ALIGN);
		height = GRALLOC_ALIGN(height, AFBC_TILED_HEADERS_WIDEBLK_HEIGHT_ALIGN);
		buffer_byte_alignment = 4 * AFBC_BODY_BUFFER_BYTE_ALIGNMENT;
	}
	else if (type == AFBC_PADDED)
	{
		AERR("GRALLOC_USAGE_PRIVATE_2 (64byte header row alignment for AFBC) is not supported for YUV");
		return false;
	}
	else if (type == AFBC_WIDEBLK)
	{
		width = GRALLOC_ALIGN(width, AFBC_WIDEBLK_WIDTH_ALIGN);
		height = GRALLOC_ALIGN(height, AFBC_WIDEBLK_HEIGHT_ALIGN);
	}
	else
	{
		width = GRALLOC_ALIGN(width, AFBC_NORMAL_WIDTH_ALIGN);
		height = GRALLOC_ALIGN(height, AFBC_NORMAL_HEIGHT_ALIGN);
	}

	yuv422_afbc_luma_stride = width;

	if (size != NULL)
	{
		int nblocks = width / AFBC_PIXELS_PER_BLOCK * height / AFBC_PIXELS_PER_BLOCK;
		/* YUV 4:2:2 luma size equals chroma size */
		*size = yuv422_afbc_luma_stride *height * 2 +
		        GRALLOC_ALIGN(nblocks * AFBC_HEADER_BUFFER_BYTES_PER_BLOCKENTRY, buffer_byte_alignment);
	}

	if (byte_stride != NULL)
	{
		*byte_stride = yuv422_afbc_luma_stride;
	}

	if (pixel_stride != NULL)
	{
		*pixel_stride = yuv422_afbc_luma_stride;
	}

	return true;
}

/*
 * Calculate strides and sizes for a P010 (Y-UV 4:2:0) or P210 (Y-UV 4:2:2) buffer.
 *
 * @param width         [in]    Buffer width.
 * @param height        [in]    Buffer height.
 * @param vss           [in]    Vertical sub-sampling factor (2 for P010, 1 for
 *                              P210. Anything else is invalid).
 * @param pixel_stride  [out]   Pixel stride; number of pixels between
 *                              consecutive rows.
 * @param byte_stride   [out]   Byte stride; number of bytes between
 *                              consecutive rows.
 * @param size          [out]   Size of the buffer in bytes. Cumulative sum of
 *                              sizes of all planes.
 *
 * @return true if the calculation was successful; false otherwise (invalid
 * parameter)
 */
static bool get_yuv_pX10_stride_and_size(int width, int height, int vss, int *pixel_stride, int *byte_stride,
                                         size_t *size)
{
	int luma_pixel_stride, luma_byte_stride;

	if (vss < 1 || vss > 2)
	{
		AERR("Invalid vertical sub-sampling factor: %d, should be 1 or 2", vss);
		return false;
	}

	/* 4:2:2 must have even width as the clump size is 2x1 pixels. This will be taken care of by the
	 * even stride alignment */
	if (vss == 2)
	{
		/* 4:2:0 must also have even height as the clump size is 2x2 */
		height = GRALLOC_ALIGN(height, 2);
	}

	luma_pixel_stride = GRALLOC_ALIGN(width, YUV_MALI_PLANE_ALIGN);
	luma_byte_stride = GRALLOC_ALIGN(width * 2, YUV_MALI_PLANE_ALIGN);

	if (size != NULL)
	{
		int chroma_size = GRALLOC_ALIGN(width * 2, YUV_MALI_PLANE_ALIGN) * (height / vss);
		*size = luma_byte_stride *height + chroma_size;
	}

	if (byte_stride != NULL)
	{
		*byte_stride = luma_byte_stride;
	}

	if (pixel_stride != NULL)
	{
		*pixel_stride = luma_pixel_stride;
	}

	return true;
}

/*
 *  Calculate strides and strides for Y210 (10 bit YUYV packed, 4:2:2) format buffer.
 *
 * @param width         [in]    Buffer width.
 * @param height        [in]    Buffer height.
 * @param pixel_stride  [out]   Pixel stride; number of pixels between
 *                              consecutive rows.
 * @param byte_stride   [out]   Byte stride; number of bytes between
 *                              consecutive rows.
 * @param size          [out]   Size of the buffer in bytes. Cumulative sum of
 *                              sizes of all planes.
 *
 * @return true if the calculation was successful; false otherwise (invalid
 * parameter)
 */
static bool get_yuv_y210_stride_and_size(int width, int height, int *pixel_stride, int *byte_stride, size_t *size)
{
	int y210_byte_stride, y210_pixel_stride;

	/* 4:2:2 formats must have buffers with even width as the clump size is 2x1 pixels.
	 * This is taken care of by the even stride alignment */

	y210_pixel_stride = GRALLOC_ALIGN(width, YUV_MALI_PLANE_ALIGN);
	/* 4x16 bits per 2 pixels */
	y210_byte_stride = GRALLOC_ALIGN(width * 4, YUV_MALI_PLANE_ALIGN);

	if (size != NULL)
	{
		*size = y210_byte_stride *height;
	}

	if (byte_stride != NULL)
	{
		*byte_stride = y210_byte_stride;
	}

	if (pixel_stride != NULL)
	{
		*pixel_stride = y210_pixel_stride;
	}

	return true;
}

/*
 *  Calculate strides and strides for Y0L2 (YUYAAYVYAA, 4:2:0) format buffer.
 *
 * @param width         [in]    Buffer width.
 * @param height        [in]    Buffer height.
 * @param pixel_stride  [out]   Pixel stride; number of pixels between
 *                              consecutive rows.
 * @param byte_stride   [out]   Byte stride; number of bytes between
 *                              consecutive rows.
 * @param size          [out]   Size of the buffer in bytes. Cumulative sum of
 *                              sizes of all planes.
 *
 * @return true if the calculation was successful; false otherwise (invalid
 * parameter)
 *
 * @note Each YUYAAYVYAA clump encodes a 2x2 area of pixels. YU&V are 10 bits. A is 1 bit. total 8 bytes
 *
 */
static bool get_yuv_y0l2_stride_and_size(int width, int height, int *pixel_stride, int *byte_stride, size_t *size)
{
	int y0l2_byte_stride, y0l2_pixel_stride;

	/* 4:2:0 formats must have buffers with even height and width as the clump size is 2x2 pixels.
	 * Width is take care of by the even stride alignment so just adjust height here for size calculation. */
	height = GRALLOC_ALIGN(height, 2);

	y0l2_pixel_stride = GRALLOC_ALIGN(width, YUV_MALI_PLANE_ALIGN);
	y0l2_byte_stride = GRALLOC_ALIGN(width * 4, YUV_MALI_PLANE_ALIGN); /* 2 horiz pixels per 8 byte clump */

	if (size != NULL)
	{
		*size = y0l2_byte_stride *height / 2; /* byte stride covers 2 vert pixels */
	}

	if (byte_stride != NULL)
	{
		*byte_stride = y0l2_byte_stride;
	}

	if (pixel_stride != NULL)
	{
		*pixel_stride = y0l2_pixel_stride;
	}

	return true;
}

/*
 *  Calculate strides and strides for Y410 (AVYU packed, 4:4:4) format buffer.
 *
 * @param width         [in]    Buffer width.
 * @param height        [in]    Buffer height.
 * @param pixel_stride  [out]   Pixel stride; number of pixels between
 *                              consecutive rows.
 * @param byte_stride   [out]   Byte stride; number of bytes between
 *                              consecutive rows.
 * @param size          [out]   Size of the buffer in bytes. Cumulative sum of
 *                              sizes of all planes.
 *
 * @return true if the calculation was successful; false otherwise (invalid
 * parameter)
 */
static bool get_yuv_y410_stride_and_size(int width, int height, int *pixel_stride, int *byte_stride, size_t *size)
{
	int y410_byte_stride, y410_pixel_stride;

	y410_pixel_stride = GRALLOC_ALIGN(width, YUV_MALI_PLANE_ALIGN);
	y410_byte_stride = GRALLOC_ALIGN(width * 4, YUV_MALI_PLANE_ALIGN);

	if (size != NULL)
	{
		/* 4x8bits per pixel */
		*size = y410_byte_stride *height;
	}

	if (byte_stride != NULL)
	{
		*byte_stride = y410_byte_stride;
	}

	if (pixel_stride != NULL)
	{
		*pixel_stride = y410_pixel_stride;
	}

	return true;
}

/*
 *  Calculate strides and strides for YUV420_10BIT_AFBC (Compressed, 4:2:0) format buffer.
 *
 * @param width         [in]    Buffer width.
 * @param height        [in]    Buffer height.
 * @param pixel_stride  [out]   Pixel stride; number of pixels between
 *                              consecutive rows.
 * @param byte_stride   [out]   Byte stride; number of bytes between
 *                              consecutive rows.
 * @param size          [out]   Size of the buffer in bytes. Cumulative sum of
 *                              sizes of all planes.
 * @param type          [in]    afbc mode that buffer should be allocated with.
 *
 * @param internalHeight [out]  Internal buffer height that used by consumer or producer
 *
 * @return true if the calculation was successful; false otherwise (invalid
 * parameter)
 */
static bool get_yuv420_10bit_afbc_stride_and_size(int width, int height, int *pixel_stride, int *byte_stride,
                                                  size_t *size, AllocType type, int *internalHeight)
{
	int yuv420_afbc_byte_stride, yuv420_afbc_pixel_stride;
	int buffer_byte_alignment = AFBC_BODY_BUFFER_BYTE_ALIGNMENT;

	if (width & 3)
	{
		return false;
	}

	*internalHeight = height;
#if MALI_VIDEO_VERSION
	/* If we have a greater internal height than public we set the internalHeight. This
	 * implies that cropping will be applied of internal dimensions to fit the public one. */
	*internalHeight += AFBC_PIXELS_PER_BLOCK;
#endif

	/* The actual height used in size calculation must include the possible extra row. But
	 * it must also be AFBC-aligned. Only the extra row-padding should be reported back in
	 * internalHeight. This as only this row needs to be considered when cropping. */
	if (type == UNCOMPRESSED)
	{
		AERR(" Buffer must be allocated with AFBC mode for internal pixel format YUV420_10BIT_AFBC!");
		return false;
	}
	else if (type == AFBC_TILED_HEADERS_BASIC)
	{
		width = GRALLOC_ALIGN(width, AFBC_TILED_HEADERS_BASIC_WIDTH_ALIGN);
		height = GRALLOC_ALIGN(*internalHeight / 2, AFBC_TILED_HEADERS_BASIC_HEIGHT_ALIGN);
		buffer_byte_alignment = 4 * AFBC_BODY_BUFFER_BYTE_ALIGNMENT;
	}
	else if (type == AFBC_TILED_HEADERS_WIDEBLK)
	{
		width = GRALLOC_ALIGN(width, AFBC_TILED_HEADERS_WIDEBLK_WIDTH_ALIGN);
		height = GRALLOC_ALIGN(*internalHeight / 2, AFBC_TILED_HEADERS_WIDEBLK_HEIGHT_ALIGN);
		buffer_byte_alignment = 4 * AFBC_BODY_BUFFER_BYTE_ALIGNMENT;
	}
	else if (type == AFBC_PADDED)
	{
		AERR("GRALLOC_USAGE_PRIVATE_2 (64byte header row alignment for AFBC) is not supported for YUV");
		return false;
	}
	else if (type == AFBC_WIDEBLK)
	{
		width = GRALLOC_ALIGN(width, AFBC_WIDEBLK_WIDTH_ALIGN);
		height = GRALLOC_ALIGN(*internalHeight / 2, AFBC_WIDEBLK_HEIGHT_ALIGN);
	}
	else
	{
		width = GRALLOC_ALIGN(width, AFBC_NORMAL_WIDTH_ALIGN);
		height = GRALLOC_ALIGN(*internalHeight / 2, AFBC_NORMAL_HEIGHT_ALIGN);
	}

	yuv420_afbc_pixel_stride = GRALLOC_ALIGN(width, 16);
	yuv420_afbc_byte_stride = GRALLOC_ALIGN(width * 4, 16); /* 64-bit packed and horizontally downsampled */

	if (size != NULL)
	{
		int nblocks = width / AFBC_PIXELS_PER_BLOCK * (*internalHeight) / AFBC_PIXELS_PER_BLOCK;
		*size = yuv420_afbc_byte_stride *height +
		        GRALLOC_ALIGN(nblocks * AFBC_HEADER_BUFFER_BYTES_PER_BLOCKENTRY, buffer_byte_alignment);
	}

	if (byte_stride != NULL)
	{
		*byte_stride = yuv420_afbc_pixel_stride;
	}

	if (pixel_stride != NULL)
	{
		*pixel_stride = yuv420_afbc_pixel_stride;
	}

	return true;
}

/*
 *  Calculate strides and strides for YUV422_10BIT_AFBC (Compressed, 4:2:2) format buffer.
 *
 * @param width         [in]    Buffer width.
 * @param height        [in]    Buffer height.
 * @param pixel_stride  [out]   Pixel stride; number of pixels between
 *                              consecutive rows.
 * @param byte_stride   [out]   Byte stride; number of bytes between
 *                              consecutive rows.
 * @param size          [out]   Size of the buffer in bytes. Cumulative sum of
 *                              sizes of all planes.
 * @param type          [in]    afbc mode that buffer should be allocated with.
 *
 * @return true if the calculation was successful; false otherwise (invalid
 * parameter)
 */
static bool get_yuv422_10bit_afbc_stride_and_size(int width, int height, int *pixel_stride, int *byte_stride,
                                                  size_t *size, AllocType type)
{
	int yuv422_afbc_byte_stride, yuv422_afbc_pixel_stride;
	int buffer_byte_alignment = AFBC_BODY_BUFFER_BYTE_ALIGNMENT;

	if (width & 3)
	{
		return false;
	}

	if (type == UNCOMPRESSED)
	{
		AERR(" Buffer must be allocated with AFBC mode for internal pixel format YUV422_10BIT_AFBC!");
		return false;
	}
	else if (type == AFBC_TILED_HEADERS_BASIC)
	{
		width = GRALLOC_ALIGN(width, AFBC_TILED_HEADERS_BASIC_WIDTH_ALIGN);
		height = GRALLOC_ALIGN(height, AFBC_TILED_HEADERS_BASIC_HEIGHT_ALIGN);
		buffer_byte_alignment = 4 * AFBC_BODY_BUFFER_BYTE_ALIGNMENT;
	}
	else if (type == AFBC_TILED_HEADERS_WIDEBLK)
	{
		width = GRALLOC_ALIGN(width, AFBC_TILED_HEADERS_WIDEBLK_WIDTH_ALIGN);
		height = GRALLOC_ALIGN(height, AFBC_TILED_HEADERS_WIDEBLK_HEIGHT_ALIGN);
		buffer_byte_alignment = 4 * AFBC_BODY_BUFFER_BYTE_ALIGNMENT;
	}
	else if (type == AFBC_PADDED)
	{
		AERR("GRALLOC_USAGE_PRIVATE_2 (64byte header row alignment for AFBC) is not supported for YUV");
		return false;
	}
	else if (type == AFBC_WIDEBLK)
	{
		width = GRALLOC_ALIGN(width, AFBC_WIDEBLK_WIDTH_ALIGN);
		height = GRALLOC_ALIGN(height, AFBC_WIDEBLK_HEIGHT_ALIGN);
	}
	else
	{
		width = GRALLOC_ALIGN(width, AFBC_NORMAL_WIDTH_ALIGN);
		height = GRALLOC_ALIGN(height, AFBC_NORMAL_HEIGHT_ALIGN);
	}

	yuv422_afbc_pixel_stride = GRALLOC_ALIGN(width, 16);
	yuv422_afbc_byte_stride = GRALLOC_ALIGN(width * 2, 16);

	if (size != NULL)
	{
		int nblocks = width / AFBC_PIXELS_PER_BLOCK * height / AFBC_PIXELS_PER_BLOCK;
		/* YUV 4:2:2 chroma size equals to luma size */
		*size = yuv422_afbc_byte_stride *height * 2 +
		        GRALLOC_ALIGN(nblocks * AFBC_HEADER_BUFFER_BYTES_PER_BLOCKENTRY, buffer_byte_alignment);
	}

	if (byte_stride != NULL)
	{
		*byte_stride = yuv422_afbc_byte_stride;
	}

	if (pixel_stride != NULL)
	{
		*pixel_stride = yuv422_afbc_pixel_stride;
	}

	return true;
}

/*
 *  Calculate strides and strides for Camera RAW and Blob formats
 *
 * @param w             [in]    Buffer width.
 * @param h             [in]    Buffer height.
 * @param format        [in]    Requested HAL format
 * @param out_stride    [out]   Pixel stride; number of pixels/bytes between
 *                              consecutive rows. Format description calls for
 *                              either bytes or pixels.
 * @param size          [out]   Size of the buffer in bytes. Cumulative sum of
 *                              sizes of all planes.
 *
 * @return true if the calculation was successful; false otherwise (invalid
 * parameter)
 */
static bool get_camera_formats_stride_and_size(int w, int h, uint64_t format, int *out_stride, size_t *out_size)
{
	int stride, size;

	switch (format)
	{
	case HAL_PIXEL_FORMAT_RAW16:
		stride = w; /* Format assumes stride in pixels */
		stride = GRALLOC_ALIGN(stride, 16); /* Alignment mandated by Android */
		size = stride * h * 2; /* 2 bytes per pixel */
		break;

	case HAL_PIXEL_FORMAT_RAW12:
		if (w % 4 != 0)
		{
			ALOGE("ERROR: Width for HAL_PIXEL_FORMAT_RAW12 buffers has to be multiple of 4.");
			return false;
		}

		stride = (w / 2) * 3; /* Stride in bytes; 2 pixels in 3 bytes */
		size = stride * h;
		break;

	case HAL_PIXEL_FORMAT_RAW10:
		if (w % 4 != 0)
		{
			ALOGE("ERROR: Width for HAL_PIXEL_FORMAT_RAW10 buffers has to be multiple of 4.");
			return false;
		}

		stride = (w / 4) * 5; /* Stride in bytes; 4 pixels in 5 bytes */
		size = stride * h;
		break;

	case HAL_PIXEL_FORMAT_BLOB:
		if (h != 1)
		{
			ALOGE("ERROR: Height for HAL_PIXEL_FORMAT_BLOB must be 1.");
			return false;
		}

		stride = 0; /* No 'rows', it's effectively a long one dimensional array */
		size = w;
		break;

	default:
		return false;
	}

	if (out_size != NULL)
	{
		*out_size = size;
	}

	if (out_stride != NULL)
	{
		*out_stride = stride;
	}

	return true;
}


static bool get_rk_nv12_stride_and_size(int width, int height, int* pixel_stride, int* byte_stride, size_t* size)
{
    /**
     * .KP : from CSY : video_decoder Ҫ��� byte_stride of buffer in NV12, �Ѿ�ͨ�� width ����.
     * �� NV12, byte_stride ���� pixel_stride, Ҳ���� luma_stride.
     */
    int luma_stride = width;

    if (width % 2 != 0 || height % 2 != 0)
    {
        return false;
    }

    if (size != NULL)
    {
        /* .KP : from CSY : video_decoder ��Ҫ�� buffer �г��� YUV ���ݻ������� metadata, Ҫ����Ŀռ�. 2 * w * h һ����. */
        *size = 2 * luma_stride * height;
    }

    if (byte_stride != NULL)
    {
        *byte_stride = luma_stride;
    }

    if (pixel_stride != NULL)
    {
        *pixel_stride = luma_stride;
    }

    return true;
}

static bool get_rk_nv12_10bit_stride_and_size (int width, int height, int* pixel_stride, int* byte_stride, size_t* size)
{

    if (width % 2 != 0 || height % 2 != 0)
    {
        return false;
    }

    /**
     * .KP : from CSY : video_decoder Ҫ��� byte_stride of buffer in NV12_10, �Ѿ�ͨ�� width ����.
     * �� NV12_10, ԭ����, byte_stride �� pixel_stride ��ͬ.
     */
    *byte_stride = width;

    /* .KP : from CSY : video_decoder ��Ҫ�� buffer �г��� YUV ���ݻ������� metadata, Ҫ����Ŀռ�. 2 * w * h һ����. */
    *size = 2 * width * height;

    *pixel_stride = *byte_stride;
    // ������, ���Ǵ����,
    // ����Ŀǰ���� NV12_10, rk_hwc, �� private_module_t::stride ��Ϊ byte_stride ʹ��.

    return true;
}

static void init_afbc(uint8_t *buf, uint64_t internal_format, int w, int h)
{
	uint32_t n_headers = (w * h) / 64;
	uint32_t body_offset = n_headers * 16;
	uint32_t headers[][4] = {
		{ body_offset, 0x1, 0x0, 0x0 }, /* Layouts 0, 3, 4 */
		{ (body_offset + (1 << 28)), 0x200040, 0x4000, 0x80 } /* Layouts 1, 5 */
	};
	uint32_t i, layout;

	/* map format if necessary (also removes internal extension bits) */
	uint64_t base_format = internal_format & MALI_GRALLOC_INTFMT_FMT_MASK;

	switch (base_format)
	{
	case MALI_GRALLOC_FORMAT_INTERNAL_RGBA_8888:
	case MALI_GRALLOC_FORMAT_INTERNAL_RGBX_8888:
	case MALI_GRALLOC_FORMAT_INTERNAL_RGB_888:
	case MALI_GRALLOC_FORMAT_INTERNAL_RGB_565:
	case MALI_GRALLOC_FORMAT_INTERNAL_BGRA_8888:
		layout = 0;
		break;

	case MALI_GRALLOC_FORMAT_INTERNAL_YV12:
	case MALI_GRALLOC_FORMAT_INTERNAL_NV12:
	case MALI_GRALLOC_FORMAT_INTERNAL_NV21:
		layout = 1;
		break;

	default:
		layout = 0;
	}

	ALOGV("Writing AFBC header layout %d for format %" PRIu64, layout, base_format);

	for (i = 0; i < n_headers; i++)
	{
		memcpy(buf, headers[layout], sizeof(headers[layout]));
		buf += sizeof(headers[layout]);
	}
}

#endif

static void drm_gem_rockchip_destroy(struct gralloc_drm_drv_t *drv)
{
	struct rockchip_info *info = (struct rockchip_info *)drv;

	if (info->rockchip)
		rockchip_device_destroy(info->rockchip);
	free(info);
}

static struct gralloc_drm_bo_t *drm_gem_rockchip_alloc(
		struct gralloc_drm_drv_t *drv,
		struct gralloc_drm_handle_t *handle)
{
	struct rockchip_info *info = (struct rockchip_info *)drv;
	struct rockchip_buffer *buf;
	struct drm_gem_close args;
#if  !RK_DRM_GRALLOC
        int ret, cpp, pitch, aligned_width, aligned_height;
        uint32_t size, gem_handle;
#else
	int ret;
	size_t size;
	uint32_t gem_handle;
	AllocType alloc_type = UNCOMPRESSED;
	bool alloc_for_extended_yuv = false, alloc_for_arm_afbc_yuv = false;
	int internalWidth,internalHeight;
	uint64_t internal_format;
        int byte_stride;   // Stride of the buffer in bytes
        int pixel_stride;  // Stride of the buffer in pixels - as returned in pStride
        int w = handle->width,h = handle->height;
        int format = handle->format;
        int usage = handle->usage;
        int err;
        bool fmt_chg = false;
        int fmt_bak = format;
        void *addr = NULL;
// #if USE_AFBC_LAYER
#if 0
	char framebuffer_size[PROPERTY_VALUE_MAX];
	uint32_t width, height, vrefresh;
#endif

        AINF("enter, w : %d, h : %d, format : 0x%x, usage : 0x%x.", w, h, format, usage);

        if(format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED  )
        {
                if(usage & GRALLOC_USAGE_HW_VIDEO_ENCODER )
                {
                        ADBG("(usage & GRALLOC_USAGE_HW_VIDEO_ENCODER treat as NV12");
                        format = HAL_PIXEL_FORMAT_YCrCb_NV12;
                }
                else
                {
                        ADBG("treat as NV12 888");
                        format = HAL_PIXEL_FORMAT_RGBX_8888;
                        fmt_chg = true;
                }
        }

	/* Some formats require an internal width and height that may be used by
	 * consumers/producers.
	 */
	internalWidth = w;
	internalHeight = h;

    internal_format = mali_gralloc_select_format(format,
                                                 MALI_GRALLOC_FORMAT_TYPE_USAGE,
                                                 usage,
                                                 w * h);

// #if USE_AFBC_LAYER
#if 0
	property_get("persist.sys.framebuffer.main", framebuffer_size, "0x0@60");
	sscanf(framebuffer_size, "%dx%d@%d", &width, &height, &vrefresh);
	//Vop cann't support 4K AFBC layer.
	if (height < 2160)
	{
#define MAGIC_USAGE_FOR_AFBC_LAYER     (0x88)
	    if (!(usage & GRALLOC_USAGE_HW_FB)) {
	            if (!(usage & GRALLOC_USAGE_EXTERNAL_DISP) &&
	                MAGIC_USAGE_FOR_AFBC_LAYER == (usage & MAGIC_USAGE_FOR_AFBC_LAYER) ) {
	                internal_format = GRALLOC_ARM_INTFMT_AFBC | GRALLOC_ARM_HAL_FORMAT_INDEXED_RGBA_8888; // .T : �޸�.
	                AWAR("use_afbc_layer: force to set 'internal_format' to 0x%llx for usage '0x%x'.", internal_format, usage);
	            }
	    } else {
	        if(!(usage & GRALLOC_USAGE_EXTERNAL_DISP) &&
	           MAGIC_USAGE_FOR_AFBC_LAYER != (usage & MAGIC_USAGE_FOR_AFBC_LAYER)) {
	                internal_format = GRALLOC_ARM_INTFMT_AFBC | GRALLOC_ARM_HAL_FORMAT_INDEXED_RGBA_8888;
	                AWAR("use_afbc_layer: force to set 'internal_format' to 0x%llx for buffer_for_fb_target_layer.",
	                internal_format);
	        }
	    }
	}
#endif

    /* Determine AFBC type for this format */
    if (internal_format & MALI_GRALLOC_INTFMT_AFBCENABLE_MASK)
    {
        if (internal_format & MALI_GRALLOC_INTFMT_AFBC_TILED_HEADERS)
        {
            if (internal_format & MALI_GRALLOC_INTFMT_AFBC_WIDEBLK)
            {
                alloc_type = AFBC_TILED_HEADERS_WIDEBLK;
            }
            else if (internal_format & MALI_GRALLOC_INTFMT_AFBC_BASIC)
            {
                alloc_type = AFBC_TILED_HEADERS_BASIC;
            }
            else if (internal_format & MALI_GRALLOC_INTFMT_AFBC_SPLITBLK)
            {
                ALOGE("Unsupported format. Splitblk in tiled header configuration.");
                return NULL;
            }
        }
        else if (usage & MALI_GRALLOC_USAGE_AFBC_PADDING)
        {
            alloc_type = AFBC_PADDED;
        }
        else if (internal_format & MALI_GRALLOC_INTFMT_AFBC_WIDEBLK)
        {
            alloc_type = AFBC_WIDEBLK;
        }
        else
        {
            alloc_type = AFBC;
        }
    }
    
    uint64_t base_format = internal_format & MALI_GRALLOC_INTFMT_FMT_MASK;

    switch (base_format)
    {
        case HAL_PIXEL_FORMAT_RGBA_8888:
        case HAL_PIXEL_FORMAT_RGBX_8888:
        case HAL_PIXEL_FORMAT_BGRA_8888:
            get_rgb_stride_and_size(w, h, 4, &pixel_stride,
                    &byte_stride, &size, alloc_type);
            break;

        case HAL_PIXEL_FORMAT_RGB_888:
            get_rgb_stride_and_size(w, h, 3, &pixel_stride,
                    &byte_stride, &size, alloc_type);
            break;

        case HAL_PIXEL_FORMAT_RGB_565:
            get_rgb_stride_and_size(w, h, 2, &pixel_stride,
                    &byte_stride, &size, alloc_type);
            break;

        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        case MALI_GRALLOC_FORMAT_INTERNAL_YV12:
        case MALI_GRALLOC_FORMAT_INTERNAL_NV12:
        case MALI_GRALLOC_FORMAT_INTERNAL_NV21:
            {
                /* Mali subsystem prefers higher stride alignment values (128 bytes) for YUV, but software components assume
                 * default of 16. We only need to care about YV12 as it's the only, implicit, HAL YUV format in Android.
                 */
                int yv12_align = YUV_MALI_PLANE_ALIGN;

                if (usage & (GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK))
                {
                    yv12_align = YUV_ANDROID_PLANE_ALIGN;
                }

                if (!get_yv12_stride_and_size(w, h, &pixel_stride,
                            &byte_stride, &size, alloc_type,
                            &internalHeight, yv12_align))
                {
                    return NULL;
                }

                break;
            }

        case HAL_PIXEL_FORMAT_YCbCr_422_I:
            {
                /* YUYV 4:2:2 */
                if (alloc_type != UNCOMPRESSED ||
                        !get_yuv422_8bit_stride_and_size(w, h,
                            &pixel_stride, &byte_stride,
                            &size))
                {
                    return NULL;
                }

                break;
            }

        case HAL_PIXEL_FORMAT_RAW16:
        case HAL_PIXEL_FORMAT_RAW12:
        case HAL_PIXEL_FORMAT_RAW10:
        case HAL_PIXEL_FORMAT_BLOB:
            if (alloc_type != UNCOMPRESSED)
            {
                return NULL;
            }

            get_camera_formats_stride_and_size(w, h, base_format,
                    &pixel_stride, &size);
            /* For Raw/Blob formats stride is defined to be either in bytes or pixels per format */
            byte_stride = pixel_stride;
            break;

        case MALI_GRALLOC_FORMAT_INTERNAL_Y0L2:

            /* YUYAAYUVAA 4:2:0 with and without AFBC */
            if (alloc_type != UNCOMPRESSED)
            {
                if (!get_yuv420_10bit_afbc_stride_and_size(
                            w, h, &pixel_stride,
                            &byte_stride, &size, alloc_type, &internalHeight))
                {
                    return NULL;
                }
            }
            else
            {
                if (!get_yuv_y0l2_stride_and_size(w, h,
                            &pixel_stride, &byte_stride,
                            &size))
                {
                    return NULL;
                }
            }

            break;

        case MALI_GRALLOC_FORMAT_INTERNAL_P010:

            /* Y-UV 4:2:0 */
            if (alloc_type != UNCOMPRESSED ||
                    !get_yuv_pX10_stride_and_size(w, h, 2,
                        &pixel_stride, &byte_stride,
                        &size))
            {
                return NULL;
            }

            break;

        case MALI_GRALLOC_FORMAT_INTERNAL_P210:

            /* Y-UV 4:2:2 */
            if (alloc_type != UNCOMPRESSED ||
                    !get_yuv_pX10_stride_and_size(w, h, 1,
                        &pixel_stride, &byte_stride,
                        &size))
            {
                return NULL;
            }

            break;

        case MALI_GRALLOC_FORMAT_INTERNAL_Y210:

            /* YUYV 4:2:2 with and without AFBC */
            if (alloc_type != UNCOMPRESSED)
            {
                if (!get_yuv422_10bit_afbc_stride_and_size(w, h,
                            &pixel_stride, &byte_stride,
                            &size, alloc_type))
                {
                    return NULL;
                }
            }
            else
            {
                if (!get_yuv_y210_stride_and_size(w, h,
                            &pixel_stride, &byte_stride,
                            &size))
                {
                    return NULL;
                }
            }

            break;

        case MALI_GRALLOC_FORMAT_INTERNAL_Y410:

            /* AVYU 2-10-10-10 */
            if (alloc_type != UNCOMPRESSED ||
                    !get_yuv_y410_stride_and_size(w, h, &pixel_stride,
                        &byte_stride, &size))
            {
                return NULL;
            }

            break;

        case MALI_GRALLOC_FORMAT_INTERNAL_YUV422_8BIT:

            /* 8BIT AFBC YUV4:2:2 testing usage */

            /* We only support compressed for this format right now.
             * Below will fail in case format is uncompressed.
             */
            if (!get_afbc_yuv422_8bit_stride_and_size(w, h,
                        &pixel_stride, &byte_stride,
                        &size, alloc_type))
            {
                return NULL;
            }

            break;

            /*
             * Additional custom formats can be added here
             * and must fill the variables pixel_stride, byte_stride and size.
             */
        case HAL_PIXEL_FORMAT_YCrCb_NV12:
            if (!get_rk_nv12_stride_and_size(w, h, &pixel_stride, &byte_stride, &size))
            {
                AERR("err.");
                return NULL;
            }
            AINF("for nv12, w : %d, h : %d, pixel_stride : %d, byte_stride : %d, size : %zu; internalHeight : %d.",
                    w,
                    h,
                    pixel_stride,
                    byte_stride,
                    size,
                    internalHeight);
            break;

        case HAL_PIXEL_FORMAT_YCrCb_NV12_10:
            if (!get_rk_nv12_10bit_stride_and_size(w, h, &pixel_stride, &byte_stride, &size))
            {
                AERR("err.");
                return NULL;
            }

            AINF("for nv12_10, w : %d, h : %d, pixel_stride : %d, byte_stride : %d, size : %zu; internalHeight : %d.",
                    w,
                    h,
                    pixel_stride,
                    byte_stride,
                    size,
                    internalHeight);
            break;

        default:
            E("unexpected 'base_format' : 0x%" PRIu64, base_format);
            return NULL;
    }

#if (1 == MALI_ARCHITECTURE_UTGARD)
	/* match the framebuffer format */
	if (usage & GRALLOC_USAGE_HW_FB)
	{
#ifdef GRALLOC_16_BITS
		format = HAL_PIXEL_FORMAT_RGB_565;
#else
		format = HAL_PIXEL_FORMAT_RGBA_8888;
#endif //end of GRALLOC_16_BITS
	}
#endif //end of MALI_ARCHITECTURE_UTGARD
#endif //end of RK_DRM_GRALLOC

	buf = (struct rockchip_buffer*)calloc(1, sizeof(*buf));
	if (!buf) {
#if RK_DRM_GRALLOC
		AERR("Failed to allocate buffer wrapper\n");
#else
                ALOGE("Failed to allocate buffer wrapper\n");
#endif
		return NULL;
	}

#if !RK_DRM_GRALLOC
        cpp = gralloc_drm_get_bpp(handle->format);
        if (!cpp) {
                ALOGE("unrecognized format 0x%x", handle->format);
                return NULL;
        }

	aligned_width = handle->width;
	aligned_height = handle->height;
	gralloc_drm_align_geometry(handle->format,
			&aligned_width, &aligned_height);

	/* TODO: We need to sort out alignment */
	pitch = ALIGN(aligned_width * cpp, 64);
	size = aligned_height * pitch;

	if (handle->format == HAL_PIXEL_FORMAT_YCbCr_420_888) {
		/*
		 * WAR for H264 decoder requiring additional space
		 * at the end of destination buffers.
		 */
		uint32_t w_mbs, h_mbs;

		w_mbs = ALIGN(handle->width, 16) / 16;
		h_mbs = ALIGN(handle->height, 16) / 16;
		size += 64 * w_mbs * h_mbs;
	}
#endif

	if (handle->prime_fd >= 0) {
		ret = drmPrimeFDToHandle(info->fd, handle->prime_fd,
			&gem_handle);
		if (ret) {
			char *c = NULL;
			ALOGE("failed to convert prime fd to handle %d ret=%d",
				handle->prime_fd, ret);
			*c = 0;
			goto err;
		}

#if RK_DRM_GRALLOC
		AINF("Got handle %d for fd %d\n", gem_handle, handle->prime_fd);
#else
                ALOGV("Got handle %d for fd %d\n", gem_handle, handle->prime_fd);
#endif

		buf->bo = rockchip_bo_from_handle(info->rockchip, gem_handle,
			0, size);
		if (!buf->bo) {
#if RK_DRM_GRALLOC
			AERR("failed to wrap bo handle=%d size=%zd\n",
				gem_handle, size);
#else
                        ALOGE("failed to wrap bo handle=%d size=%d\n",
				gem_handle, size);
#endif
			memset(&args, 0, sizeof(args));
			args.handle = gem_handle;
			drmIoctl(info->fd, DRM_IOCTL_GEM_CLOSE, &args);
			return NULL;
		}
	} 
    else // if (handle->prime_fd >= 0)
    {
		uint32_t flags=0;

        if ( has_usage_flags(usage, GRALLOC_USAGE_SW_WRITE_OFTEN)
            || has_usage_flags(usage, GRALLOC_USAGE_SW_READ_OFTEN) )
        {
            D("to ask for cachable buffer for CPU access, usage : 0x%x", usage);
            flags = ROCKCHIP_BO_CACHABLE;
        }

		if(format == HAL_PIXEL_FORMAT_YCrCb_NV12_10)
		{
			//set cache flag
			flags = ROCKCHIP_BO_CACHABLE;
		}

		buf->bo = rockchip_bo_create(info->rockchip, size, flags);
		if (!buf->bo) {
#if RK_DRM_GRALLOC
			AERR("failed to allocate bo %dx%dx%dx%zd\n",
				handle->height, pixel_stride,byte_stride, size);
#else
                        ALOGE("failed to allocate bo %dx%dx%dx%d\n",
				handle->height, pitch, cpp, size);
#endif
			goto err;
		}

		gem_handle = rockchip_bo_handle(buf->bo);
		ret = drmPrimeHandleToFD(info->fd, gem_handle, 0,
			&handle->prime_fd);
#if RK_DRM_GRALLOC
                AINF("Got fd %d for handle %d\n", handle->prime_fd, gem_handle);
#else
		ALOGV("Got fd %d for handle %d\n", handle->prime_fd, gem_handle);
#endif
		if (ret) {
#if RK_DRM_GRALLOC
			AERR("failed to get prime fd %d", ret);
#else
                        ALOGE("failed to get prime fd %d", ret);
#endif
			goto err_unref;
		}

		buf->base.fb_handle = gem_handle;
	}

#if GRALLOC_INIT_AFBC == 1
        if (!(usage & GRALLOC_USAGE_PROTECTED))
        {
                addr = rockchip_bo_map(buf->bo);
                if (!addr) {
                        AERR("failed to map bo\n");
                        goto err_unref;
                }

                    
                if ( internal_format & MALI_GRALLOC_INTFMT_AFBCENABLE_MASK )
                {
                        init_afbc((uint8_t*)addr, internal_format, w, h);
                }
        }
#endif /* GRALLOC_INIT_AFBC == 1 */

#if RK_DRM_GRALLOC
#if MALI_AFBC_GRALLOC == 1
        /*
         * If handle has been dup,then the fd is a negative number.
         * Either you should close it or don't allocate the fd agagin.
         * Otherwize,it will leak fd.
         */
        if(handle->share_attr_fd < 0)
        {
                err = gralloc_buffer_attr_allocate( handle );
                //ALOGD("err=%d,isfb=%x,[%d,%x]",err,usage & GRALLOC_USAGE_HW_FB,hnd->share_attr_fd,hnd->attr_base);
                if( err < 0 )
                {
                        if ( (usage & GRALLOC_USAGE_HW_FB) )
                        {
                                /*
                                 * Having the attribute region is not critical for the framebuffer so let it pass.
                                 */
                                err = 0;
                        }
                        else
                        {
                                drm_gem_rockchip_free( drv, &buf->base );
                                goto err_unref;
                        }
                }
	}
#endif

#if 0
	int private_usage = usage & (GRALLOC_USAGE_PRIVATE_0 |
	                             GRALLOC_USAGE_PRIVATE_1);
	switch (private_usage)
	{
		case 0:
			if(usage & ARM_P010)
				handle->yuv_info = MALI_YUV_BT709_WIDE;//MALI_YUV_BT601_NARROW;
			else
				handle->yuv_info = MALI_YUV_BT601_NARROW;
			break;
		case GRALLOC_USAGE_PRIVATE_1:
			handle->yuv_info = MALI_YUV_BT601_WIDE;
			break;
		case GRALLOC_USAGE_PRIVATE_0:
			handle->yuv_info = MALI_YUV_BT709_NARROW;
			break;
		case (GRALLOC_USAGE_PRIVATE_0 | GRALLOC_USAGE_PRIVATE_1):
			handle->yuv_info = MALI_YUV_BT709_WIDE;
			break;
	}
#endif
    
    switch (usage & MALI_GRALLOC_USAGE_YUV_CONF_MASK)
    {
        case MALI_GRALLOC_USAGE_YUV_CONF_0:
			if ( usage & ARM_P010 )
            {
				handle->yuv_info = MALI_YUV_BT709_WIDE; // for rk_hdr.
            }
			else
            {
				handle->yuv_info = MALI_YUV_BT601_NARROW;
            }
			break;

        case MALI_GRALLOC_USAGE_YUV_CONF_1:
            handle->yuv_info = MALI_YUV_BT601_WIDE;
            break;

        case MALI_GRALLOC_USAGE_YUV_CONF_2:
            handle->yuv_info = MALI_YUV_BT709_NARROW;
            break;

        case MALI_GRALLOC_USAGE_YUV_CONF_3:
            handle->yuv_info = MALI_YUV_BT709_WIDE;
            break;
    }
    
    /*-------------------------------------------------------*/

        handle->stride = byte_stride;//pixel_stride;
        handle->pixel_stride = pixel_stride;
        handle->byte_stride = byte_stride;
        handle->format = fmt_chg ? fmt_bak : format;
        handle->size = size;
        handle->offset = 0;
        handle->internalWidth = internalWidth;
        handle->internalHeight = internalHeight;
        handle->internal_format = internal_format;
#else
        handle->stride = pitch;
#endif
        handle->name = 0;
	buf->base.handle = handle;

        AINF("leave, w : %d, h : %d, format : 0x%x,internal_format : 0x%" PRIu64 ", usage : 0x%x. size=%d,pixel_stride=%d,byte_stride=%d",
                handle->width, handle->height, handle->format,internal_format, handle->usage, handle->size,
                pixel_stride,byte_stride);
        AINF("leave: prime_fd=%d,share_attr_fd=%d",handle->prime_fd,handle->share_attr_fd);
	return &buf->base;

err_unref:
	rockchip_bo_destroy(buf->bo);
err:
	free(buf);
	return NULL;
}

static void drm_gem_rockchip_free(struct gralloc_drm_drv_t *drv,
		struct gralloc_drm_bo_t *bo)
{
	struct rockchip_buffer *buf = (struct rockchip_buffer *)bo;
        struct gralloc_drm_handle_t *gr_handle = gralloc_drm_handle((buffer_handle_t)bo->handle);

	UNUSED(drv);

        if (!gr_handle)
                return;

#if RK_DRM_GRALLOC
#if MALI_AFBC_GRALLOC == 1
	gralloc_buffer_attr_free( gr_handle );
#endif

	if (gr_handle->prime_fd)
		close(gr_handle->prime_fd);

	gr_handle->prime_fd = -1;
#endif
        gralloc_drm_unlock_handle((buffer_handle_t)bo->handle);

	/* TODO: Is destroy correct here? */
	rockchip_bo_destroy(buf->bo);
	free(buf);
}

static int drm_gem_rockchip_map(struct gralloc_drm_drv_t *drv,
		struct gralloc_drm_bo_t *bo, int x, int y, int w, int h,
		int enable_write, void **addr)
{
	struct rockchip_buffer *buf = (struct rockchip_buffer *)bo;

	UNUSED(drv, x, y, w, h, enable_write);

	*addr = rockchip_bo_map(buf->bo);
	if (!*addr) {
		ALOGE("failed to map bo\n");
		return -1;
	}

	return 0;
}

static void drm_gem_rockchip_unmap(struct gralloc_drm_drv_t *drv,
		struct gralloc_drm_bo_t *bo)
{
	UNUSED(drv, bo);
}

#if RK_DRM_GRALLOC
static int drm_init_version()
{
        char value[PROPERTY_VALUE_MAX];

        property_get("sys.ggralloc.version", value, "NULL");
        if(!strcmp(value,"NULL"))
        {
                property_set("sys.ggralloc.version", RK_GRALLOC_VERSION);
                ALOGD(RK_GRAPHICS_VER);
                ALOGD("gralloc ver '%s' on arm_release_ver '%s'.",
                        RK_GRALLOC_VERSION,
                        ARM_RELEASE_VER);
        }

        return 0;
}
#endif

struct gralloc_drm_drv_t *gralloc_drm_drv_create_for_rockchip(int fd)
{
	struct rockchip_info *info;
	int ret;

#if RK_DRM_GRALLOC
        drm_init_version();
#endif

	info = (struct rockchip_info*)calloc(1, sizeof(*info));
	if (!info) {
		ALOGE("Failed to allocate rockchip gralloc device\n");
		return NULL;
	}

	info->rockchip = rockchip_device_create(fd);
	if (!info->rockchip) {
		ALOGE("Failed to create new rockchip instance\n");
		free(info);
		return NULL;
	}

	info->fd = fd;
	info->base.destroy = drm_gem_rockchip_destroy;
	info->base.alloc = drm_gem_rockchip_alloc;
	info->base.free = drm_gem_rockchip_free;
	info->base.map = drm_gem_rockchip_map;
	info->base.unmap = drm_gem_rockchip_unmap;

	return &info->base;
}