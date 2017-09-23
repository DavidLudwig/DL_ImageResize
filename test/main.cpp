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
    virtual std::string GetShortName() { return "SRC"; }
    virtual void Draw(DLT_Env & env) {
        SDL_Rect destRect = {0, 0, src->w, src->h};
        SDL_UpperBlit(src, NULL, env.dest, &destRect);
    }
};

struct Render_Src2X : public DLT_Renderer {
    virtual void Init(DLT_Env & env) {
        env.dest = SDL_CreateRGBSurface(0, src->w * 2, src->h * 2, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
    }
    virtual std::string GetShortName() { return "SRC2X"; }
    virtual void Draw(DLT_Env & env) {
        SDL_Rect destRect = {0, 0, src->w * 2, src->h * 2};
        SDL_BlitScaled(src, NULL, env.dest, &destRect);
    }
};



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
    virtual std::string GetShortName() = 0;
    virtual void Draw(DLT_Env & env) = 0;
};

struct Render_ScaleSDL : public Render_ScalerBase {
    virtual std::string GetShortName() { return "SDL_BlitScaled"; }
    virtual void Draw(DLT_Env & env) {
        SDL_BlitScaled(src, NULL, env.dest, NULL);
    }
};


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
    virtual std::string GetShortName() { return "Bi1"; }
    virtual void Draw(DLT_Env & env) {
        BilinearScale1((uint32_t*)src->pixels, src->w, src->h, (uint32_t*)env.dest->pixels, env.dest->w, env.dest->h);
    }
};

// #define FIXED 16
// typedef int fixed;
// typedef int64_t fixedbig;

    #define FIXED 10
    #define FRAC_BITS 0x3FF
    typedef uint32_t fixed;
    typedef uint32_t fixedbig;

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
    virtual std::string GetShortName() { return "Bi2"; }
    virtual void Draw(DLT_Env & env) {
        BilinearScale2((uint32_t*)src->pixels, src->w, src->h, (uint32_t*)env.dest->pixels, env.dest->w, env.dest->h);
    }
};


extern "C" void BilinearScale3_x86(uint32_t * src, uint32_t srcWidth, uint32_t srcHeight, uint32_t * dest, uint32_t destWidth, uint32_t destHeight);

struct Render_Bilinear3 : public Render_ScalerBase {
    virtual std::string GetShortName() { return "Bi3"; }
    virtual void Draw(DLT_Env & env) {
        BilinearScale3_x86((uint32_t*)src->pixels, src->w, src->h, (uint32_t*)env.dest->pixels, env.dest->w, env.dest->h);
    }
};


extern "C" void BilinearScale4_x86(uint32_t * src, uint32_t srcWidth, uint32_t srcHeight, uint32_t * dest, uint32_t destWidth, uint32_t destHeight);

struct Render_Bilinear4 : public Render_ScalerBase {
    virtual std::string GetShortName() { return "Bi4"; }
    virtual void Draw(DLT_Env & env) {
        BilinearScale4_x86((uint32_t*)src->pixels, src->w, src->h, (uint32_t*)env.dest->pixels, env.dest->w, env.dest->h);
    }
};



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
        } else if (SDL_strcmp(argv[i], "-r") == 0 || SDL_strcmp(argv[i], "--render") == 0) {
            if (++i < argc) {
                if (SDL_strcasecmp(argv[i], "Src") == 0) {
                    DLT_AddRenderer(new Render_Src());
                } else if (SDL_strcasecmp(argv[i], "Compare") == 0) {
                    DLT_AddRenderer(new DLT_Render_Compare());
                } else if (SDL_strcasecmp(argv[i], "Src2X") == 0) {
                    DLT_AddRenderer(new Render_Src2X());
                } else if (SDL_strcasecmp(argv[i], "ScaleSDL") == 0) {
                    DLT_AddRenderer(new Render_ScaleSDL());
                } else if (SDL_strcasecmp(argv[i], "Bi1") == 0) {
                    DLT_AddRenderer(new Render_Bilinear1());
                } else if (SDL_strcasecmp(argv[i], "Bi2") == 0) {
                    DLT_AddRenderer(new Render_Bilinear2());
                } else if (SDL_strcasecmp(argv[i], "Bi3") == 0) {
                    DLT_AddRenderer(new Render_Bilinear3());
                } else if (SDL_strcasecmp(argv[i], "Bi4") == 0) {
                    DLT_AddRenderer(new Render_Bilinear4());
                } else {
                    printf("ERROR: unknown renderer, \"%s\"\n", argv[i]);
                    exit(1);
                }
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

    if (DLT_GetEnvs().empty()) {
        DLT_AddRenderer(new Render_Src());
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
    
    return DLT_Run(argc, argv);
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
