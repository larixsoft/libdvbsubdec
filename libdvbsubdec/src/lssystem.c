/*-----------------------------------------------------------------------------
 * lssystem.c
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

#ifndef _LS_SYSTEM_C
#define _LS_SYSTEM_C

#include <stdio.h>
#include <stdarg.h>

#include "lssystem.h"
#include "lssubmacros.h"
#include "lssubport.h"
/*-----------------------------------------------------------------------------
 * local static variable
 *----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------
 * public APIs
 *----------------------------------------------------------------------------*/
int32_t
LS_MutexCreate(LS_Mutex_t* mutex)
{
  LS_SystemFuncs_t* systemfunc = LS_GetSystemFuncs();

  if (systemfunc &&
      systemfunc->mutexCreateFunc)
  {
    return systemfunc->mutexCreateFunc(mutex);
  }
  else
  {
    return LS_ERROR_GENERAL;
  }
}


int32_t
LS_MutexDelete(LS_Mutex_t mutex)
{
  LS_SystemFuncs_t* systemfunc = LS_GetSystemFuncs();

  if (systemfunc &&
      systemfunc->mutexDeleteFunc)
  {
    return systemfunc->mutexDeleteFunc(mutex);
  }
  else
  {
    return LS_ERROR_GENERAL;
  }
}


int32_t
LS_MutexWait(LS_Mutex_t mutex)
{
  LS_SystemFuncs_t* systemfunc = LS_GetSystemFuncs();

  if (systemfunc &&
      systemfunc &&
      systemfunc->mutexWaitFunc)
  {
    return systemfunc->mutexWaitFunc(mutex);
  }
  else
  {
    return LS_ERROR_GENERAL;
  }
}


int32_t
LS_MutexSignal(LS_Mutex_t mutex)
{
  LS_SystemFuncs_t* systemfunc = LS_GetSystemFuncs();

  if (systemfunc &&
      systemfunc->mutexSignalFunc)
  {
    return systemfunc->mutexSignalFunc(mutex);
  }
  else
  {
    return LS_ERROR_GENERAL;
  }
}


int32_t
LS_TimerNew(LS_Timer_t* timer, void (*callbackfunc) (void* param), void* param)
{
  LS_SystemFuncs_t* systemfunc = LS_GetSystemFuncs();

  if (systemfunc &&
      systemfunc->timerCreateFunc)
  {
    return systemfunc->timerCreateFunc(timer, callbackfunc, param);
  }
  else
  {
    return LS_ERROR_GENERAL;
  }
}


int32_t
LS_TimerStart(LS_Timer_t timer, uint32_t time_ms)
{
  LS_SystemFuncs_t* systemfunc = LS_GetSystemFuncs();

  if (systemfunc &&
      systemfunc->timerStartFunc &&
      timer)
  {
    return systemfunc->timerStartFunc(timer, time_ms);
  }
  else
  {
    return LS_ERROR_GENERAL;
  }
}


int32_t
LS_TimerStop(LS_Timer_t timer, LS_Time_t* left_ms)
{
  LS_SystemFuncs_t* systemfunc = LS_GetSystemFuncs();

  if (systemfunc &&
      systemfunc->timerStopFunc &&
      timer)
  {
    return systemfunc->timerStopFunc(timer, left_ms);
  }
  else
  {
    return LS_ERROR_GENERAL;
  }
}


int32_t
LS_TimerDelete(LS_Timer_t timer)
{
  LS_SystemFuncs_t* systemfunc = LS_GetSystemFuncs();

  if (systemfunc &&
      systemfunc->timerDeleteFunc &&
      timer)
  {
    return systemfunc->timerDeleteFunc(timer);
  }
  else
  {
    return LS_ERROR_GENERAL;
  }
}


char*
LS_GetTimeStamp(void)
{
  LS_SystemFuncs_t* systemfunc = LS_GetSystemFuncs();

  if (systemfunc &&
      systemfunc->getTimeStampFunc)
  {
    return systemfunc->getTimeStampFunc();
  }
  else
  {
    return " ";
  }
}


int32_t
LS_Printf(int32_t level, const char* format, ...)
{
  va_list args;
  int32_t num = 0;
  LS_SystemLogger_t* systemlogger = LS_GetSystemLogger();

  if (systemlogger &&
      systemlogger->func &&
      (level <= (int32_t)systemlogger->level))
  {
    va_start(args, format);
    num = systemlogger->func(systemlogger->user_data, format, args);
    va_end(args);
  }
  else if (level <= LS_DEFAULT_VERB_LEVEL)
  {
    va_start(args, format);
    num = vprintf(format, args);
    va_end(args);
  }

  return num;
}


#endif
