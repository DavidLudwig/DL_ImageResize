// DL_ImageResize.h
// zlib-licensed, Oct 8 2017, David Ludwig, dludwig@pobox.com. See "COPYING" statement at the end of this file.

#ifndef DL_ImageResize_h
#define DL_ImageResize_h

#include <stdint.h>

#if !defined(DLIR_USE_SSE2)
    #if defined(__has_include)
        #if __has_include("xmmintrin.h")
            #define DLIR_USE_SSE2 1
        #else
            #undef DLIR_USE_SSE2
        #endif
    #else
        #undef DLIR_USE_SSE2
    #endif
#endif

#if !defined(DLIR_USE_SSE41)
    #if defined(__has_include)
        #if __has_include("smmintrin.h")
            #define DLIR_USE_SSE41 1
        #else
            #undef DLIR_USE_SSE41
        #endif
    #else
        #undef DLIR_USE_SSE41
    #endif
#endif

extern "C"
void DLIR_ResizeBilinear_ARGB8888(
    void * src,           // source buffer
    uint32_t srcX,        // left-bounds of source region, in pixels
    uint32_t srcY,        // top-bounds of source region, in pixels
    uint32_t srcWidth,    // width of source region, in pixels
    uint32_t srcHeight,   // height of source region, in pixels
    uint32_t srcPitch,    // byte-width of a row in the source image

    void * dest,          // destination buffer
    uint32_t destX,       // left-bounds of dest region, in pixels
    uint32_t destY,       // top-bounds of dest region, in pixels
    uint32_t destWidth,   // width of dest region, in pixels
    uint32_t destHeight,  // height of dest region, in pixels
    uint32_t destPitch,   // byte-width of a row in the destination image

    int32_t srcUpdateX,
    int32_t srcUpdateY,
    int32_t srcUpdateWidth,
    int32_t srcUpdateHeight
);

#endif  // ifndef DL_ImageResize_h


//
// COPYING
//
// Copyright (c) 2017 David Lee Ludwig
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
//     David Lee Ludwig <dludwig@pobox.com>
//
