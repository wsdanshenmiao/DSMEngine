#ifndef __PLATFORMDETECTION_H__
#define __PLATFORMDETECTION_H__


#ifdef _WIN32
	/* Windows x64/x86 */
	#ifdef _WIN64
		/* Windows x64  */
		#define DSM_PLATFORM_WINDOWS
	#else
		/* Windows x86 */
		#error "x86 Builds are not supported!"
	#endif
#else
	#error "Unknown platform!"
#endif

#if defined(DSM_PLATFORM_WINDOWS)
#define NOMINMAX
#include <Windows.h>
#include <comdef.h>
#include <commdlg.h>

#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

#endif

#endif