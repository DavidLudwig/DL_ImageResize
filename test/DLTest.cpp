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

#include <cstdarg>
#include <unordered_map>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Disable deprecated C function warnings from MSVC:
//#define _CRT_SECURE_NO_WARNINGS

static std::vector<DLT_Env> envs;


DLT_Env::DLT_Env(DLT_Renderer * _r) :
    renderer(_r)
{
}

DLT_Renderer::~DLT_Renderer() {
}

void DLT_Renderer::Init(DLT_Env & env) {
}

void DLT_Renderer::Draw(DLT_Env & env) {
}

const std::string & DLT_Renderer::GetShortName() const {
    static const std::string emptyString;
    if (metadata) {
        return metadata->shortName;
    } else {
        return emptyString;
    }
}


std::vector<DLT_RendererMetadata *> & DLT_AllRendererMetadata() {
    static std::vector<DLT_RendererMetadata *> value;
    return value;
}

DLT_RendererRegistrar::DLT_RendererRegistrar(
    std::function<DLT_Renderer *()> factory,
    std::string shortName,
    std::string description
) {
    DLT_RendererMetadata * metadata = new DLT_RendererMetadata({
        factory,
        std::move(shortName),
        std::move(description)
    });
    DLT_AllRendererMetadata().push_back(metadata);
}

DLT_RendererMetadata * DLT_FindRendererMetadataByName(const std::string & name) {
    for (DLT_RendererMetadata * metadata : DLT_AllRendererMetadata()) {
        if (SDL_strcasecmp(metadata->shortName.c_str(), name.c_str()) == 0) {
            return metadata;
        }
    }
    return NULL;
}


bool DLT_GetOtherEnvs(DLT_Env * self, DLT_Env ** other, int numOther) {
    int otherIndex = numOther - 1;
    for (int i = self->index; i >= 0; --i) {
        if (otherIndex < 0) {
            break;
        }
        if (&envs[i] != self) {
            other[otherIndex] = &envs[i];
            otherIndex--;
        }
    }

    return (otherIndex < 0);
}

DLT_RegisterRenderer(DLT_Render_Compare, "CMP", "finds differences between two images");

void DLT_Render_Compare::Init(DLT_Env & env) {
    DLT_Env * compareMe[2] = {NULL, NULL};
    int w = 0;
    int h = 0;
    if (DLT_GetOtherEnvs(&env, (DLT_Env **)&compareMe, 2)) {
        for (int i = 0; i < SDL_arraysize(compareMe); ++i) {
            if (envs[i].dest) {
                w = std::max(w, envs[i].dest->w);
                h = std::max(h, envs[i].dest->h);
            }
        }
    }
    if (w > 0 && h > 0) {
        env.dest = SDL_CreateRGBSurface(0, w, h, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
    }
}


bool DLT_CompareSurfaces(
    const SDL_Surface * srcA,
    const SDL_Surface * srcB,
    SDL_Surface * dest,
    DLT_ComparisonType compare_type,
    int compare_threshold
) {
    bool all_pixels_are_in_threshold = true;
    for (int y = 0; y < dest->h; ++y) {
        for (int x = 0; x < dest->w; ++x) {
            Uint32 dc = 0xffff0000;
            if (srcA && srcB) {
                if (x >= srcA->w || y >= srcA->h || y >= srcB->w || y >= srcB->h) {
                    dc = 0xffffffff;
                } else {
                    Uint32 ac = DLT_ReadPixel32_Raw(srcA->pixels, srcA->pitch, 4, x, y);
                    int aa, ar, ag, ab;
                    DLT_SplitARGB32(ac, aa, ar, ag, ab);
                    
                    Uint32 bc = DLT_ReadPixel32_Raw(srcB->pixels, srcB->pitch, 4, x, y);
                    int ba, br, bg, bb;
                    DLT_SplitARGB32(bc, ba, br, bg, bb);
                    
                    int beyond = 0;
                    if (compare_type & DLT_COMPARE_A) {
                        beyond |= abs(aa - ba) > compare_threshold;
                    }
                    if (compare_type & DLT_COMPARE_R) {
                        beyond |= abs(ar - br) > compare_threshold;
                    }
                    if (compare_type & DLT_COMPARE_G) {
                        beyond |= abs(ag - bg) > compare_threshold;
                    }
                    if (compare_type & DLT_COMPARE_B) {
                        beyond |= abs(ab - bb) > compare_threshold;
                    }
                    
                    dc = beyond ? 0xffffffff : 0x00000000;
                    if (beyond) {
                        all_pixels_are_in_threshold = false;
                    }
                }
            }
            if (dest) {
                DLT_WritePixel32_Raw(dest->pixels, dest->pitch, 4, x, y, dc);
            }
        }
    }
    return all_pixels_are_in_threshold;
}

void DLT_Render_Compare::Draw(DLT_Env & env) {
    //std::vector<DLT_Env> & envs = DLT_GetEnvs();
    
    // Draw diff between scenes 0 and 1
    DLT_Env * compareMe[2] = {NULL, NULL};
    if ( ! DLT_GetOtherEnvs(&env, (DLT_Env **)&compareMe, 2)) {
        return;
    }

    SDL_Surface * A = compareMe[0]->dest;
    SDL_Surface * B = compareMe[1]->dest;
    this->last_compare_result = DLT_CompareSurfaces(A, B, env.dest, this->compare_type, this->compare_threshold);
}



#include <stdio.h>
#include <limits>
#include <algorithm>
#include <string>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifndef _MSC_VER
#include <signal.h>     // For Ctrl+C handling
#endif



static int winSpacingW = 10;
static int winSpacingH = 60;

std::vector<DLT_Env> & DLT_GetEnvs() {
    return envs;
}

static int numTicksToQuit = -1;
static int numTicksBetweenPerfMeasurements = -1;
static int numTicksToNextMeasurement = 0;
static Uint64 initialMeasurement = 0;
static Uint64 lastMeasurement = 0;
static uint64_t totalTicks = 0;
static int lockedX = -1;
static int lockedY = -1;
#define DLT_IS_LOCKED() (lockedX >= 0 && lockedY >= 0)
bool cmpExit = false;


static void DLT_UpdateWindowTitles()
{
    int curX, curY;
    if (DLT_IS_LOCKED()) {
        curX = lockedX;
        curY = lockedY;
    } else {
        if (SDL_GetMouseFocus()) {
            SDL_GetMouseState(&curX, &curY);
        } else {
            curX = curY = -1;
        }
    }

    char window_title[128];
    for (int i = 0; i < (int)envs.size(); ++i) {
        SDL_Surface * bg = envs[i].dest;
        Uint32 c = ( ! (bg && curX >= 0 && curX < bg->w && curY < bg->h)) ? 0 : DLT_ReadPixel32_Raw(bg->pixels, bg->pitch, 4, curX, curY);
        Uint8 a, r, g, b;
        DLT_SplitARGB32(c, a, r, g, b);

        const std::string typeName = envs[i].renderer->GetShortName();
        const char * lockedText = DLT_IS_LOCKED() ? ",lock" : "";

        if (dynamic_cast<DLT_Render_Compare *>(envs[i].renderer)) {
            DLT_Render_Compare * cmp = (DLT_Render_Compare *) envs[i].renderer;
            const char * compare_name = "";
            switch (cmp->compare_type) {
                case DLT_COMPARE_ARGB: {
                    compare_name = "argb";
                } break;
                case DLT_COMPARE_A: {
                    compare_name = "a";
                } break;
                case DLT_COMPARE_R: {
                    compare_name = "r";
                } break;
                case DLT_COMPARE_G: {
                    compare_name = "g";
                } break;
                case DLT_COMPARE_B: {
                    compare_name = "b";
                } break;
                case DLT_COMPARE_RGB: {
                    compare_name = "rgb";
                } break;
            }
            SDL_snprintf(window_title, SDL_arraysize(window_title), "%s %s +/-:%d p:%d,%d%s a,rgb:%02x,%02x%02x%02x",
                typeName.c_str(), compare_name, cmp->compare_threshold, curX, curY, lockedText,
                a, r, g, b);
        } else {
            SDL_snprintf(window_title, SDL_arraysize(window_title), "%s p:(%d,%d)%s a,rgb:0x%02x,0x%02x%02x%02x",
                typeName.c_str(), curX, curY, lockedText,
                a, r, g, b);
        }
        window_title[SDL_arraysize(window_title)-1] = '\0';
        SDL_SetWindowTitle(envs[i].window, window_title);
    }
}

void DLT_Test_ProcessEvent(const SDL_Event & e)
{
    switch (e.type) {
        case SDL_MOUSEMOTION: {
            DLT_UpdateWindowTitles();
        } break;
        
        case SDL_MOUSEBUTTONDOWN: {
            if (DLT_IS_LOCKED()) {
                lockedX = -1;
                lockedY = -1;
            } else {
                lockedX = e.button.x;
                lockedY = e.button.y;
            }
            DLT_UpdateWindowTitles();
        } break;

        case SDL_KEYDOWN: {
            if (e.key.keysym.mod & KMOD_CTRL) {
                switch (e.key.keysym.sym) {
                    case SDLK_c: {
                        exit(0);
                    } break;
                }
            }

            switch (e.key.keysym.sym) {
                case SDLK_q:
                case SDLK_ESCAPE: {
                    exit(0);
                } break;

                case SDLK_RIGHT: if (DLT_IS_LOCKED()) { lockedX = SDL_min(lockedX + 1, envs[0].dest->w - 1); DLT_UpdateWindowTitles(); } break;
                case SDLK_LEFT:  if (DLT_IS_LOCKED()) { lockedX = SDL_max(lockedX - 1,                   0); DLT_UpdateWindowTitles(); } break;
                case SDLK_DOWN:  if (DLT_IS_LOCKED()) { lockedY = SDL_min(lockedY + 1, envs[0].dest->h - 1); DLT_UpdateWindowTitles(); } break;
                case SDLK_UP:    if (DLT_IS_LOCKED()) { lockedY = SDL_max(lockedY - 1,                   0); DLT_UpdateWindowTitles(); } break;

                case SDLK_0:
                case SDLK_1:
                case SDLK_2:
                case SDLK_3:
                case SDLK_4:
                case SDLK_5:
                case SDLK_6:
                case SDLK_7:
                case SDLK_8:
                case SDLK_9: {
                    if (e.key.keysym.mod & KMOD_CTRL) {
                        char file[32];
                        for (int i = 0; i < (int)envs.size(); ++i) {
                            SDL_snprintf(file, sizeof(file), "output%s-%d.png", SDL_GetKeyName(e.key.keysym.sym), i);
                            if (envs[i].dest) {
                                SDL_Surface * tempDestABGR = NULL;
                                SDL_PixelFormat * format = SDL_AllocFormat(SDL_PIXELFORMAT_ABGR8888);
                                
                                if ( ! format) {
                                    SDL_Log("Unable to save %s: \"%s\"", file, SDL_GetError());
                                } else {
                                    tempDestABGR = SDL_ConvertSurface(envs[i].dest, format, 0);
                                    if ( ! tempDestABGR) {
                                        SDL_Log("Unable to save %s: \"%s\"", file, SDL_GetError());
                                    } else if ( ! stbi_write_png(file, tempDestABGR->w, tempDestABGR->h, 4, tempDestABGR->pixels, tempDestABGR->pitch)) {
                                        SDL_Log("Unable to save %s: unknown error in stbi_write_png", file);
                                    } else {
                                        SDL_Log("Saved %s", file);
                                    }
                                }
                                
                                if (tempDestABGR) {
                                    SDL_FreeSurface(tempDestABGR);
                                }
                                if (format) {
                                    SDL_FreeFormat(format);
                                }
                           }
                        }
                    } else {
                        int compare_threshold = SDL_atoi(SDL_GetKeyName(e.key.keysym.sym));
                        for (int i = 0; i < (int)envs.size(); ++i) {
                            DLT_Render_Compare * cmp = dynamic_cast<DLT_Render_Compare *>(envs[i].renderer);
                            if (cmp) {
                                cmp->compare_threshold = compare_threshold;
                            }
                        }
                        DLT_UpdateWindowTitles();
                    }
                } break;

                case SDLK_c:
                case SDLK_a:
                case SDLK_r:
                case SDLK_g:
                case SDLK_b:
                case SDLK_x: {
                    DLT_ComparisonType compare = DLT_COMPARE_ARGB;
                    switch (e.key.keysym.sym) {
                        case SDLK_c: compare = DLT_COMPARE_ARGB;    break;
                        case SDLK_a: compare = DLT_COMPARE_A;       break;
                        case SDLK_r: compare = DLT_COMPARE_R;       break;
                        case SDLK_g: compare = DLT_COMPARE_G;       break;
                        case SDLK_b: compare = DLT_COMPARE_B;       break;
                        case SDLK_x: compare = DLT_COMPARE_RGB;     break;
                    }
                    for (int i = 0; i < (int)envs.size(); ++i) {
                        DLT_Render_Compare * cmp = dynamic_cast<DLT_Render_Compare *>(envs[i].renderer);
                        if (cmp) {
                            cmp->compare_type = compare;
                        }
                    }
                    DLT_UpdateWindowTitles();
                } break;
            }
        } break;
        case SDL_WINDOWEVENT: {
            switch (e.window.event) {
                case SDL_WINDOWEVENT_CLOSE: {
                    exit(0);
                } break;
            }
        } break;
    }
}

#ifndef _MSC_VER
void DLT_HandleUnixSignal(int sig)
{
    switch (sig) {
        case SIGINT: {
            exit(1);
        } break;
    }
}
#endif

void DLT_Tick()
{
    double lastLockedX = 0;
    double lastLockedY = 0;

    if (numTicksToQuit == 0) {
        int exitCode = 0;
        if (cmpExit) {
            for (int i = 0; i < (int)envs.size(); ++i) {
                DLT_Render_Compare * cmp = dynamic_cast<DLT_Render_Compare *>(envs[i].renderer);
                if (cmp && ! cmp->last_compare_result) {
                    exitCode = 1;
                }
            }
        }
        exit(exitCode);
    }

    totalTicks++;
    if (numTicksToNextMeasurement > 0) {
        numTicksToNextMeasurement--;
    }
    if (numTicksToNextMeasurement == 0) {
        Uint64 now = SDL_GetPerformanceCounter();
        double dtInMS = (double)((now - lastMeasurement) * 1000) / SDL_GetPerformanceFrequency();
        double fps = (double)numTicksBetweenPerfMeasurements / (dtInMS / 1000.0);
        // SDL_Log("%f ms for %d ticks ; %f FPS\n", dtInMS, numTicksBetweenPerfMeasurements, fps);
        double totalDtInS = (double)((now - initialMeasurement) * 1) / SDL_GetPerformanceFrequency();
        printf("  %-15.5f %-15.5f %-15.5f\n", totalDtInS, fps, (dtInMS / numTicksBetweenPerfMeasurements));
        lastMeasurement = now;
        numTicksToNextMeasurement = numTicksBetweenPerfMeasurements;
    }

    // Process events
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        DLT_Test_ProcessEvent(e);
    }

    // Draw
    for (int i = 0; i < (int)envs.size(); ++i) {
        DLT_Env * env = &envs[i];

        // Render schene
        envs[i].renderer->Draw(envs[i]);

        // Present rendered scene
        if (envs[i].window) {
            SDL_Renderer * r = SDL_GetRenderer(envs[i].window);
            SDL_UpdateTexture(env->texture, NULL, env->dest->pixels, env->dest->pitch);
            SDL_SetRenderDrawColor(r, 0, 0, 0, 0xff);
            SDL_RenderClear(r);
            SDL_RenderCopy(r, envs[i].texture, NULL, NULL);

            // Draw locked position
            if (DLT_IS_LOCKED()) {
                const int w = 15;
                const int h = 15;
                SDL_Rect box;
                if ( ! envs[i].crosshairs) {
                    SDL_Surface * crosshairs = SDL_CreateRGBSurface(0, w, h, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
                    const uint32_t crosshair_color = SDL_MapRGBA(crosshairs->format, 0xff, 0, 0, 0xff);
                    const uint32_t fill_inner_color = SDL_MapRGBA(crosshairs->format, 0, 0, 0, 0);
                    if (crosshairs) {
                        // Clear entire surface:
                        SDL_FillRect(crosshairs, NULL, 0x00000000);

                        // Draw vertical bar:
                        box = {w/2, 0, 1, h};
                        SDL_FillRect(crosshairs, &box, crosshair_color);

                        // Draw horizontal bar:
                        box = {0, h/2, w, 1};
                        SDL_FillRect(crosshairs, &box, crosshair_color);

                        // Draw outside of inner box:
                        box = {(w/2)-3, (h/2)-3, 7, 7};
                        SDL_FillRect(crosshairs, &box, crosshair_color);

                        // Clear inside of inner box:
                        box = {(w/2)-1, (h/2)-1, 3, 3};
                        SDL_FillRect(crosshairs, &box, fill_inner_color);

                        // Convert to texture:
                        envs[i].crosshairs = SDL_CreateTextureFromSurface(r, crosshairs);
                        SDL_FreeSurface(crosshairs);
                    }
                }
                if (envs[i].crosshairs) {
                    box = {lockedX-(w/2), lockedY-(h/2), w, h};
                    SDL_RenderCopy(r, envs[i].crosshairs, NULL, &box);
                }
            }

            SDL_RenderPresent(r);
        } // end of headless check
    }

    if (numTicksToQuit > 0) {
        --numTicksToQuit;
    }

    lastLockedX = lockedX;
    lastLockedY = lockedY;
}

void DLT_AddRenderer(DLT_Renderer * env)
{
    envs.push_back({env});
}

static char DLT_ErrorBuffer[8192] = {0};

static const char * DLT_GetError() {
    return DLT_ErrorBuffer;
}

static void DLT_ClearError() {
    DLT_ErrorBuffer[0] = '\0';
}

static void DLT_SetError(const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if ( ! fmt) {
        DLT_ClearError();
    } else {
        SDL_vsnprintf(DLT_ErrorBuffer, sizeof(DLT_ErrorBuffer), fmt, args);
    }
    va_end(args);
}

static bool DLT_AddRendererByName(const std::string & name) {
    DLT_ClearError();
    DLT_RendererMetadata * rmeta = DLT_FindRendererMetadataByName(name);
    if (rmeta) {
        DLT_Renderer * renderer = rmeta->factory();
        if (renderer) {
            renderer->metadata = rmeta;
            DLT_AddRenderer(renderer);
        } else {
            printf("ERROR: Unable to create renderer of registered-type, \"%s\"\n", rmeta->shortName.c_str());
            return false;
        }
    } else {
        printf("ERROR: unknown renderer, \"%s\"\n", name.c_str());
        return false;
    }
    return true;
}

int DLT_Run(int argc, char **argv, const char * defaultRenderer) {
    bool isHeadless = false;
    bool isFullscreen = false;
    int compare_threshold_default = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-?") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("DL_Raster test app\n"
                   "\n"
                   "Usage: \n"
                   "\ttest [-h | --headless] [-c | --cmp] [--interval|-i N] [--ticks|-n N] [-r NAME] [--list|-l] [--destscale|-d WxH]\n"
                   "\n"
                   "Options:\n"
                   "\t--headless | -hl      Run in headless-mode (without displaying any GUIs)\n"
                   "\t-n | --ticks N        Run for N ticks, then quit, or -1 to run indefinitely (default: -1)\n"
                   "\t--cmp | -c            Run in comparison mode\n"
                   "\t--cmpthreshold N      Set the pixel-difference threshold for comparisons (default: 0)\n"
                   "\t--cmpexit             Return a non-zero exit code if a comparison had any 'beyond' pixels.\n"
                   "\t                        (requires -t|--ticks N option)\n"
                   "\t--interval | -i N     Log a performance measurement every N ticks (default: %d)\n"
                   "\t-r NAME               Add a renderer of name (case-insensitive), NAME \n"
                   "\t-rl                   List available renderers\n"
                   "\t--destscale | -d WxH  Destination scaling factor\n"
                   "\n",
                   numTicksBetweenPerfMeasurements
                   );
            return 0;
        } else if (strcmp(argv[i], "-hl") == 0 || strcmp(argv[i], "--headless") == 0) {
            isHeadless = true;
        } else if (SDL_strcmp(argv[i], "-d") == 0 || SDL_strcmp(argv[i], "--destscale") == 0) {
            if (++i < argc) {
                if (SDL_strcasecmp(argv[i], "full") == 0) {
                    isFullscreen = true;
                }
            }
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--cmp") == 0) {
            DLT_AddRenderer(new DLT_Render_Compare());
        } else if (strcmp(argv[i], "--cmpthreshold") == 0) {
            if (++i < argc) {
                compare_threshold_default = SDL_atoi(argv[i]);
            }
        } else if (strcmp(argv[i], "--cmpexit") == 0) {
            cmpExit = true;
        } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--ticks") == 0) {
            if ((i + 1) < argc) {
                numTicksToQuit = atoi(argv[i+1]);
                i++;
            }
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interval") == 0) {
            if ((i + 1) < argc) {
                numTicksBetweenPerfMeasurements = atoi(argv[i+1]);
                i++;
            }
        } else if (SDL_strcmp(argv[i], "-rl") == 0) {
            std::vector<DLT_RendererMetadata *> sorted(DLT_AllRendererMetadata());
            std::sort(sorted.begin(), sorted.end(),
                [] (DLT_RendererMetadata * a, DLT_RendererMetadata * b) {
                    return SDL_strcasecmp(a->shortName.c_str(), b->shortName.c_str()) < 0;
                }
            );
            for (DLT_RendererMetadata * metadata : sorted) {
                if (metadata->description.empty()) {
                    printf("%s\n", metadata->shortName.c_str());
                } else {
                    printf("%-12s -- %s\n",
                        metadata->shortName.c_str(),
                        metadata->description.c_str()
                    );
                }
            }
            return 0;
        } else if (SDL_strcmp(argv[i], "-r") == 0 || SDL_strcmp(argv[i], "--render") == 0) {
            if (++i < argc) {
                if ( ! DLT_AddRendererByName(argv[i])) {
                    printf("ERROR: %s\n", DLT_GetError());
                    return 1;
                }
            }
        }
    }


    if (DLT_GetEnvs().empty()) {
        if (defaultRenderer) {
            if ( ! DLT_AddRendererByName(defaultRenderer)) {
                printf("ERROR: %s\n", DLT_GetError());
                return 1;
            }
        }
    }

    if (DLT_GetEnvs().empty()) {
        printf("ERROR: no renderers specified, and no, valid default is available!\n");
        return 1;
    }

    // Catch Ctrl+C
#ifndef _MSC_VER
    struct sigaction signalHandler;
    signalHandler.sa_handler = DLT_HandleUnixSignal;
    sigemptyset(&signalHandler.sa_mask);
    signalHandler.sa_flags = 0;
    sigaction(SIGINT, &signalHandler, NULL);
#endif

    // Init SDL, but only if we're not all-headless
    for (int i = 0; i < (int)envs.size(); ++i) {
        if (!isHeadless || isFullscreen) {
            if ( ! SDL_WasInit(0)) {
                if (SDL_Init(SDL_INIT_VIDEO) != 0) {
                    SDL_Log("Can't init SDL: %s", SDL_GetError());
                    return 1;
                }
            }
        }
    }

    // Init each env
    int winX = 0;
    int winY = winSpacingH;
    for (int i = 0; i < (int)envs.size(); ++i) {
        envs[i].index = i;
        envs[i].renderer->Init(envs[i]);
        if (dynamic_cast<DLT_Render_Compare *>(envs[i].renderer)) {
            DLT_Render_Compare * cmp = (DLT_Render_Compare *) envs[i].renderer;
            cmp->compare_threshold = compare_threshold_default;
        }

        int w = (envs[i].dest ? envs[i].dest->w : 256);
        int h = (envs[i].dest ? envs[i].dest->h : 256);
        if (!isHeadless) {
            int window_flags = 0;
            if (isFullscreen) {
                window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
            }
            
            envs[i].window = SDL_CreateWindow(
                "DLTest",
                winX,
                winY,
                w, h,
                window_flags
            );
            winX += w + winSpacingW;
            if ( ! envs[i].window) {
                SDL_Log("SDL_CreateWindow failed, err=%s", SDL_GetError());
                return 1;
            }

            SDL_Renderer * r = SDL_CreateRenderer(envs[i].window, -1, 0);
            if ( ! r) {
                SDL_Log("SDL_CreateRenderer failed, err=%s", SDL_GetError());
                return 1;
            }
            envs[i].texture = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
            if ( ! envs[i].texture) {
                SDL_Log("Can't create background texture: %s", SDL_GetError());
                return 1;
            }
        }
        if ( ! envs[i].dest) {
            envs[i].dest = SDL_CreateRGBSurface(0, w, h, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
        }
        if ( ! envs[i].dest) {
            SDL_Log("Can't create background surface: %s", SDL_GetError());
            return 1;
        }
        SDL_FillRect(envs[i].dest, NULL, 0xFF000000);
    } // end of init loop
    DLT_UpdateWindowTitles();

    // Performance measurement stuff:
    if (numTicksBetweenPerfMeasurements > 0) {
        printf("# %-15s %-15s %-15s\n", "t,seconds", "fps", "ms/frame");
    }
    numTicksToNextMeasurement = numTicksBetweenPerfMeasurements;
    initialMeasurement = SDL_GetPerformanceCounter();
    lastMeasurement = initialMeasurement;

    // Render loop: process events, then draw.  Repeat until app is done.
    totalTicks = 0;
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(DLT_Tick, 60, 1);
#else
    while (true) {
        DLT_Tick();
    }
#endif

    return 0;
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
