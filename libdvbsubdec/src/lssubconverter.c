/*-----------------------------------------------------------------------------
 * lssubconverter.c
 *
 * Copyright (c) Larixsoft Inc
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *---------------------------------------------------------------------------*/

#include "lssubconverter.h"
#include "lsmemory.h"
/* Endianness detection for ARGB32/BGRA32 conversion */
#if !defined (__BYTE_ORDER__)
#if defined (__linux__) || \
  defined (__GNU__) || \
  defined (__GLIBC__)
#include <endian.h>
#elif defined (__APPLE__)
#include <machine/endian.h>
#elif defined (__FreeBSD__) || \
  defined (__NetBSD__) || \
  defined (__OpenBSD__)
#include <sys/endian.h>
#elif defined (_WIN32)
/* Windows is little-endian */
#define __BYTE_ORDER__    __ORDER_LITTLE_ENDIAN__
#endif
#endif
/* Determine if we need byte swap for ARGB32 format */
/* ARGB32 means 0xAARRGGBB in register
 * - Little-endian: memory layout [B,G,R,A]
 * - Big-endian: memory layout [A,R,G,B]
 *
 * LS_ColorRGB_t is [A,R,G,B] in memory, so:
 * - Little-endian: need byte swap
 * - Big-endian: no swap needed
 */
#if defined (__BYTE_ORDER__)
#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define ARGB32_NEEDS_BYTESWAP    1
#else
#define ARGB32_NEEDS_BYTESWAP    0
#endif
#else
/* Default to little-endian (most common) */
#define ARGB32_NEEDS_BYTESWAP    1
#warning "Byte order not detected, assuming little-endian"
#endif
/*ITU R601 version*/
static int32_t __rbg2ycbcrMatrix[9] =
{
  66, 129, 25, -38, -74, 112, 112, -94, -18,
};
static int32_t __ycbcr2rgbMatrix[9] =
{
  298, 0, 409, 298, -100, -208, 298, 516, 0
};

void
RGB2YUV(const LS_ColorRGB_t* rgb, LS_ColorYUV_t* yuv)
{
  int32_t r, g, b, y, u, v;

  r = rgb->redValue;
  g = rgb->greenValue;
  b = rgb->blueValue;
  y = 16 + ((128 + __rbg2ycbcrMatrix[0] * r + __rbg2ycbcrMatrix[1] * g + __rbg2ycbcrMatrix[2] * b) >> 8);
  u = 128 + ((128 + __rbg2ycbcrMatrix[3] * r + __rbg2ycbcrMatrix[4] * g + __rbg2ycbcrMatrix[5] * b) >> 8);
  v = 128 + ((128 + __rbg2ycbcrMatrix[6] * r + __rbg2ycbcrMatrix[7] * g + __rbg2ycbcrMatrix[8] * b) >> 8);
  yuv->yValue = (uint8_t)(y);
  yuv->uValue = (uint8_t)(u);
  yuv->vValue = (uint8_t)(v);
  yuv->alphaValue = rgb->alphaValue;
}


/**
 * Converts an YUV color to an RGB color, using ITU-R version formula
 *
 * @param[in] yuv the YUV color to convert.
 * @param[out] rgb the RGB representation of the color
 * @return None
 */
void
YUV2RGB(const LS_ColorYUV_t* yuv, LS_ColorRGB_t* rgb)
{
  int32_t r, g, b, y, u, v;

  y = yuv->yValue - 16;
  u = yuv->uValue - 128;
  v = yuv->vValue - 128;
  r = (128 + __ycbcr2rgbMatrix[0] * y + __ycbcr2rgbMatrix[1] * u + __ycbcr2rgbMatrix[2] * v) >> 8;
  g = (128 + __ycbcr2rgbMatrix[3] * y + __ycbcr2rgbMatrix[4] * u + __ycbcr2rgbMatrix[5] * v) >> 8;
  b = (128 + __ycbcr2rgbMatrix[6] * y + __ycbcr2rgbMatrix[7] * u + __ycbcr2rgbMatrix[8] * v) >> 8;
  rgb->redValue = CLAMP((uint8_t)(r), 0, 255);
  rgb->greenValue = CLAMP((uint8_t)(g), 0, 255);
  rgb->blueValue = CLAMP((uint8_t)(b), 0, 255);
  rgb->alphaValue = yuv->alphaValue;
}


/**
 * Converts an RGB color to an HSV color, according to the algorithm described
 * at http://en.wikipedia.org/wiki/HSL_and_HSV
 *
 * @param[in] hsv the RGB color to convert.
 * @param[out] the HSV representation of the color
 * @return None
 */
void
RGB2HSV(const LS_ColorRGB_t* rgb, LS_ColorHSV_t* hsv)
{
  float min, max, delta;
  float r, g, b, h, s, v;

  r = rgb->redValue;
  g = rgb->greenValue;
  b = rgb->blueValue;
  min = MIN3(r, g, b);
  max = MAX3(r, g, b);
  v = max;                                                                                  // v
  delta = max - min;

  if (max != 0)
  {
    s = delta / max * 255.0;
  }
  else
  {
    // r = g = b = 0
    // s = 0, v is undefined
    s = 0;
    h = -1;
    return;
  }

  if (r == max)
  {
    h = (g - b) / delta;                // between yellow & magenta
  }
  else if (g == max)
  {
    h = 2 + (b - r) / delta;            // between cyan & yellow
  }
  else
  {
    h = 4 + (r - g) / delta;            // between magenta & cyan
  }

  h *= 60;                              // degrees

  if (h < 0)
  {
    h += 360;
  }

  hsv->hValue = (uint8_t)(h + 0.5);
  hsv->sValue = (uint8_t)(s + 0.5);
  hsv->vValue = (uint8_t)(v + 0.5);
  hsv->alphaValue = rgb->alphaValue;
}


/**
 * Converts an HSV color to an RGB color, according to the algorithm described
 * at http://en.wikipedia.org/wiki/HSL_and_HSV
 *
 * @param[in] hsv the HSV color to convert.
 * @param[out] the RGB representation of the color
 * @return None
 */
void
HSV2RGB(const LS_ColorHSV_t* hsv, LS_ColorRGB_t* rgb)
{
  int32_t i;
  float f, p, q, t;
  float r, g, b, h, s, v;

  h = hsv->hValue;
  s = hsv->sValue;
  v = hsv->vValue;

  if (s == 0)
  {
    // achromatic (grey)
    rgb->redValue = rgb->greenValue = rgb->blueValue = v;
    return;
  }

  s /= 255.0;
  h /= 60.0;                                                                              // sector 0 to 5
  i = (int32_t)h;
  f = h - i;                                                                              // factorial part of h
  p = v * (1 - s);
  q = v * (1 - s * f);
  t = v * (1 - s * (1 - f));

  switch (i)
  {
    case 0:
      r = v;
      g = t;
      b = p;
      break;

    case 1:
      r = q;
      g = v;
      b = p;
      break;

    case 2:
      r = p;
      g = v;
      b = t;
      break;

    case 3:
      r = p;
      g = q;
      b = v;
      break;

    case 4:
      r = t;
      g = p;
      b = v;
      break;

    default:                                                                                // case 5:
      r = v;
      g = p;
      b = q;
      break;
  }

  rgb->redValue = (uint8_t)r;
  rgb->greenValue = (uint8_t)g;
  rgb->blueValue = (uint8_t)b;
  rgb->alphaValue = hsv->alphaValue;
}


void
ConvertCLUT82ARGB32(const uint8_t* src_data, uint32_t data_len, const LS_ColorRGB_t* clut, uint8_t* dest_data)
{
  int32_t i = 0;
  uint8_t idx = 0;

  if ((src_data == NULL) ||
      (clut == NULL) ||
      (dest_data == NULL) ||
      (data_len == 0))
  {
    LS_ERROR("Wrong parameters: src_data = %p," "lut=%p," "dest_data=%p," "data_len=%d\n",
             (void*)src_data,
             (void*)clut,
             (void*)dest_data,
             data_len);
    return;
  }

  LS_DEBUG("Converting CLUT_8 into ARGB32 ... ... \n");

#if ARGB32_NEEDS_BYTESWAP
  /* Little-endian: Convert [A,R,G,B] to [B,G,R,A] for standard ARGB32 format */
  LS_DEBUG("Using little-endian byte swap for ARGB32\n");

  while (i < (int32_t)data_len)
  {
    idx = src_data[i];

    dest_data[i * 4 + 0] = clut[idx].blueValue;       // B (LSB)
    dest_data[i * 4 + 1] = clut[idx].greenValue;      // G
    dest_data[i * 4 + 2] = clut[idx].redValue;        // R
    dest_data[i * 4 + 3] = clut[idx].alphaValue;      // A (MSB)

    i++;
  }
#else
  /* Big-endian: LS_ColorRGB_t [A,R,G,B] matches ARGB32 format */
  LS_DEBUG("Using direct copy for big-endian ARGB32\n");

  while (i < (int32_t)data_len)
  {
    idx = src_data[i];

    SYS_MEMCPY((dest_data + i * 4), (clut + idx), sizeof(LS_ColorRGB_t));

    i++;
  }
#endif
  LS_DEBUG("Converting CLUT_8 into ARGB32 ... ... Done\n");
}


void
ConvertCLUT82BGRA32(const uint8_t* src_data, uint32_t data_len, const LS_ColorRGB_t* clut, uint8_t* dest_data)
{
  int32_t i = 0;
  uint8_t idx = 0;

  if ((src_data == NULL) ||
      (clut == NULL) ||
      (dest_data == NULL) ||
      (data_len == 0))
  {
    LS_ERROR("Wrong parameters: src_data = %p," "lut=%p," "dest_data=%p," "data_len=%d\n",
             (void*)src_data,
             (void*)clut,
             (void*)dest_data,
             data_len);
    return;
  }

  LS_DEBUG("Converting CLUT_8 into BGRA32 ... ... \n");

#if ARGB32_NEEDS_BYTESWAP
  /* Little-endian: Convert [A,R,G,B] to [R,G,B,A] for standard BGRA32 format
   * BGRA32 means 0xAABBGGRR in register, which is [R,G,B,A] on little-endian */
  LS_DEBUG("Using little-endian byte swap for BGRA32\n");

  while (i < (int32_t)data_len)
  {
    idx = src_data[i];

    dest_data[i * 4 + 0] = clut[idx].redValue;        // R (LSB)
    dest_data[i * 4 + 1] = clut[idx].greenValue;      // G
    dest_data[i * 4 + 2] = clut[idx].blueValue;       // B
    dest_data[i * 4 + 3] = clut[idx].alphaValue;      // A (MSB)

    i++;
  }
#else
  /* Big-endian: BGRA32 (0xAABBGGRR) means [A,B,G,R] in memory
   * Convert from LS_ColorRGB_t [A,R,G,B] to [A,B,G,R] */
  LS_DEBUG("Using byte swap for big-endian BGRA32\n");

  while (i < (int32_t)data_len)
  {
    idx = src_data[i];

    dest_data[i * 4 + 0] = clut[idx].alphaValue;      // A (LSB)
    dest_data[i * 4 + 1] = clut[idx].blueValue;       // B
    dest_data[i * 4 + 2] = clut[idx].greenValue;      // G
    dest_data[i * 4 + 3] = clut[idx].redValue;        // R (MSB)

    i++;
  }
#endif
  LS_DEBUG("Converting CLUT_8 into BGRA32 ... ... Done\n");
}


void
ConvertCLUT82YUV420P(const uint8_t*       src_data,
                     int32_t              width,
                     int32_t              height,
                     const LS_ColorRGB_t* clut,
                     uint8_t*             dest_data)
{
  int frameSize = width * height;
  int chromasize = frameSize / 4;
  int i;
  int j;
  int yIndex = 0;
  int uIndex = frameSize;
  int vIndex = frameSize + chromasize;
  int R, G, B, Y, U, V;
  int pos = 0;
  int clutindex = 0;

  for (j = 0; j < height; j++)
  {
    for (i = 0; i < width; i++)
    {
      clutindex = src_data[pos];

      /* Bounds check: clutindex must be valid CLUT index (typically 0-255 for 8-bit) */
      if (clutindex > 255)
      {
        /* Invalid CLUT index - use black as fallback */
        R = G = B = 0;
      }
      else
      {
        R = clut[clutindex].redValue;
        G = clut[clutindex].greenValue;
        B = clut[clutindex].blueValue;
      }

      Y = ((66 * R + 129 * G + 25 * B + 128) >> 8) + 16;
      U = ((-38 * R - 74 * G + 112 * B + 128) >> 8) + 128;
      V = ((112 * R - 94 * G - 18 * B + 128) >> 8) + 128;
      dest_data[yIndex++] = CLAMP(Y, 0, 255);

      if ((j % 2 == 0) &&
          (pos % 2 == 0))
      {
        dest_data[uIndex++] = CLAMP(U, 0, 255);
        dest_data[vIndex++] = CLAMP(V, 0, 255);
      }

      pos++;
    }
  }

  LS_DEBUG("Converting CLUT_8 into YUV420P ... ... Done\n");
}


void
ConvertColor2RGBYUV(const LS_Color_t* color, LS_ColorYUV_t* yuv, LS_ColorRGB_t* rgb)
{
  if (color)
  {
    switch (color->colorMode)
    {
      case LS_COLOR_RGB:

        if (rgb)
        {
          SYS_MEMCPY((void*)(rgb), (void*)(&(color->colorData.rgbColor)), sizeof(LS_ColorRGB_t));

          if (yuv)
          {
            RGB2YCBCR(rgb->redValue, rgb->greenValue, rgb->blueValue, yuv->yValue, yuv->uValue, yuv->vValue);
          }
        }

        break;

      case LS_COLOR_YUV:

        if (yuv)
        {
          SYS_MEMCPY((void*)(yuv), (void*)(&(color->colorData.yuvColor)), sizeof(LS_ColorYUV_t));

          if (yuv)
          {
            YCBCR2RGB(yuv->yValue, yuv->uValue, yuv->vValue, rgb->redValue, rgb->greenValue, rgb->blueValue);
          }
        }

        break;

      default:
        LS_ERROR("Unsupported color model: %d\n", color->colorMode);
    }
  }
}
