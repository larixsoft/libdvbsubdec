/*-----------------------------------------------------------------------------
 * lssubdisplay.h
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
/**
 * @file lssubdisplay.h
 * @brief Display Page Management Functions
 *
 * This header provides functions for managing the display of subtitle pages,
 * including putting pages on screen and removing them.
 */

#ifndef LS_DISPLAY_H__
#define LS_DISPLAY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lssubdecoder.h"
/**
 * @brief Display a page on screen
 *
 * Renders the specified display page to the screen using the
 * configured OSD callbacks.
 *
 * @param page Display page to render
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_DisplayPageOnScreen(LS_DisplayPage* page);

/**
 * @brief Remove a page from screen
 *
 * Removes the specified display page from the screen, clearing
 * the subtitle regions.
 *
 * @param page Display page to remove
 * @return LS_OK (1) on success, error code on failure
 */
int32_t LS_RemovePageFromScreen(LS_DisplayPage* page);


#ifdef __cplusplus
}
#endif


#endif
