//============================================================
//
//  wintime.c - Win32 OSD core timing functions
//
//  Copyright (c) 1996-2007, Nicola Salmoria and the MAME Team.
//  Visit http://mamedev.org for licensing and usage restrictions.
//
//============================================================

// standard windows headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

// MAME headers
#include "osdepend.h"



//============================================================
//  GLOBAL VARIABLES
//============================================================

static osd_ticks_t ticks_per_second = 0;
static osd_ticks_t suspend_adjustment = 0;
static osd_ticks_t suspend_ticks = 0;
static BOOL using_qpc = TRUE;



//============================================================
//  osd_ticks
//============================================================

osd_ticks_t osd_ticks(void)
{
	LARGE_INTEGER performance_count;

	// if we're suspended, just return that
	if (suspend_ticks != 0)
		return suspend_ticks;

	// if we have a per second count, just go for it
	if (ticks_per_second != 0)
	{
		// QueryPerformanceCounter if we can
		if (using_qpc)
		{
			QueryPerformanceCounter(&performance_count);
			return (osd_ticks_t)performance_count.QuadPart - suspend_ticks;
		}

		// otherwise, fall back to timeGetTime
		else
			return (osd_ticks_t)timeGetTime() - suspend_ticks;
	}

	// if not, we have to determine it
	using_qpc = QueryPerformanceFrequency(&performance_count) && (performance_count.QuadPart != 0);
	if (using_qpc)
		ticks_per_second = (osd_ticks_t)performance_count.QuadPart;
	else
		ticks_per_second = 1000;

	// call ourselves to get the first value
	return osd_ticks();
}


//============================================================
//  osd_ticks_per_second
//============================================================

osd_ticks_t osd_ticks_per_second(void)
{
	return ticks_per_second;
}


//============================================================
//  osd_profiling_ticks
//============================================================

#ifdef _MSC_VER

#ifdef PTR64

osd_ticks_t osd_profiling_ticks(void)
{
	return __rdtsc();
}

#else

osd_ticks_t osd_profiling_ticks(void)
{
	INT64 result;
	INT64 *presult = &result;

	__asm {
		__asm _emit 0Fh __asm _emit 031h	// rdtsc
		mov ebx, presult
		mov [ebx],eax
		mov [ebx+4],edx
	}

	return result;
}

#endif

#else

osd_ticks_t osd_profiling_ticks(void)
{
	INT64 result;

	// use RDTSC
	__asm__ __volatile__ (
		"rdtsc"
		: "=A" (result)
	);

	return result;
}

#endif


//============================================================
//  osd_sleep
//============================================================

void osd_sleep(osd_ticks_t duration)
{
	DWORD msec;

	// make sure we've computed ticks_per_second
	if (ticks_per_second == 0)
		(void)osd_ticks();

	// convert to milliseconds, rounding down
	msec = (DWORD)(duration * 1000 / ticks_per_second);

	// only sleep if at least 2 full milliseconds
	if (msec >= 2)
	{
		HANDLE current_thread = GetCurrentThread();
		int old_priority = GetThreadPriority(current_thread);

		// take a couple of msecs off the top for good measure
		msec -= 2;

		// bump our thread priority super high so that we get
		// priority when we need it
		SetThreadPriority(current_thread, THREAD_PRIORITY_TIME_CRITICAL);
		Sleep(msec);
		SetThreadPriority(current_thread, old_priority);
	}
}


//============================================================
//  win_timer_enable
//============================================================

void win_timer_enable(int enabled)
{
	// get the current count
	osd_ticks_t current_ticks = osd_ticks();

	// if we're disabling, remember the time
	if (!enabled && suspend_ticks == 0)
		suspend_ticks = current_ticks;

	// if we're enabling, compute the adjustment
	else if (suspend_ticks > 0)
	{
		suspend_adjustment += current_ticks - suspend_ticks;
		suspend_ticks = 0;
	}
}
