/*-----------------------------------------------------------------------------
 * lssubgfx.c
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

#include "lssubdec.h"
#include "lssubgfx.h"
#include "lssuben300743.h"
#include "lssubmacros.h"

void
LS_FillRegionColorARGB32(uint32_t width, uint32_t height, uint8_t* data, LS_ColorRGB_t* color)
{
  uint8_t* start = NULL;
  int32_t i, j;

  for (i = 0; i < (int32_t)height; i++)
  {
    start = data + i * width * 4;

    for (j = 0; j < (int32_t)width; j++)
    {
      SYS_MEMCPY((void*)start, (void*)color, 4);
      start += 4;
    }
  }
}


void
LS_FillRegionColorAYUV32(uint32_t width, uint32_t height, uint8_t* data, LS_ColorYUV_t* color)
{
  uint8_t* start = NULL;
  int32_t i, j;

  for (i = 0; i < (int32_t)height; i++)
  {
    start = data + i * width * 4;

    for (j = 0; j < (int32_t)width; j++)
    {
      SYS_MEMCPY((void*)start, (void*)color, 4);
      start += 4;
    }
  }
}
