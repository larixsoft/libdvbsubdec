/*-----------------------------------------------------------------------------
 * lssubport.c
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

#ifndef _LS_PORT_C
#define _LS_PORT_C

#include "lssubport.h"
#include "lssubmacros.h"
/*-----------------------------------------------------------------------------
 * local static variables
 *----------------------------------------------------------------------------*/
static LS_SystemFuncs_t* sSystemFuncs = NULL;
static LS_SystemLogger_t* sSystemLogger = NULL;

/*-----------------------------------------------------------------------------
 * public extern APIs
 *----------------------------------------------------------------------------*/
int32_t
LS_UpdateSystemFuncs(const LS_SystemFuncs_t sysFuncs)
{
  if (sSystemFuncs == NULL)
  {
    sSystemFuncs = (LS_SystemFuncs_t*)SYS_MALLOC(sizeof(LS_SystemFuncs_t));

    if (sSystemFuncs == NULL)
    {
      return LS_ERROR_SYSTEM_ERROR;
    }
  }

  SYS_MEMCPY((void*)(sSystemFuncs), (void*)&sysFuncs, sizeof(sysFuncs));
  return LS_OK;
}


LS_SystemFuncs_t*
LS_GetSystemFuncs(void)
{
  return sSystemFuncs;
}


void
LS_ResetSystemFuncs(void)
{
  if (sSystemFuncs)
  {
    SYS_MEMSET((void*)sSystemFuncs, 0, sizeof(LS_SystemFuncs_t));
    SYS_FREE((void*)sSystemFuncs);
    sSystemFuncs = NULL;
  }
}


int32_t
LS_UpdateSystemLogger(const LS_SystemLogger_t logger)
{
  if (sSystemLogger == NULL)
  {
    sSystemLogger = (LS_SystemLogger_t*)SYS_MALLOC(sizeof(LS_SystemLogger_t));

    if (sSystemLogger == NULL)
    {
      return LS_ERROR_SYSTEM_ERROR;
    }
  }

  SYS_MEMCPY((void*)(sSystemLogger), (void*)&logger, sizeof(logger));
  return LS_OK;
}


LS_SystemLogger_t*
LS_GetSystemLogger(void)
{
  return sSystemLogger;
}


void
LS_ResetSystemLogger(void)
{
  if (sSystemLogger)
  {
    SYS_MEMSET((void*)sSystemLogger, 0, sizeof(*sSystemLogger));
    SYS_FREE((void*)sSystemLogger);
    sSystemLogger = NULL;
  }
}


#endif
