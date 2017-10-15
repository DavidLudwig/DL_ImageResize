//
// DL_ImageResize -- an image resizing library for C/C++
// Written by David Lee Ludwig <dludwig@pobox.com>
//
// This project is dual-licensed under the CC0 (public domain) and zlib licenses.
//
// You may use this code under the terms of either license.
//
// See "COPYING" statement at the end of this file.
//

#include "DLTest.h"
#include <string>
#include <cstring>
#include <unordered_map>
#include "stb_image.h"
#include <errno.h>


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
DLT_RegisterRenderer(Render_Src, "src", "source image, no scaling");

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
        void * alignedBuffer = calloc(1, (ScaleWidth * ScaleHeight * 4)); // + 16);
        //void * alignedBuffer = (void *)(((uintptr_t)buffer+15) & ~ 0x0F);
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
    // WORKS!
    #define FIXED 10
    #define FRAC_BITS 0x3FF
    typedef uint32_t fixed;
    typedef uint32_t fixedbig;
#elif 0
    // ALSO WORKS!
    #define FIXED 10
    // #define FRAC_BITS 0x3FF
    typedef uint16_t fixed;
    typedef uint32_t fixedbig;
#else
    // FAILS, pretty much: chops off right edge
    #define FIXED 6
    // #define FRAC_BITS 0x3FF
    typedef uint16_t fixed;
    typedef uint16_t fixedbig;
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


/*
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
*/


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


void BilinearScale7(uint32_t * src, uint32_t nSrcWidth, uint32_t nSrcHeight, uint32_t * dest, uint32_t nDestWidth, uint32_t nDestHeight) {
    // uint32_t s0, s1, s2, s3;
    __m128i vs0, vs1, vs2, vs3;

    // uint32_t nSrcX, nSrcY, nSrcIndex;
    uint32_t nSrcY;
    __m128i vnSrcX, vnSrcIndex;

    // fixed fx22_10_RatioDestXtoSrcX = (((nSrcWidth - 1)<<FIXED) / nDestWidth);
    __m128i vfx22_10_RatioDestXtoSrcX = _mm_set1_epi32(((nSrcWidth - 1)<<FIXED) / nDestWidth);

    fixed vfx22_10_RatioDestYToSrcY = (((nSrcHeight - 1)<<FIXED) / nDestHeight);
    //fixedbig diffX, diffY;
    // fixedbig fx22_10_DiffX, fx22_10_DiffY;
    fixedbig fx22_10_DiffY;
    __m128i vfx22_10_DiffX;

    // fixedbig fx22_10_Blue, fx22_10_Green, fx22_10_Red;
    __m128i vfx22_10_Blue, vfx22_10_Green, vfx22_10_Red;

    int nDestIndex = 0;
    for (int nDestY = 0; nDestY < nDestHeight; nDestY++) {
        nSrcY = ((fixedbig)vfx22_10_RatioDestYToSrcY * (fixedbig)(nDestY<<FIXED))>>(FIXED*2);
        fx22_10_DiffY = (((fixedbig)vfx22_10_RatioDestYToSrcY * (fixedbig)(nDestY<<FIXED))>>FIXED) - (nSrcY << FIXED);

        for (int nDestX = 0; nDestX < nDestWidth; nDestX += 4) {
            __m128i vnDestX = _mm_setr_epi32(nDestX, nDestX + 1, nDestX + 2, nDestX + 3);

            // nSrcX  =  (fx22_10_RatioDestXtoSrcX * (nDestX<<FIXED)) >> (FIXED*2);
            vnSrcX = _mm_srli_epi32(_mm_mullo_epi32(vfx22_10_RatioDestXtoSrcX, _mm_slli_epi32(vnDestX, FIXED)), FIXED*2);

            // fx22_10_DiffX = ((fx22_10_RatioDestXtoSrcX * (nDestX<<FIXED))>>FIXED) - (nSrcX << FIXED);
            vfx22_10_DiffX = _mm_sub_epi32(
                _mm_srli_epi32(_mm_mullo_epi32(vfx22_10_RatioDestXtoSrcX, _mm_slli_epi32(vnDestX, FIXED)), FIXED),
                _mm_slli_epi32(vnSrcX, FIXED)
            );

            // nSrcIndex = (nSrcY * nSrcWidth + nSrcX);
            vnSrcIndex = _mm_add_epi32(
                _mm_set1_epi32(nSrcY * nSrcWidth),
                vnSrcX
            );

            // s0 = src[nSrcIndex];
            vs0 = _mm_setr_epi32(
                src[_mm_extract_epi32(vnSrcIndex, 0)],
                src[_mm_extract_epi32(vnSrcIndex, 1)],
                src[_mm_extract_epi32(vnSrcIndex, 2)],
                src[_mm_extract_epi32(vnSrcIndex, 3)]
            );

            // s1 = src[nSrcIndex + 1];
            vs1 = _mm_setr_epi32(
                src[_mm_extract_epi32(vnSrcIndex, 0) + 1],
                src[_mm_extract_epi32(vnSrcIndex, 1) + 1],
                src[_mm_extract_epi32(vnSrcIndex, 2) + 1],
                src[_mm_extract_epi32(vnSrcIndex, 3) + 1]
            );

            // s2 = src[nSrcIndex + nSrcWidth];
            vs2 = _mm_setr_epi32(
                src[_mm_extract_epi32(vnSrcIndex, 0) + nSrcWidth],
                src[_mm_extract_epi32(vnSrcIndex, 1) + nSrcWidth],
                src[_mm_extract_epi32(vnSrcIndex, 2) + nSrcWidth],
                src[_mm_extract_epi32(vnSrcIndex, 3) + nSrcWidth]
            );

            // s3 = src[nSrcIndex + nSrcWidth + 1];
            vs3 = _mm_setr_epi32(
                src[_mm_extract_epi32(vnSrcIndex, 0) + nSrcWidth + 1],
                src[_mm_extract_epi32(vnSrcIndex, 1) + nSrcWidth + 1],
                src[_mm_extract_epi32(vnSrcIndex, 2) + nSrcWidth + 1],
                src[_mm_extract_epi32(vnSrcIndex, 3) + nSrcWidth + 1]
            );

            // fixedbig fx22_10_Factor3 = ((             fx22_10_DiffX) * (             fx22_10_DiffY)) >> FIXED;
            __m128i vfx22_10_Factor3 = _mm_srli_epi32(
                _mm_mullo_epi32(
                    vfx22_10_DiffX,
                    _mm_set1_epi32(fx22_10_DiffY)
                ),
                FIXED
            );

            // fixedbig fx22_10_Factor2 = (((1<<FIXED) - fx22_10_DiffX) * (             fx22_10_DiffY)) >> FIXED;
            __m128i vfx22_10_Factor2 = _mm_srli_epi32(
                _mm_mullo_epi32(
                    _mm_sub_epi32(
                        _mm_set1_epi32(1<<FIXED),
                        vfx22_10_DiffX
                    ),
                    _mm_set1_epi32(fx22_10_DiffY)
                ),
                FIXED
            );

            // fixedbig fx22_10_Factor1 = ((             fx22_10_DiffX) * ((1<<FIXED) - fx22_10_DiffY)) >> FIXED;
            __m128i vfx22_10_Factor1 = _mm_srli_epi32(
                _mm_mullo_epi32(
                    vfx22_10_DiffX,
                    _mm_set1_epi32((1<<FIXED) - fx22_10_DiffY)
                ),
                FIXED
            );

            // fixedbig fx22_10_Factor0 = (((1<<FIXED) - fx22_10_DiffX) * ((1<<FIXED) - fx22_10_DiffY)) >> FIXED;
            __m128i vfx22_10_Factor0 = _mm_srli_epi32(
                _mm_mullo_epi32(
                    _mm_sub_epi32(
                        _mm_set1_epi32(1<<FIXED),
                        vfx22_10_DiffX
                    ),
                    _mm_set1_epi32((1<<FIXED) - fx22_10_DiffY)
                ),
                FIXED
            );

            // fx22_10_Blue = ( \
            //     (((s0    ) & 0xff)<<FIXED) * fx22_10_Factor0 +
            //     (((s1    ) & 0xff)<<FIXED) * fx22_10_Factor1 +
            //     (((s2    ) & 0xff)<<FIXED) * fx22_10_Factor2 +
            //     (((s3    ) & 0xff)<<FIXED) * fx22_10_Factor3
            // ) >> FIXED;
            vfx22_10_Blue = _mm_srli_epi32(
                _mm_add_epi32(
                    _mm_add_epi32(
                        _mm_add_epi32(
                            _mm_mullo_epi32(
                                _mm_slli_epi32(
                                    _mm_and_si128(
                                        vs0,
                                        _mm_set1_epi32(0xff)
                                    ),
                                    FIXED
                                ),
                                vfx22_10_Factor0
                            ),
                            _mm_mullo_epi32(
                                _mm_slli_epi32(
                                    _mm_and_si128(
                                        vs1,
                                        _mm_set1_epi32(0xff)
                                    ),
                                    FIXED
                                ),
                                vfx22_10_Factor1
                            )
                        ),
                        _mm_mullo_epi32(
                            _mm_slli_epi32(
                                _mm_and_si128(
                                    vs2,
                                    _mm_set1_epi32(0xff)
                                ),
                                FIXED
                            ),
                            vfx22_10_Factor2
                        )
                    ),
                    _mm_mullo_epi32(
                        _mm_slli_epi32(
                            _mm_and_si128(
                                vs3,
                                _mm_set1_epi32(0xff)
                            ),
                            FIXED
                        ),
                        vfx22_10_Factor3
                    )
                ),
                FIXED
            );

            // fx22_10_Green = ( \
            //     (((s0>> 8) & 0xff)<<FIXED) * fx22_10_Factor0 +
            //     (((s1>> 8) & 0xff)<<FIXED) * fx22_10_Factor1 +
            //     (((s2>> 8) & 0xff)<<FIXED) * fx22_10_Factor2 +
            //     (((s3>> 8) & 0xff)<<FIXED) * fx22_10_Factor3
            // ) >> FIXED;
            vfx22_10_Green = _mm_srli_epi32(
                _mm_add_epi32(
                    _mm_add_epi32(
                        _mm_add_epi32(
                            _mm_mullo_epi32(
                                _mm_slli_epi32(
                                    _mm_and_si128(
                                        _mm_srli_epi32(
                                            vs0,
                                            8
                                        ),
                                        _mm_set1_epi32(0xff)
                                    ),
                                    FIXED
                                ),
                                vfx22_10_Factor0
                            ),
                            _mm_mullo_epi32(
                                _mm_slli_epi32(
                                    _mm_and_si128(
                                        _mm_srli_epi32(
                                            vs1,
                                            8
                                        ),
                                        _mm_set1_epi32(0xff)
                                    ),
                                    FIXED
                                ),
                                vfx22_10_Factor1
                            )
                        ),
                        _mm_mullo_epi32(
                            _mm_slli_epi32(
                                _mm_and_si128(
                                    _mm_srli_epi32(
                                        vs2,
                                        8
                                    ),
                                    _mm_set1_epi32(0xff)
                                ),
                                FIXED
                            ),
                            vfx22_10_Factor2
                        )
                    ),
                    _mm_mullo_epi32(
                        _mm_slli_epi32(
                            _mm_and_si128(
                                _mm_srli_epi32(
                                    vs3,
                                    8
                                ),
                                _mm_set1_epi32(0xff)
                            ),
                            FIXED
                        ),
                        vfx22_10_Factor3
                    )
                ),
                FIXED
            );

            // fx22_10_Red = ( \
            //     (((s0>>16) & 0xff)<<FIXED) * fx22_10_Factor0 +
            //     (((s1>>16) & 0xff)<<FIXED) * fx22_10_Factor1 +
            //     (((s2>>16) & 0xff)<<FIXED) * fx22_10_Factor2 +
            //     (((s3>>16) & 0xff)<<FIXED) * fx22_10_Factor3
            // ) >> FIXED;
            vfx22_10_Red = _mm_srli_epi32(
                _mm_add_epi32(
                    _mm_add_epi32(
                        _mm_add_epi32(
                            _mm_mullo_epi32(
                                _mm_slli_epi32(
                                    _mm_and_si128(
                                        _mm_srli_epi32(
                                            vs0,
                                            16
                                        ),
                                        _mm_set1_epi32(0xff)
                                    ),
                                    FIXED
                                ),
                                vfx22_10_Factor0
                            ),
                            _mm_mullo_epi32(
                                _mm_slli_epi32(
                                    _mm_and_si128(
                                        _mm_srli_epi32(
                                            vs1,
                                            16
                                        ),
                                        _mm_set1_epi32(0xff)
                                    ),
                                    FIXED
                                ),
                                vfx22_10_Factor1
                            )
                        ),
                        _mm_mullo_epi32(
                            _mm_slli_epi32(
                                _mm_and_si128(
                                    _mm_srli_epi32(
                                        vs2,
                                        16
                                    ),
                                    _mm_set1_epi32(0xff)
                                ),
                                FIXED
                            ),
                            vfx22_10_Factor2
                        )
                    ),
                    _mm_mullo_epi32(
                        _mm_slli_epi32(
                            _mm_and_si128(
                                _mm_srli_epi32(
                                    vs3,
                                    16
                                ),
                                _mm_set1_epi32(0xff)
                            ),
                            FIXED
                        ),
                        vfx22_10_Factor3
                    )
                ),
                FIXED
            );

            // dest[nDestIndex++] =
            //     0xff000000 |
            //     (((fx22_10_Red  >>FIXED) << 16) & 0xff0000) |
            //     (((fx22_10_Green>>FIXED) <<  8) &   0xff00) |
            //      ((fx22_10_Blue >>FIXED) <<  0);
            dest[nDestIndex++] =
                0xff000000 |
                (((_mm_extract_epi32(vfx22_10_Red, 0)  >>FIXED) << 16) & 0xff0000) |
                (((_mm_extract_epi32(vfx22_10_Green, 0)>>FIXED) <<  8) &   0xff00) |
                 ((_mm_extract_epi32(vfx22_10_Blue, 0) >>FIXED) <<  0);
            dest[nDestIndex++] =
                0xff000000 |
                (((_mm_extract_epi32(vfx22_10_Red, 1)  >>FIXED) << 16) & 0xff0000) |
                (((_mm_extract_epi32(vfx22_10_Green, 1)>>FIXED) <<  8) &   0xff00) |
                 ((_mm_extract_epi32(vfx22_10_Blue, 1) >>FIXED) <<  0);
            dest[nDestIndex++] =
                0xff000000 |
                (((_mm_extract_epi32(vfx22_10_Red, 2)  >>FIXED) << 16) & 0xff0000) |
                (((_mm_extract_epi32(vfx22_10_Green, 2)>>FIXED) <<  8) &   0xff00) |
                 ((_mm_extract_epi32(vfx22_10_Blue, 2) >>FIXED) <<  0);
            dest[nDestIndex++] =
                0xff000000 |
                (((_mm_extract_epi32(vfx22_10_Red, 3)  >>FIXED) << 16) & 0xff0000) |
                (((_mm_extract_epi32(vfx22_10_Green, 3)>>FIXED) <<  8) &   0xff00) |
                 ((_mm_extract_epi32(vfx22_10_Blue, 3) >>FIXED) <<  0);
        }
    }
}

struct Render_Bilinear7 : public Render_ScalerBase {
    virtual void Draw(DLT_Env & env) {
        BilinearScale7((uint32_t*)src->pixels, src->w, src->h, (uint32_t*)env.dest->pixels, env.dest->w, env.dest->h);
    }
};
DLT_RegisterRenderer(Render_Bilinear7, "Bi7", "Bi2 renderer, with 4 pixels at once via SSE 4.1 intrinsics");



struct vec4u32 {
    __m128i data;

    inline vec4u32() = default;
    inline vec4u32(__m128i src) : data(src) {}
    inline vec4u32(uint32_t scalar) : data(_mm_set1_epi32(scalar)) {}
    inline vec4u32(uint32_t scalar0, uint32_t scalar1, uint32_t scalar2, uint32_t scalar3) : data(_mm_setr_epi32(scalar0,scalar1,scalar2,scalar3)) {}

    inline vec4u32  operator +  (const vec4u32 & b) const { return _mm_add_epi32(data, b.data);   }
    inline vec4u32  operator -  (const vec4u32 & b) const { return _mm_sub_epi32(data, b.data);   }
    inline vec4u32  operator *  (const vec4u32 & b) const { return _mm_mullo_epi32(data, b.data); }
    inline vec4u32  operator &  (uint32_t scalar)   const { return _mm_and_si128(data, _mm_set1_epi32(scalar)); }
    inline vec4u32  operator |  (const vec4u32 & b) const { return _mm_or_si128(data, b.data);    }
    inline vec4u32  operator << (int imm8)          const { return _mm_slli_epi32(data, imm8);    }
    inline vec4u32  operator >> (int imm8)          const { return _mm_srli_epi32(data, imm8);    }
    inline uint32_t getu32      (int imm8)          const { return _mm_extract_epi32(data, imm8); }

    inline void store_si128_aligned  (uint32_t * dest)      const { _mm_store_si128(((__m128i *)(dest)), data); }
    inline void store_si128_unaligned(uint32_t * dest)      const { _mm_storeu_si128(((__m128i *)(dest)), data); }
};

inline vec4u32 operator + (uint32_t scalar, const vec4u32 & b) { return vec4u32(scalar) + b; }
inline vec4u32 operator - (uint32_t scalar, const vec4u32 & b) { return vec4u32(scalar) - b; }

#if 1
// using a macro is slightly faster, in at least some cases

#if 0
// Slower, SSE2-compatible version:
#define vec4u32_loadFromOffsets4x32(dest, src, offsets, extraOffset) { \
    uint32_t __attribute__((aligned(16))) rawOffsets[4]; \
    _mm_store_si128((__m128i *)rawOffsets, offsets.data); \
    dest = { \
        src[rawOffsets[0] + extraOffset], \
        src[rawOffsets[1] + extraOffset], \
        src[rawOffsets[2] + extraOffset], \
        src[rawOffsets[3] + extraOffset], \
    }; \
}
#else
// Faster, SSE4.1 version:
#define vec4u32_loadFromOffsets4x32(dest, src, offsets, extraOffset) \
dest = {                                                        \
    src[_mm_extract_epi32(offsets.data, 0) + extraOffset],      \
    src[_mm_extract_epi32(offsets.data, 1) + extraOffset],      \
    src[_mm_extract_epi32(offsets.data, 2) + extraOffset],      \
    src[_mm_extract_epi32(offsets.data, 3) + extraOffset]       \
}
#endif


#else
inline void vec4u32_loadFromOffsets4x32(vec4u32 & dest, const uint32_t * src, const vec4u32 & offsets, const uint32_t & extraOffset) {
    #if 0
    // Slower, SSE2-compatible version:
    uint32_t __attribute__((aligned(16))) rawOffsets[4];
    _mm_storeu_si128((__m128i *)rawOffsets, offsets.data);
    dest = {
        src[rawOffsets[0] + extraOffset],
        src[rawOffsets[1] + extraOffset],
        src[rawOffsets[2] + extraOffset],
        src[rawOffsets[3] + extraOffset],
    };
    #else
    // Faster, SSE4.1 version:
    dest = {
        src[_mm_extract_epi32(offsets.data, 0) + extraOffset],
        src[_mm_extract_epi32(offsets.data, 1) + extraOffset],
        src[_mm_extract_epi32(offsets.data, 2) + extraOffset],
        src[_mm_extract_epi32(offsets.data, 3) + extraOffset]
    };
    #endif
}
#endif

#if 0
#define vec4u32_store_RGB(dest, nDestIndex, vfx22_10_Red, vfx22_10_Green, vfx22_10_Blue) \
    dest[nDestIndex  ] =                                            \
        0xff000000 |                                                \
        (((vfx22_10_Red.getu32(0)  >>FIXED) << 16) & 0xff0000) |    \
        (((vfx22_10_Green.getu32(0)>>FIXED) <<  8) &   0xff00) |    \
         ((vfx22_10_Blue.getu32(0) >>FIXED) <<  0);                 \
    dest[nDestIndex+1] =                                            \
        0xff000000 |                                                \
        (((vfx22_10_Red.getu32(1)  >>FIXED) << 16) & 0xff0000) |    \
        (((vfx22_10_Green.getu32(1)>>FIXED) <<  8) &   0xff00) |    \
         ((vfx22_10_Blue.getu32(1) >>FIXED) <<  0);                 \
    dest[nDestIndex+2] =                                            \
        0xff000000 |                                                \
        (((vfx22_10_Red.getu32(2)  >>FIXED) << 16) & 0xff0000) |    \
        (((vfx22_10_Green.getu32(2)>>FIXED) <<  8) &   0xff00) |    \
         ((vfx22_10_Blue.getu32(2) >>FIXED) <<  0);                 \
    dest[nDestIndex+3] =                                            \
        0xff000000 |                                                \
        (((vfx22_10_Red.getu32(3)  >>FIXED) << 16) & 0xff0000) |    \
        (((vfx22_10_Green.getu32(3)>>FIXED) <<  8) &   0xff00) |    \
         ((vfx22_10_Blue.getu32(3) >>FIXED) <<  0);
#elif 0
inline void vec4u32_store_RGB(
    uint32_t * dest,
    uint32_t nDestIndex,
    const vec4u32 & vfx22_10_Red,
    const vec4u32 & vfx22_10_Green,
    const vec4u32 & vfx22_10_Blue
) {
    dest[nDestIndex  ] =
        0xff000000 |
        (((vfx22_10_Red.getu32(0)  >>FIXED) << 16) & 0xff0000) |
        (((vfx22_10_Green.getu32(0)>>FIXED) <<  8) &   0xff00) |
         ((vfx22_10_Blue.getu32(0) >>FIXED) <<  0);
    dest[nDestIndex+1] =
        0xff000000 |
        (((vfx22_10_Red.getu32(1)  >>FIXED) << 16) & 0xff0000) |
        (((vfx22_10_Green.getu32(1)>>FIXED) <<  8) &   0xff00) |
         ((vfx22_10_Blue.getu32(1) >>FIXED) <<  0);
    dest[nDestIndex+2] =
        0xff000000 |
        (((vfx22_10_Red.getu32(2)  >>FIXED) << 16) & 0xff0000) |
        (((vfx22_10_Green.getu32(2)>>FIXED) <<  8) &   0xff00) |
         ((vfx22_10_Blue.getu32(2) >>FIXED) <<  0);
    dest[nDestIndex+3] =
        0xff000000 |
        (((vfx22_10_Red.getu32(3)  >>FIXED) << 16) & 0xff0000) |
        (((vfx22_10_Green.getu32(3)>>FIXED) <<  8) &   0xff00) |
         ((vfx22_10_Blue.getu32(3) >>FIXED) <<  0);
}
#else
inline void vec4u32_store_RGB(
    uint32_t * dest,
    uint32_t nDestIndex,
    const vec4u32 & vfx22_10_Red,
    const vec4u32 & vfx22_10_Green,
    const vec4u32 & vfx22_10_Blue
) {

#if FIXED == 10
    vec4u32 final = \
        vec4u32(0xff000000) |
        ((vfx22_10_Red   <<  6) & 0x00ff0000) |
        ((vfx22_10_Green >>  2) & 0x0000ff00) |
        ((vfx22_10_Blue  >> 10) & 0x000000ff);
#else
    vec4u32 final = \
        vec4u32(0xff000000) |
        (((vfx22_10_Red   >> FIXED) << 16) & 0x00ff0000) |
        (((vfx22_10_Green >> FIXED) <<  8) & 0x0000ff00) |
        (((vfx22_10_Blue  >> FIXED)      ) & 0x000000ff);
#endif

    // vec4u32 final = \
    //     vec4u32(0xff000000) |
    //     (vfx22_10_Red   << 14 >> 24 << 16) |
    //     (vfx22_10_Green << 14 >> 24 << 8) |
    //     (vfx22_10_Blue  << 14 >> 24);

    final.store_si128_unaligned(dest + nDestIndex);
    // final.store_si128_aligned(dest + nDestIndex);



    // dest[nDestIndex  ] =
    //     0xff000000 |
    //     (((vfx22_10_Red.getu32(0)  >>FIXED) << 16) & 0xff0000) |
    //     (((vfx22_10_Green.getu32(0)>>FIXED) <<  8) &   0xff00) |
    //      ((vfx22_10_Blue.getu32(0) >>FIXED) <<  0);
    // dest[nDestIndex+1] =
    //     0xff000000 |
    //     (((vfx22_10_Red.getu32(1)  >>FIXED) << 16) & 0xff0000) |
    //     (((vfx22_10_Green.getu32(1)>>FIXED) <<  8) &   0xff00) |
    //      ((vfx22_10_Blue.getu32(1) >>FIXED) <<  0);
    // dest[nDestIndex+2] =
    //     0xff000000 |
    //     (((vfx22_10_Red.getu32(2)  >>FIXED) << 16) & 0xff0000) |
    //     (((vfx22_10_Green.getu32(2)>>FIXED) <<  8) &   0xff00) |
    //      ((vfx22_10_Blue.getu32(2) >>FIXED) <<  0);
    // dest[nDestIndex+3] =
    //     0xff000000 |
    //     (((vfx22_10_Red.getu32(3)  >>FIXED) << 16) & 0xff0000) |
    //     (((vfx22_10_Green.getu32(3)>>FIXED) <<  8) &   0xff00) |
    //      ((vfx22_10_Blue.getu32(3) >>FIXED) <<  0);
}
#endif

void BilinearScale8(uint32_t * src, uint32_t nSrcWidth, uint32_t nSrcHeight, uint32_t * dest, uint32_t nDestWidth, uint32_t nDestHeight) {
    vec4u32  vs0, vs1, vs2, vs3;
    uint32_t nSrcY;

    vec4u32  vnSrcX, vnSrcIndex;
    vec4u32  vfx22_10_RatioDestXtoSrcX = ((nSrcWidth - 1)<<FIXED) / nDestWidth;
    fixed    vfx22_10_RatioDestYToSrcY = (((nSrcHeight - 1)<<FIXED) / nDestHeight);
    fixedbig fx22_10_DiffY;
    vec4u32  vfx22_10_DiffX;
    vec4u32  vfx22_10_Blue, vfx22_10_Green, vfx22_10_Red;

    uint32_t nDestIndex = 0;
    for (uint32_t nDestY = 0; nDestY < nDestHeight; nDestY++) {
        nSrcY = ((fixedbig)vfx22_10_RatioDestYToSrcY * (fixedbig)(nDestY<<FIXED))>>(FIXED*2);
        fx22_10_DiffY = (((fixedbig)vfx22_10_RatioDestYToSrcY * (fixedbig)(nDestY<<FIXED))>>FIXED) - (nSrcY << FIXED);

        for (uint32_t nDestX = 0; nDestX < nDestWidth; nDestX += 4) {
            vec4u32 vnDestX = vec4u32(nDestX, nDestX + 1, nDestX + 2, nDestX + 3);
            vnSrcX  =  (vfx22_10_RatioDestXtoSrcX * (vnDestX<<FIXED)) >> (FIXED*2);
            vfx22_10_DiffX = ((vfx22_10_RatioDestXtoSrcX * (vnDestX<<FIXED))>>FIXED) - (vnSrcX << FIXED);

            vnSrcIndex = (nSrcY * nSrcWidth + vnSrcX);

            vec4u32_loadFromOffsets4x32(vs0, src, vnSrcIndex, 0);
            vec4u32_loadFromOffsets4x32(vs1, src, vnSrcIndex, 1);
            vec4u32_loadFromOffsets4x32(vs2, src, vnSrcIndex, nSrcWidth);
            vec4u32_loadFromOffsets4x32(vs3, src, vnSrcIndex, nSrcWidth + 1);

            vec4u32 vfx22_10_Factor3 = ((             vfx22_10_DiffX) * (             fx22_10_DiffY)) >> FIXED;
            vec4u32 vfx22_10_Factor2 = (((1<<FIXED) - vfx22_10_DiffX) * (             fx22_10_DiffY)) >> FIXED;
            vec4u32 vfx22_10_Factor1 = ((             vfx22_10_DiffX) * ((1<<FIXED) - fx22_10_DiffY)) >> FIXED;
            vec4u32 vfx22_10_Factor0 = (((1<<FIXED) - vfx22_10_DiffX) * ((1<<FIXED) - fx22_10_DiffY)) >> FIXED;

            vfx22_10_Blue = ( \
                (((vs0    ) & 0xff)<<FIXED) * vfx22_10_Factor0 +
                (((vs1    ) & 0xff)<<FIXED) * vfx22_10_Factor1 +
                (((vs2    ) & 0xff)<<FIXED) * vfx22_10_Factor2 +
                (((vs3    ) & 0xff)<<FIXED) * vfx22_10_Factor3
            ) >> FIXED;

            vfx22_10_Green = ( \
                (((vs0>> 8) & 0xff)<<FIXED) * vfx22_10_Factor0 +
                (((vs1>> 8) & 0xff)<<FIXED) * vfx22_10_Factor1 +
                (((vs2>> 8) & 0xff)<<FIXED) * vfx22_10_Factor2 +
                (((vs3>> 8) & 0xff)<<FIXED) * vfx22_10_Factor3
            ) >> FIXED;

            vfx22_10_Red = ( \
                (((vs0>>16) & 0xff)<<FIXED) * vfx22_10_Factor0 +
                (((vs1>>16) & 0xff)<<FIXED) * vfx22_10_Factor1 +
                (((vs2>>16) & 0xff)<<FIXED) * vfx22_10_Factor2 +
                (((vs3>>16) & 0xff)<<FIXED) * vfx22_10_Factor3
            ) >> FIXED;

            vec4u32_store_RGB(dest, nDestIndex, vfx22_10_Red, vfx22_10_Green, vfx22_10_Blue);
            nDestIndex += 4;
        }
    }
}

struct Render_Bilinear8 : public Render_ScalerBase {
    virtual void Draw(DLT_Env & env) {
        BilinearScale8((uint32_t*)src->pixels, src->w, src->h, (uint32_t*)env.dest->pixels, env.dest->w, env.dest->h);
    }
};
DLT_RegisterRenderer(Render_Bilinear8, "Bi8", "Bi7 renderer, with intrinsics refactored into a class, vec4u32");


void BilinearScale9(uint32_t * src, int srcWidth, int srcHeight, uint32_t * dest, int destWidth, int destHeight) {
    uint32_t s0, s1, s2, s3;
    int srcX, srcY, srcIndex;

    fixed ratioDestXtoSrcX = (((srcWidth - 1)<<FIXED) / destWidth);
    fixed ratioDestYToSrcY = (((srcHeight - 1)<<FIXED) / destHeight);
    fixedbig fx22_10_DiffX, fx22_10_DiffY;
    fixedbig blue, green, red;
    
    int destIndex = 0;
    for (int destY = 0; destY < destHeight; destY++) {
        srcY = ((fixedbig)ratioDestYToSrcY * (fixedbig)(destY<<FIXED))>>(FIXED*2);
        fx22_10_DiffY = (((fixedbig)ratioDestYToSrcY * (fixedbig)(destY<<FIXED))>>FIXED) - (srcY << FIXED);

        for (int destX = 0; destX < destWidth; destX++) {
            srcX  =  ((fixedbig)ratioDestXtoSrcX * (fixedbig)(destX<<FIXED))>>(FIXED*2);
            fx22_10_DiffX = (((fixedbig)ratioDestXtoSrcX * (fixedbig)(destX<<FIXED))>>FIXED) - (srcX << FIXED);

            srcIndex = (srcY * srcWidth + srcX);
            s0 = src[srcIndex];
            s1 = src[srcIndex + 1];
            s2 = src[srcIndex + srcWidth];
            s3 = src[srcIndex + srcWidth + 1];

            fixedbig fx22_10_Factor3 = ((             fx22_10_DiffX) * (             fx22_10_DiffY)) >> FIXED;
            fixedbig fx22_10_Factor2 = (((1<<FIXED) - fx22_10_DiffX) * (             fx22_10_DiffY)) >> FIXED;
            fixedbig fx22_10_Factor1 = ((             fx22_10_DiffX) * ((1<<FIXED) - fx22_10_DiffY)) >> FIXED;
            fixedbig fx22_10_Factor0 = (((1<<FIXED) - fx22_10_DiffX) * ((1<<FIXED) - fx22_10_DiffY)) >> FIXED;

            blue = ( \
                (((s0    ) & 0xff)) * fx22_10_Factor0 +
                (((s1    ) & 0xff)) * fx22_10_Factor1 +
                (((s2    ) & 0xff)) * fx22_10_Factor2 +
                (((s3    ) & 0xff)) * fx22_10_Factor3
            );
            
            green = ( \
                (((s0>> 8) & 0xff)) * fx22_10_Factor0 +
                (((s1>> 8) & 0xff)) * fx22_10_Factor1 +
                (((s2>> 8) & 0xff)) * fx22_10_Factor2 +
                (((s3>> 8) & 0xff)) * fx22_10_Factor3
            );
            
            red = ( \
                (((s0>>16) & 0xff)) * fx22_10_Factor0 +
                (((s1>>16) & 0xff)) * fx22_10_Factor1 +
                (((s2>>16) & 0xff)) * fx22_10_Factor2 +
                (((s3>>16) & 0xff)) * fx22_10_Factor3
            );
            
            dest[destIndex++] =
                0xff000000 |
                (((red  >>FIXED) << 16) & 0xff0000) |
                (((green>>FIXED) <<  8) &   0xff00) |
                 ((blue >>FIXED) <<  0);
        }
    }
}

struct Render_Bilinear9 : public Render_ScalerBase {
    virtual void Draw(DLT_Env & env) {
        BilinearScale9((uint32_t*)src->pixels, src->w, src->h, (uint32_t*)env.dest->pixels, env.dest->w, env.dest->h);
    }
};
DLT_RegisterRenderer(Render_Bilinear9, "Bi9", "Bi8 renderer, with less shifts");


void BilinearScale10(uint32_t * src, int nSrcWidth, int nSrcHeight, uint32_t * dest, int nDestWidth, int nDestHeight) {
    vec4u32  vs0, vs1, vs2, vs3;
    uint32_t nSrcY;

    vec4u32  vnSrcX, vnSrcIndex;
    vec4u32  vfx22_10_RatioDestXtoSrcX = ((nSrcWidth - 1)<<FIXED) / nDestWidth;
    fixed    vfx22_10_RatioDestYToSrcY = (((nSrcHeight - 1)<<FIXED) / nDestHeight);
    fixedbig fx22_10_DiffY;
    vec4u32  vfx22_10_DiffX;
    vec4u32  vfx22_10_Blue, vfx22_10_Green, vfx22_10_Red;

    uint32_t nDestIndex = 0;
    for (uint32_t nDestY = 0; nDestY < nDestHeight; nDestY++) {
        nSrcY = ((fixedbig)vfx22_10_RatioDestYToSrcY * (fixedbig)(nDestY<<FIXED))>>(FIXED*2);
        fx22_10_DiffY = (((fixedbig)vfx22_10_RatioDestYToSrcY * (fixedbig)(nDestY<<FIXED))>>FIXED) - (nSrcY << FIXED);

        for (uint32_t nDestX = 0; nDestX < nDestWidth; nDestX += 4) {
            vec4u32 vnDestX = vec4u32(nDestX, nDestX + 1, nDestX + 2, nDestX + 3);
            vnSrcX  =  (vfx22_10_RatioDestXtoSrcX * (vnDestX<<FIXED)) >> (FIXED*2);
            vfx22_10_DiffX = ((vfx22_10_RatioDestXtoSrcX * (vnDestX<<FIXED))>>FIXED) - (vnSrcX << FIXED);

            vnSrcIndex = (nSrcY * nSrcWidth + vnSrcX);

            vec4u32_loadFromOffsets4x32(vs0, src, vnSrcIndex, 0);
            vec4u32_loadFromOffsets4x32(vs1, src, vnSrcIndex, 1);
            vec4u32_loadFromOffsets4x32(vs2, src, vnSrcIndex, nSrcWidth);
            vec4u32_loadFromOffsets4x32(vs3, src, vnSrcIndex, nSrcWidth + 1);

            vec4u32 vfx22_10_Factor3 = ((             vfx22_10_DiffX) * (             fx22_10_DiffY)) >> FIXED;
            vec4u32 vfx22_10_Factor2 = (((1<<FIXED) - vfx22_10_DiffX) * (             fx22_10_DiffY)) >> FIXED;
            vec4u32 vfx22_10_Factor1 = ((             vfx22_10_DiffX) * ((1<<FIXED) - fx22_10_DiffY)) >> FIXED;
            vec4u32 vfx22_10_Factor0 = (((1<<FIXED) - vfx22_10_DiffX) * ((1<<FIXED) - fx22_10_DiffY)) >> FIXED;

            vfx22_10_Blue = ( \
                (((vs0    ) & 0xff)) * vfx22_10_Factor0 +
                (((vs1    ) & 0xff)) * vfx22_10_Factor1 +
                (((vs2    ) & 0xff)) * vfx22_10_Factor2 +
                (((vs3    ) & 0xff)) * vfx22_10_Factor3
            );

            vfx22_10_Green = ( \
                (((vs0>> 8) & 0xff)) * vfx22_10_Factor0 +
                (((vs1>> 8) & 0xff)) * vfx22_10_Factor1 +
                (((vs2>> 8) & 0xff)) * vfx22_10_Factor2 +
                (((vs3>> 8) & 0xff)) * vfx22_10_Factor3
            );

            vfx22_10_Red = ( \
                (((vs0>>16) & 0xff)) * vfx22_10_Factor0 +
                (((vs1>>16) & 0xff)) * vfx22_10_Factor1 +
                (((vs2>>16) & 0xff)) * vfx22_10_Factor2 +
                (((vs3>>16) & 0xff)) * vfx22_10_Factor3
            );
            
            vec4u32_store_RGB(dest, nDestIndex, vfx22_10_Red, vfx22_10_Green, vfx22_10_Blue);
            nDestIndex += 4;
        }
    }
}

struct Render_Bilinear10 : public Render_ScalerBase {
    virtual void Draw(DLT_Env & env) {
        BilinearScale10((uint32_t*)src->pixels, src->w, src->h, (uint32_t*)env.dest->pixels, env.dest->w, env.dest->h);
    }
};
DLT_RegisterRenderer(Render_Bilinear10, "Bi10", "Bi9 renderer, 4 pixels at a time via SSE4.1 intrinsics");



void BilinearScale11(uint32_t * src, int nSrcWidth, int nSrcHeight, uint32_t * dest, int nDestWidth, int nDestHeight) {
    vec4u32  vs0, vs1, vs2, vs3;
    uint32_t nSrcY;

    vec4u32  vnSrcX, vnSrcIndex;
    vec4u32  vfx22_10_RatioDestXtoSrcX = ((nSrcWidth - 1)<<FIXED) / nDestWidth;
    fixed    fx22_10_RatioDestYToSrcY = (((nSrcHeight - 1)<<FIXED) / nDestHeight);
    fixedbig fx22_10_DiffY;
    vec4u32  vfx22_10_DiffX;
    vec4u32  vfx22_10_Blue, vfx22_10_Green, vfx22_10_Red;

    uint32_t nDestIndex = 0;
    for (uint32_t nDestY = 0; nDestY < nDestHeight; nDestY++) {
#if 1
        nSrcY = ((fixedbig)fx22_10_RatioDestYToSrcY * (fixedbig)(nDestY<<FIXED))>>(FIXED*2);
        fx22_10_DiffY = (((fixedbig)fx22_10_RatioDestYToSrcY * (fixedbig)(nDestY<<FIXED))>>FIXED) - (nSrcY << FIXED);
#else
        nSrcY = ((fixedbig)fx22_10_RatioDestYToSrcY * nDestY)>>FIXED;
        fx22_10_DiffY = ((fixedbig)fx22_10_RatioDestYToSrcY * (fixedbig)nDestY) - (nSrcY << FIXED);
#endif

        for (uint32_t nDestX = 0; nDestX < nDestWidth; nDestX += 4) {
            vec4u32 vnDestX = vec4u32(nDestX, nDestX + 1, nDestX + 2, nDestX + 3);
            vnSrcX  =  (vfx22_10_RatioDestXtoSrcX * vnDestX) >> FIXED;
            vfx22_10_DiffX = (vfx22_10_RatioDestXtoSrcX * vnDestX) - (vnSrcX << FIXED);

            vnSrcIndex = (nSrcY * nSrcWidth + vnSrcX);

            vec4u32_loadFromOffsets4x32(vs0, src, vnSrcIndex, 0);
            vec4u32_loadFromOffsets4x32(vs1, src, vnSrcIndex, 1);
            vec4u32_loadFromOffsets4x32(vs2, src, vnSrcIndex, nSrcWidth);
            vec4u32_loadFromOffsets4x32(vs3, src, vnSrcIndex, nSrcWidth + 1);

            const vec4u32 vfx22_10_one = (1<<FIXED);
            vec4u32 vfx22_10_Factor3 = ((               vfx22_10_DiffX) * (               fx22_10_DiffY)) >> FIXED;
            vec4u32 vfx22_10_Factor2 = ((vfx22_10_one - vfx22_10_DiffX) * (               fx22_10_DiffY)) >> FIXED;
            vec4u32 vfx22_10_Factor1 = ((               vfx22_10_DiffX) * (vfx22_10_one - fx22_10_DiffY)) >> FIXED;
            vec4u32 vfx22_10_Factor0 = ((vfx22_10_one - vfx22_10_DiffX) * (vfx22_10_one - fx22_10_DiffY)) >> FIXED;

            vfx22_10_Blue = ( \
                (((vs0    ) & 0xff)) * vfx22_10_Factor0 +
                (((vs1    ) & 0xff)) * vfx22_10_Factor1 +
                (((vs2    ) & 0xff)) * vfx22_10_Factor2 +
                (((vs3    ) & 0xff)) * vfx22_10_Factor3
            );

            vfx22_10_Green = ( \
                (((vs0>> 8) & 0xff)) * vfx22_10_Factor0 +
                (((vs1>> 8) & 0xff)) * vfx22_10_Factor1 +
                (((vs2>> 8) & 0xff)) * vfx22_10_Factor2 +
                (((vs3>> 8) & 0xff)) * vfx22_10_Factor3
            );

            vfx22_10_Red = ( \
                (((vs0>>16) & 0xff)) * vfx22_10_Factor0 +
                (((vs1>>16) & 0xff)) * vfx22_10_Factor1 +
                (((vs2>>16) & 0xff)) * vfx22_10_Factor2 +
                (((vs3>>16) & 0xff)) * vfx22_10_Factor3
            );
            
            vec4u32_store_RGB(dest, nDestIndex, vfx22_10_Red, vfx22_10_Green, vfx22_10_Blue);
            nDestIndex += 4;
        }
    }
}

struct Render_Bilinear11 : public Render_ScalerBase {
    virtual void Draw(DLT_Env & env) {
        BilinearScale11((uint32_t*)src->pixels, src->w, src->h, (uint32_t*)env.dest->pixels, env.dest->w, env.dest->h);
    }
};
DLT_RegisterRenderer(Render_Bilinear11, "Bi11", "Bi10 renderer, with optimizations");


#include "DL_ImageResize.h"

struct Render_BilinearDLIR : public Render_ScalerBase {
    virtual void Draw(DLT_Env & env) {
        SDL_FillRect(env.dest, NULL, 0xffff0000);
        DLIR_ResizeBilinear_ARGB8888(
            src->pixels,
            0,
            0,
            src->w,
            src->h,
            src->pitch,

            env.dest->pixels,
            0,
            0,
            env.dest->w,
            env.dest->h,
            env.dest->pitch,

            0, //165,
            0, //165,
            src->w, // - 165, // - 32,
            src->h  // - 165 // - 2
        );
    }
};
DLT_RegisterRenderer(Render_BilinearDLIR, "dlir", "DLIR default renderer");




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
        } else if (SDL_strcmp(argv[i], "-d") == 0 || SDL_strcmp(argv[i], "--destscale") == 0) {
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
            FILE * srcFilePtr = fopen(srcFile.c_str(), "r");
            if (!srcFilePtr) {
                printf("ERROR: Unable to open srcFile, reason:\"%s\"\n", strerror(errno));
                exit(1);
            }
            int w = 0;
            int h = 0;
            stbi_uc * result = stbi_load_from_file(srcFilePtr, &w, &h, NULL, 4);
            if (!result) {
                printf("ERROR: Unable to decode opened srcFile, reason:\"%s\"\n", stbi_failure_reason());
                exit(1);
            }
            // Convert from ABGR8888 to ARGB8888
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    const size_t offset = ((y * w) + x) * 4;
                    uint8_t r = result[offset + 0];
                    uint8_t g = result[offset + 1];
                    uint8_t b = result[offset + 2];
                    uint8_t a = result[offset + 3];
                    result[offset + 0] = b;
                    result[offset + 1] = g;
                    result[offset + 2] = r;
                    result[offset + 3] = a;
                }
            }
            // Create a corresponding SDL_Surface
            srcTemp = SDL_CreateRGBSurfaceFrom((void *)result, 330, 330, 32, w * 4, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
        }

        // Align src's pixel-bufferon a 16-byte boundary, in case we want to
        // use SSE2 aligned I/O.
//        void * buffer = calloc(1, (srcTemp->w * srcTemp->h * 4) + 15);
//        memset(buffer, 0x77, (srcTemp->w * srcTemp->h * 4) + 15);
//        void * alignedBuffer = (void *)(((uintptr_t)buffer+15) & ~ 0x0F);
//        src = SDL_CreateRGBSurfaceFrom(alignedBuffer, srcTemp->w, srcTemp->h, 32, (srcTemp->w * 4), 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
        void * buffer = calloc(1, (srcTemp->w * srcTemp->h * 4));
        src = SDL_CreateRGBSurfaceFrom(buffer, srcTemp->w, srcTemp->h, 32, (srcTemp->w * 4), 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
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
// COPYING
//
// This project is dual-licensed under the CC0 (public domain) and zlib licenses.
// 
// You may use this code under the terms of either license.
// 
// ---------------------------------------------------------------------------
// CC0 License:
// 
// DL_ImageResize -- an image resizing library for C/C++
// 
// Written in 2017 by David Lee Ludwig <dludwig@pobox.com>
// 
// To the extent possible under law, the author(s) have dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. This software is distributed without any warranty.
// 
// You should have received a copy of the CC0 Public Domain Dedication along with
// this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
// 
// ---------------------------------------------------------------------------
// zlib License:
// 
// DL_ImageResize -- an image resizing library for C/C++
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
// ---------------------------------------------------------------------------
// 
