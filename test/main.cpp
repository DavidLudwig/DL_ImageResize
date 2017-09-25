// main.cpp
// Public domain, Sep 23 2017, David Ludwig, dludwig@pobox.com. See "unlicense" statement at the end of this file.

#include "DLTest.h"
#include <string>
#include <cstring>
#include <unordered_map>

SDL_Surface * src = NULL;

struct Render_Src : public DLT_Renderer {
    virtual void Init(DLT_Env & env) {
        //env.dest = SDL_CreateRGBSurfaceWithFormat(0, src->w, src->h, src->format->BitsPerPixel, src->format->format);
        env.dest = SDL_CreateRGBSurface(0, src->w, src->h, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
    }
    virtual void Draw(DLT_Env & env) {
        SDL_Rect destRect = {0, 0, src->w, src->h};
        SDL_UpperBlit(src, NULL, env.dest, &destRect);
    }
};
DLT_RegisterRenderer(Render_Src, "SRC", "source image, no scaling");

struct Render_Src2X : public DLT_Renderer {
    virtual void Init(DLT_Env & env) {
        env.dest = SDL_CreateRGBSurface(0, src->w * 2, src->h * 2, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
    }
    virtual void Draw(DLT_Env & env) {
        SDL_Rect destRect = {0, 0, src->w * 2, src->h * 2};
        SDL_BlitScaled(src, NULL, env.dest, &destRect);
    }
};
DLT_RegisterRenderer(Render_Src2X, "SRC2X", "source image with size * 2, via SDL_BlitScaled");



int ScaleWidth = 448;
int ScaleHeight = 448;
bool Fullscreen = false;

// int ScaleWidth = 1300;
// int ScaleHeight = 800;

// int ScaleWidth = 1920;
// int ScaleHeight = 1080;

struct Render_ScalerBase : public DLT_Renderer {

    void * buffer = NULL;
    virtual void Init(DLT_Env & env) {
        if (Fullscreen) {
            SDL_Rect r = {0, 0, 0, 0};
            if (SDL_GetDisplayBounds(0, &r) != 0) {
                printf("ERROR: Unable to get display bounds! reason=\"%s\"\n", SDL_GetError());
                exit(1);
                return;
            }
            ScaleWidth = r.w;
            ScaleHeight = r.h;
            fprintf(stderr, "# Using fullscreen display size: %dx%d\n", r.w, r.h);
        }
        
        // Create a pixel buffer aligned on a 16-byte boundary, in case we want
        // to use SSE2 aligned I/O.
        buffer = calloc(1, (ScaleWidth * ScaleHeight * 4) + 16);
        void * alignedBuffer = (void *)(((uintptr_t)buffer+15) & ~ 0x0F);
        env.dest = SDL_CreateRGBSurfaceFrom(
            alignedBuffer, ScaleWidth, ScaleHeight, 32, (ScaleWidth * 4), 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
    }
    //virtual std::string GetShortName() = 0;
    virtual void Draw(DLT_Env & env) = 0;
};

struct Render_ScaleSDL : public Render_ScalerBase {
    virtual void Draw(DLT_Env & env) {
        SDL_BlitScaled(src, NULL, env.dest, NULL);
    }
};
DLT_RegisterRenderer(Render_ScaleSDL, "ScaleSDL", "SDL_BlitScaled, which uses nearest-neighbor scaling");



void BilinearScale1(uint32_t * src, int srcWidth, int srcHeight, uint32_t * dest, int destWidth, int destHeight) {
    uint32_t s0, s1, s2, s3;
    int srcX, srcY, srcIndex;
    float ratioDestXtoSrcX = ((float)(srcWidth - 1)) / destWidth;
    float ratioDestYToSrcY = ((float)(srcHeight - 1)) / destHeight;
    float diffX, diffY, blue, red, green;
    int destIndex = 0;
    for (int destY = 0; destY < destHeight; destY++) {
        srcY = (int)(ratioDestYToSrcY * destY);
        diffY = (ratioDestYToSrcY * destY) - srcY;
        for (int destX = 0; destX < destWidth; destX++) {
            srcX = (int)(ratioDestXtoSrcX * destX);
            diffX = (ratioDestXtoSrcX * destX) - srcX;
            srcIndex = (srcY * srcWidth + srcX);
            s0 = src[srcIndex];
            s1 = src[srcIndex + 1];
            s2 = src[srcIndex + srcWidth];
            s3 = src[srcIndex + srcWidth + 1];
            
            blue = \
                ((s0    ) & 0xff) * (1 - diffX) * (1 - diffY) +
                ((s1    ) & 0xff) * (    diffX) * (1 - diffY) +
                ((s2    ) & 0xff) * (1 - diffX) * (    diffY) +
                ((s3    ) & 0xff) * (    diffX) * (    diffY);
            
            green = \
                ((s0>> 8) & 0xff) * (1 - diffX) * (1 - diffY) +
                ((s1>> 8) & 0xff) * (    diffX) * (1 - diffY) +
                ((s2>> 8) & 0xff) * (1 - diffX) * (    diffY) +
                ((s3>> 8) & 0xff) * (    diffX) * (    diffY);
            
            red = \
                ((s0>>16) & 0xff) * (1 - diffX) * (1 - diffY) +
                ((s1>>16) & 0xff) * (    diffX) * (1 - diffY) +
                ((s2>>16) & 0xff) * (1 - diffX) * (    diffY) +
                ((s3>>16) & 0xff) * (    diffX) * (    diffY);
            
            dest[destIndex++] =
                0xff000000 |
                ((((int)red  ) <<16 ) & 0xff0000) |
                ((((int)green) << 8 ) &   0xff00) |
                  ((int)blue ) ;
        }
    }
}

struct Render_Bilinear1 : public Render_ScalerBase {
    virtual void Draw(DLT_Env & env) {
        BilinearScale1((uint32_t*)src->pixels, src->w, src->h, (uint32_t*)env.dest->pixels, env.dest->w, env.dest->h);
    }
};
DLT_RegisterRenderer(Render_Bilinear1, "Bi1", "initial, bilinear scaler, with 'float' based math");

// #define FIXED 16
// typedef int fixed;
// typedef int64_t fixedbig;

#if 1
    #define FIXED 10
    #define FRAC_BITS 0x3FF
    typedef uint32_t fixed;
    typedef uint32_t fixedbig;
#else
    #define FIXED 10
    // #define FRAC_BITS 0x3FF
    typedef uint16_t fixed;
    typedef uint32_t fixedbig;
#endif


void BilinearScale2(uint32_t * src, int srcWidth, int srcHeight, uint32_t * dest, int destWidth, int destHeight) {
    uint32_t s0, s1, s2, s3;
    int srcX, srcY, srcIndex;

    fixed ratioDestXtoSrcX = (((srcWidth - 1)<<FIXED) / destWidth);
    fixed ratioDestYToSrcY = (((srcHeight - 1)<<FIXED) / destHeight);
    fixedbig diffX, diffY;
    fixedbig blue, green, red;
    
    int destIndex = 0;
    for (int destY = 0; destY < destHeight; destY++) {
        srcY = ((fixedbig)ratioDestYToSrcY * (fixedbig)(destY<<FIXED))>>(FIXED*2);
        diffY = (((fixedbig)ratioDestYToSrcY * (fixedbig)(destY<<FIXED))>>FIXED) - (srcY << FIXED);
        // diffY = (((fixedbig)ratioDestYToSrcY * (fixedbig)(destY<<FIXED))>>FIXED) & FRAC_BITS;

        for (int destX = 0; destX < destWidth; destX++) {
            srcX  =  ((fixedbig)ratioDestXtoSrcX * (fixedbig)(destX<<FIXED))>>(FIXED*2);
            diffX = (((fixedbig)ratioDestXtoSrcX * (fixedbig)(destX<<FIXED))>>FIXED) - (srcX << FIXED);
            // diffX = (((fixedbig)ratioDestXToSrcX * (fixedbig)(destX<<FIXED))>>FIXED) & FRAC_BITS;

            srcIndex = (srcY * srcWidth + srcX);
            s0 = src[srcIndex];
            s1 = src[srcIndex + 1];
            s2 = src[srcIndex + srcWidth];
            s3 = src[srcIndex + srcWidth + 1];

            fixedbig factor3 = ((             diffX) * (             diffY)) >> FIXED;
            fixedbig factor2 = (((1<<FIXED) - diffX) * (             diffY)) >> FIXED;
            fixedbig factor1 = ((             diffX) * ((1<<FIXED) - diffY)) >> FIXED;
            fixedbig factor0 = (((1<<FIXED) - diffX) * ((1<<FIXED) - diffY)) >> FIXED;

            blue = ( \
                (((s0    ) & 0xff)<<FIXED) * factor0 +
                (((s1    ) & 0xff)<<FIXED) * factor1 +
                (((s2    ) & 0xff)<<FIXED) * factor2 +
                (((s3    ) & 0xff)<<FIXED) * factor3
            ) >> FIXED;
            
            green = ( \
                (((s0>> 8) & 0xff)<<FIXED) * factor0 +
                (((s1>> 8) & 0xff)<<FIXED) * factor1 +
                (((s2>> 8) & 0xff)<<FIXED) * factor2 +
                (((s3>> 8) & 0xff)<<FIXED) * factor3
            ) >> FIXED;
            
            red = ( \
                (((s0>>16) & 0xff)<<FIXED) * factor0 +
                (((s1>>16) & 0xff)<<FIXED) * factor1 +
                (((s2>>16) & 0xff)<<FIXED) * factor2 +
                (((s3>>16) & 0xff)<<FIXED) * factor3
            ) >> FIXED;
            
            dest[destIndex++] =
                0xff000000 |
                (((red  >>FIXED) << 16) & 0xff0000) |
                (((green>>FIXED) <<  8) &   0xff00) |
                 ((blue >>FIXED) <<  0);
        }
    }
}

struct Render_Bilinear2 : public Render_ScalerBase {
    virtual void Draw(DLT_Env & env) {
        BilinearScale2((uint32_t*)src->pixels, src->w, src->h, (uint32_t*)env.dest->pixels, env.dest->w, env.dest->h);
    }
};
DLT_RegisterRenderer(Render_Bilinear2, "Bi2", "Bi1 renderer, with 'fixed-point' based math");


extern "C" void BilinearScale3_x86(uint32_t * src, uint32_t srcWidth, uint32_t srcHeight, uint32_t * dest, uint32_t destWidth, uint32_t destHeight);

struct Render_Bilinear3 : public Render_ScalerBase {
    virtual void Draw(DLT_Env & env) {
        BilinearScale3_x86((uint32_t*)src->pixels, src->w, src->h, (uint32_t*)env.dest->pixels, env.dest->w, env.dest->h);
    }
};
DLT_RegisterRenderer(Render_Bilinear3, "Bi3", "post-Bi2 renderer, in hand-coded assembly, 1st pass");


extern "C" void BilinearScale4_x86(uint32_t * src, uint32_t srcWidth, uint32_t srcHeight, uint32_t * dest, uint32_t destWidth, uint32_t destHeight);

struct Render_Bilinear4 : public Render_ScalerBase {
    virtual void Draw(DLT_Env & env) {
        BilinearScale4_x86((uint32_t*)src->pixels, src->w, src->h, (uint32_t*)env.dest->pixels, env.dest->w, env.dest->h);
    }
};
DLT_RegisterRenderer(Render_Bilinear4, "Bi4", "Bi3 renderer, with minor, working optimizations (on Penryn)");


extern "C" void BilinearScale5_x86(uint32_t * src, uint32_t srcWidth, uint32_t srcHeight, uint32_t * dest, uint32_t destWidth, uint32_t destHeight);

struct Render_Bilinear5 : public Render_ScalerBase {
    virtual void Draw(DLT_Env & env) {
        BilinearScale5_x86((uint32_t*)src->pixels, src->w, src->h, (uint32_t*)env.dest->pixels, env.dest->w, env.dest->h);
    }
};
DLT_RegisterRenderer(Render_Bilinear5, "Bi5", "Bi4 renderer, with \"optimizations\" (slower, albeit less stack use)");



#include "emmintrin.h"  // SSE2
#include "smmintrin.h"  // SSE4.1
#include "tmmintrin.h"  // SSSE3

void BilinearScale6(uint32_t * src, uint32_t srcWidth, uint32_t srcHeight, uint32_t * dest, uint32_t destWidth, uint32_t destHeight) {
    int srcX, srcY, srcIndex;

    fixed ratioDestXtoSrcX = (((srcWidth - 1)<<FIXED) / destWidth);
    fixed ratioDestYToSrcY = (((srcHeight - 1)<<FIXED) / destHeight);
    fixedbig diffX, diffY;
    
    int destIndex = 0;
    for (int destY = 0; destY < destHeight; destY++) {
        srcY = ((fixedbig)ratioDestYToSrcY * (fixedbig)(destY<<FIXED))>>(FIXED*2);
        diffY = (((fixedbig)ratioDestYToSrcY * (fixedbig)(destY<<FIXED))>>FIXED) - (srcY << FIXED);
        // diffY = (((fixedbig)ratioDestYToSrcY * (fixedbig)(destY<<FIXED))>>FIXED) & FRAC_BITS;

        for (int destX = 0; destX < destWidth; destX++) {
            srcX  =  ((fixedbig)ratioDestXtoSrcX * (fixedbig)(destX<<FIXED))>>(FIXED*2);
            diffX = (((fixedbig)ratioDestXtoSrcX * (fixedbig)(destX<<FIXED))>>FIXED) - (srcX << FIXED);
            // diffX = (((fixedbig)ratioDestXToSrcX * (fixedbig)(destX<<FIXED))>>FIXED) & FRAC_BITS;

            srcIndex = (srcY * srcWidth + srcX);

            __m128i s01 = _mm_loadl_epi64((__m128i const*)(src + srcIndex));
            __m128i s23 = _mm_loadl_epi64((__m128i const*)(src + srcIndex + srcWidth));
            __m128i s__23 = _mm_slli_si128(s23, 8);
            __m128i s0123 = _mm_or_si128(s01, s__23);
            
            __m128i reds   = _mm_slli_epi32(_mm_srli_epi32(_mm_slli_epi32(s0123,  8), 24), FIXED);
            __m128i greens = _mm_slli_epi32(_mm_srli_epi32(_mm_slli_epi32(s0123, 16), 24), FIXED);
            __m128i blues  = _mm_slli_epi32(_mm_srli_epi32(_mm_slli_epi32(s0123, 24), 24), FIXED);
            
            fixedbig factor3 = ((             diffX) * (             diffY)) >> FIXED;
            fixedbig factor2 = (((1<<FIXED) - diffX) * (             diffY)) >> FIXED;
            fixedbig factor1 = ((             diffX) * ((1<<FIXED) - diffY)) >> FIXED;
            fixedbig factor0 = (((1<<FIXED) - diffX) * ((1<<FIXED) - diffY)) >> FIXED;

            __m128i factors = _mm_setr_epi32(factor0, factor1, factor2, factor3);

            __m128i reds2   = _mm_srli_epi32(_mm_mullo_epi32(reds,   factors), FIXED);
            __m128i greens2 = _mm_srli_epi32(_mm_mullo_epi32(greens, factors), FIXED);
            __m128i blues2  = _mm_srli_epi32(_mm_mullo_epi32(blues,  factors), FIXED);
            
            __m128i reds3   = _mm_hadd_epi32(reds2,   reds2);
            __m128i greens3 = _mm_hadd_epi32(greens2, greens2);
            __m128i blues3  = _mm_hadd_epi32(blues2,  blues2);

            __m128i reds4   = _mm_hadd_epi32(reds3,   reds3);
            __m128i greens4 = _mm_hadd_epi32(greens3, greens3);
            __m128i blues4  = _mm_hadd_epi32(blues3,  blues3);

            uint32_t red5 = _mm_extract_epi32(reds4,     0);
            uint32_t green5 = _mm_extract_epi32(greens4, 0);
            uint32_t blue5 = _mm_extract_epi32(blues4,   0);
            
            dest[destIndex++] =
                0xff000000 |
                (((red5  >>FIXED) << 16) & 0xff0000) |
                (((green5>>FIXED) <<  8) &   0xff00) |
                 ((blue5 >>FIXED) <<  0);
        }
    }
}

struct Render_Bilinear6 : public Render_ScalerBase {
    virtual void Draw(DLT_Env & env) {
        BilinearScale6((uint32_t*)src->pixels, src->w, src->h, (uint32_t*)env.dest->pixels, env.dest->w, env.dest->h);
    }
};
DLT_RegisterRenderer(Render_Bilinear6, "Bi6", "Bi2 renderer, with Bi3 style SSE 4.1 use, via intrinsics");



int main(int argc, char ** argv) {
    std::string srcFile;
    
    for (int i = 1; i < argc; ++i) {
        // const char * dot = SDL_strrchr(argv[i], '.');
        // if (dot && SDL_strcasecmp(dot, ".bmp") == 0) {
        //     srcFile = argv[i];
        // }

        if (SDL_strcmp(argv[i], "-f") == 0 || SDL_strcmp(argv[i], "--src") == 0) {
            if (++i < argc) {
                srcFile = argv[i];
            }
        } else if (SDL_strcmp(argv[i], "-s") == 0 || SDL_strcmp(argv[i], "--scale") == 0) {
            if (++i < argc) {
                if (SDL_strcasecmp(argv[i], "full") == 0) {
                    ScaleWidth = -1;
                    ScaleHeight = -1;
                    Fullscreen = true;
                } else {
                    sscanf(argv[i], "%dx%d", &ScaleWidth, &ScaleHeight);
                }
            }
        }
    }

    // Load source data
    {
        SDL_Surface * srcTemp = NULL;
        if (srcFile.empty()) {
            srcTemp = SDL_CreateRGBSurface(0, 330, 330, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
            for (int y = 0; y < srcTemp->h; ++y) {
                for (int x = 0; x < srcTemp->w; ++x) {
                    static uint32_t c[3] = { 0xffff0000, 0xff00ff00, 0xff0000ff };
                    ((uint32_t *)srcTemp->pixels)[(y * srcTemp->w) + x] = c[(x + (y % 3)) % 3];
                }
            }
        } else {
            srcTemp = SDL_LoadBMP(srcFile.c_str());
            if (!srcTemp) {
                SDL_Log("ERROR, Couldn't load source image\n");
                return 1;
            }
        }

        // Align src's pixel-bufferon a 16-byte boundary, in case we want to
        // use SSE2 aligned I/O.
        void * buffer = calloc(1, (srcTemp->w * srcTemp->h * 4) + 15);
        void * alignedBuffer = (void *)(((uintptr_t)buffer+15) & ~ 0x0F);
        src = SDL_CreateRGBSurfaceFrom(alignedBuffer, srcTemp->w, srcTemp->h, 32, (srcTemp->w * 4), 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
        if (!src) {
            SDL_Log("ERROR, Couldn't convert source image to wanted pixel-format: %s\n", SDL_GetError());
            return 1;
        }
        SDL_UpperBlit(srcTemp, NULL, src, NULL);

        SDL_FreeSurface(srcTemp);
    }
    
    return DLT_Run(argc, argv, "Src");
}

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
