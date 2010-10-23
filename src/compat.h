/* MegaZeux
 *
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

// Provide some compatibility macros for combined C/C++ binary

#ifndef __COMPAT_H
#define __COMPAT_H

#include "config.h"

#ifdef __cplusplus

#define __M_BEGIN_DECLS extern "C" {
#define __M_END_DECLS   }

#else

#define __M_BEGIN_DECLS
#define __M_END_DECLS

#if !defined(CONFIG_WII) && !defined(CONFIG_NDS)

#undef false
#undef true
#undef bool

typedef enum
{
  false = 0,
  true  = 1,
} bool;

#endif // !CONFIG_WII && !CONFIG_NDS

#endif /* __cplusplus */

#ifdef CONFIG_NDS
#include <nds.h>
#endif

#ifdef CONFIG_WII
#define BOOL _BOOL
#include <gctypes.h>
#undef BOOL
#endif

#ifdef CONFIG_EDITOR
#define __editor_maybe_static
#else
#define __editor_maybe_static static
#endif

#ifdef CONFIG_UPDATER
#define __updater_maybe_static
#else
#define __updater_maybe_static static
#endif

#ifdef CONFIG_UTILS
#define __utils_maybe_static
#else
#define __utils_maybe_static static
#endif

#ifdef CONFIG_AUDIO
#if defined(CONFIG_MODPLUG) || defined(CONFIG_MIKMOD)
#define __audio_c_maybe_static
#else // !CONFIG_MODPLUG && !CONFIG_MIKMOD
#define __audio_c_maybe_static static
#endif // CONFIG_MODPLUG || CONFIG_MIKMOD
#else // !CONFIG_AUDIO
#define __audio_c_maybe_static static
#endif // CONFIG_AUDIO

#ifdef _MSC_VER
#include "msvc.h"
#endif

#ifdef __WIN32__
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifdef ANDROID
#define HAVE_SYS_UIO_H
#define LOG_TAG "MegaZeux"
#include <cutils/log.h>
#undef CONDITION
#endif

#ifdef CONFIG_DJGPP
// FIXME: DANGEROUS HACK! FIX AS SOON AS POSSIBLE!
#include <stdio.h>
#define snprintf(ptr, size, ...) sprintf(ptr, __VA_ARGS__)
#endif

#ifndef MAX_PATH
#define MAX_PATH 512
#endif

#if defined(CONFIG_MODULAR) && defined(__WIN32__)
#define LIBSPEC __declspec(dllimport)
#elif defined(CONFIG_MODULAR)
#define LIBSPEC __attribute__((visibility("default")))
#else
#define LIBSPEC
#endif

#ifndef CORE_LIBSPEC
#define CORE_LIBSPEC LIBSPEC
#endif

#ifndef EDITOR_LIBSPEC
#define EDITOR_LIBSPEC LIBSPEC
#endif

#ifdef CONFIG_UPDATER
#define UPDATER_LIBSPEC CORE_LIBSPEC
#else
#define UPDATER_LIBSPEC
#endif

__M_BEGIN_DECLS

#ifdef CONFIG_CHECK_ALLOC

#include <stddef.h>

#define ccalloc(nmemb, size) check_calloc(nmemb, size, __FILE__, __LINE__)
CORE_LIBSPEC void *check_calloc(size_t nmemb, size_t size, const char *file,
 int line);

#define cmalloc(size) check_malloc(size, __FILE__, __LINE__)
CORE_LIBSPEC void *check_malloc(size_t size, const char *file,
 int line);

#define crealloc(ptr, size) check_realloc(ptr, size, __FILE__, __LINE__)
CORE_LIBSPEC void *check_realloc(void *ptr, size_t size, const char *file,
 int line);

#else /* CONFIG_CHECK_ALLOC */

#include <stdlib.h>

static inline void *ccalloc(size_t nmemb, size_t size)
{
  return calloc(nmemb, size);
}

static inline void *cmalloc(size_t size)
{
  return malloc(size);
}

static inline void *crealloc(void *ptr, size_t size)
{
  return realloc(ptr, size);
}

#endif /* CONFIG_CHECK_ALLOC */

__M_END_DECLS

#endif // __COMPAT_H
