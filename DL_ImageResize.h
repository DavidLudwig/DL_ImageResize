// DL_ImageResize.h
// Public domain, Sep 26 2017, David Ludwig, dludwig@pobox.com. See "unlicense" statement at the end of this file.

#ifndef DL_ImageResize_h
#define DL_ImageResize_h

#include <stdint.h>

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
// This is free and unencumbered software released into the public domain.
// 
// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.
// 
// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the
// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this
// software under copyright law.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
// 
// For more information, please refer to <http://unlicense.org/>
//
