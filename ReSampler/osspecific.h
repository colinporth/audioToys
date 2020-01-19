// macros which address differences between compilers / operating systems go here.
//{{{
/*
* Copyright (C) 2016 - 2020 Judd Niemann - All Rights Reserved.
* You may use, distribute and modify this code under the
* terms of the GNU Lesser General Public License, version 2.1
*
* You should have received a copy of GNU Lesser General Public License v2.1
* with this file. If not, please refer to: https://github.com/jniemann66/ReSampler
*/
//}}}
#pragma once

#ifdef _WIN32
  #if defined(_MSC_VER)
    /* MSVC */
    #define NOMINMAX // disable min() and max() macros (use std:: library instead)
    #pragma warning(disable : 4996) // suppress pointless MS "deprecation" warnings
    #pragma warning(disable : 4244) // suppress double-to-float warnings
    #define BYTESWAP_METHOD_MSVCRT
  #else
    /* Not MSVC */
    #define BYTESWAP_METHOD_BUILTIN // note : gcc >= 4.8.1 , clang >= 3.5
  #endif

  #define TEMPFILE_OPEN_METHOD_WINAPI
  // Note: tmpfile() doesn't seem to work reliably with MSVC - probably related to this:
  // http://www.mega-nerd.com/libsndfile/api.html#open_fd (see note regarding differing versions of MSVC runtime DLL)

  #ifndef UNICODE
    #define UNICODE // turns TCHAR into wchar_t
  #endif

  #include <Windows.h>
  #include <codecvt>

#else
  #include <cstdint>
  typedef uint64_t __int64;
  #define stricmp strcasecmp
  #define TEMPFILE_OPEN_METHOD_STD_TMPFILE
  #define BYTESWAP_METHOD_BUILTIN
#endif
