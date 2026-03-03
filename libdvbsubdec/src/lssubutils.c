/*-----------------------------------------------------------------------------
 * lssubutils.c
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

#include <math.h>
#include "lssubdec.h"
#include "lssubutils.h"
#include "lssubmacros.h"
#include "lssystem.h"

uint32_t
ReadBitStream32(uint8_t* data, uint32_t bytes_offset, uint32_t bits_offset, uint32_t number_bits, int32_t* status)
{
  int32_t ret_val;
  uint8_t* startaddr = NULL;
  uint32_t bytes;
  uint64_t tmpval;
  uint64_t mask;
  int32_t totalbits;
  int32_t startbit;

  if ((number_bits == 0) ||
      (number_bits > (sizeof(uint32_t) * 8)))
  {
    LS_ERROR("out of range\n");
    *status = LS_ERROR_GENERAL;
    return (uint32_t)0;
  }

  startaddr = (uint8_t*)(data + bytes_offset + bits_offset / 8);
  bits_offset = bits_offset % 8;
  /*how many bytes we need to read*/
  bytes = (bits_offset + number_bits + 7) / 8;

  switch (bytes)
  {
    case 1:
      tmpval = (uint64_t)startaddr[0];
      totalbits = 8;
      break;

    case 2:
      tmpval = (((uint64_t)startaddr[0]) << 8) + ((uint64_t)startaddr[1]);
      totalbits = 16;
      break;

    case 3:
      tmpval = (((uint64_t)startaddr[0]) << 16) + (((uint64_t)startaddr[1]) << 8) + ((uint64_t)startaddr[2]);
      totalbits = 24;
      break;

    case 4:
      tmpval = (((uint64_t)startaddr[0]) << 24) + (((uint64_t)startaddr[1]) << 16) + (((uint64_t)startaddr[2]) <<
    8) + ((uint64_t)startaddr[3]);
      totalbits = 32;
      break;

    case 5:
      tmpval = (((uint64_t)startaddr[0]) << 32) + (((uint64_t)startaddr[1]) << 24) + (((uint64_t)startaddr[2]) <<
    16) + (((uint64_t)startaddr[3]) << 8) + ((uint64_t)startaddr[4]);
      totalbits = 40;
      break;

    default:
      LS_ERROR("out of range\n");
      *status = LS_ERROR_GENERAL;
      return (uint32_t)0;
  }

  startbit = totalbits - (bits_offset + number_bits);
  tmpval = tmpval >> startbit;
  mask = ((uint64_t)1 << number_bits) - 1;
  ret_val = (uint32_t)(tmpval & mask);
  *status = LS_OK;
  return ret_val;
}


/**
 * Converts an RGB color to an HSV color, according to the algorithm described
 * at http://en.wikipedia.org/wiki/HSL_and_HSV
 *
 * @param[in] rgb the RGB color to convert.
 * @param[out] hsv the HSV representation of the color
 * @return None
 */
void
RGBtoHSV(const LS_ColorRGB_t* rgb, LS_ColorHSV_t* hsv)
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
    s = delta / max * 255.0;                                                // s
  }
  else
  {
    // r = g = b = 0        // s = 0, v is undefined
    s = 0;
    h = -1;
    return;
  }

  if (r == max)
  {
    h = (g - b) / delta;                                                    // between yellow & magenta
  }
  else if (g == max)
  {
    h = 2 + (b - r) / delta;                                            // between cyan & yellow
  }
  else
  {
    h = 4 + (r - g) / delta;                                            // between magenta & cyan
  }

  h *= 60;                                                              // degrees

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
 * @param[out] rgb the RGB representation of the color
 * @return None
 */
void
HSVtoRGB(const LS_ColorHSV_t* hsv, LS_ColorRGB_t* rgb)
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


char*
PTStoHMS(const uint64_t pts)
{
  uint32_t pts_ms;
  uint32_t left_ms;
  int hours;
  int minutes;
  int seconds;
  int milliseconds;
  static char timestr[20];  // Increased size to accommodate snprintf safety margin

  pts_ms = (uint32_t)(pts / 90);
  hours = pts_ms / (60 * 60 * 1000);
  left_ms = pts_ms % (60 * 60 * 1000);
  minutes = left_ms / (60 * 1000);
  left_ms = left_ms % (60 * 1000);
  seconds = left_ms / 1000;
  milliseconds = left_ms % 1000;
  snprintf(timestr, sizeof(timestr), "%02dh:%02dm:%02ds:%03dms", hours, minutes, seconds, milliseconds);
  return timestr;
}
