// DLTest.h
// Public domain, Sep 23 2017, David Ludwig, dludwig@pobox.com. See "unlicense" statement at the end of this file.

#ifndef DLTest_h
#define DLTest_h

#include <functional>
#include <string>
#include <vector>
#include "SDL.h"

#if ! SDL_VERSION_ATLEAST(2,0,0)
#error SDL 2.0 or higher must be used
#endif

#define DLT_ReadPixel32_Raw(PIXELS, PITCH, BYTESPP, X, Y) \
    (*(uint32_t *)((uint8_t *)(PIXELS) + (((Y) * (PITCH)) + ((X) * (BYTESPP)))))

#define DLT_ReadPixel32(SURFACE, X, Y) \
    DLT_ReadPixel32_Raw((SURFACE).pixels, (SURFACE).pitch, 4, (X), (Y))

#define DLT_WritePixel32_Raw(PIXELS, PITCH, BYTESPP, X, Y, COLOR) \
    (*(uint32_t *)((uint8_t *)(PIXELS) + (((Y) * (PITCH)) + ((X) * (BYTESPP))))) = (COLOR)

#define DLT_WritePixel32(SURFACE, X, Y, COLOR) \
    DLT_WritePixel32_Raw((SURFACE).pixels, (SURFACE).pitch, 4, (X), (Y), (COLOR));

#define DLT_SplitARGB32(SRC, A, R, G, B) \
    (A) = (((SRC) >> 24) & 0xff), \
    (R) = (((SRC) >> 16) & 0xff), \
    (G) = (((SRC) >>  8) & 0xff), \
    (B) = (((SRC)      ) & 0xff)

#define DLT_JoinARGB32(A, R, G, B) \
    ((((A) & 0xff) << 24) | \
     (((R) & 0xff) << 16) | \
     (((G) & 0xff) <<  8) | \
     (((B) & 0xff)      ))


struct DLT_Renderer;

struct DLT_Env {
    DLT_Renderer * renderer = NULL;
    SDL_Window * window = NULL;        // the SDL_Window, set on app-init (to a valid, non-NULL SDL_Window *)
    SDL_Surface * dest = NULL;
    SDL_Texture * texture = NULL;
    SDL_Texture * crosshairs = NULL;
    int index = 0;
    
    DLT_Env() = default;
    DLT_Env(const DLT_Env & src) = default;
    DLT_Env(DLT_Renderer * renderer);
};

std::vector<DLT_Env> & DLT_GetEnvs();

struct DLT_RendererMetadata;

struct DLT_Renderer {
    DLT_RendererMetadata * metadata = NULL;
    virtual ~DLT_Renderer();
    virtual void Init(DLT_Env & env);
    const std::string & GetShortName() const;
    virtual void Draw(DLT_Env & env);
};

struct DLT_RendererMetadata {
    std::function<DLT_Renderer *()> factory;
    std::string shortName;
    std::string description;
};

struct DLT_RendererRegistrar {
    DLT_RendererRegistrar(
        std::function<DLT_Renderer *()> factory,
        std::string shortName,
        std::string description = ""
    );
};

#define DLT_RegisterRenderer(CLASS, ...) \
    static const DLT_RendererRegistrar DLT_RendererRegistrar_##CLASS( \
        [] () { return (DLT_Renderer *) new CLASS; }, \
        __VA_ARGS__ \
    );



enum DLT_ComparisonType : Uint8 {
    DLT_COMPARE_A       = (1 << 0),
    DLT_COMPARE_R       = (1 << 1),
    DLT_COMPARE_G       = (1 << 2),
    DLT_COMPARE_B       = (1 << 3),
    DLT_COMPARE_RGB     = (DLT_COMPARE_R | DLT_COMPARE_G | DLT_COMPARE_B),
    DLT_COMPARE_ARGB    = (DLT_COMPARE_A | DLT_COMPARE_R | DLT_COMPARE_G | DLT_COMPARE_B),
};
static DLT_ComparisonType compare = DLT_COMPARE_ARGB;
static int compare_threshold = 0;
struct DLT_Render_Compare : public DLT_Renderer {
    virtual void Init(DLT_Env & env);
    virtual void Draw(DLT_Env & env);
};

std::vector<DLT_RendererMetadata *> & DLT_AllRendererMetadata();
DLT_RendererMetadata * DLT_FindRendererMetadataByName(const std::string & name);
void DLT_AddRenderer(DLT_Renderer * env);
bool DLT_GetOtherEnvs(DLT_Env * self, DLT_Env ** other, int numOther);
int DLT_Run(int argc, char **argv, const char * defaultRendererName);


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

#endif /* DLTest_h */
