
#define DLIR_IMPLEMENTATION
#include "../DL_ImageResize.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb_image.h"

#include <benchmark/benchmark.h>
#include <errno.h>

struct DLIRBench_Surface {
    uint8_t * pixels;
    void * alloced_buffer;
    int w;
    int h;
    int pitch;

    DLIRBench_Surface(const char * fileName) : pixels(0), alloced_buffer(0), w(0), h(0), pitch(0) {
        FILE * srcFilePtr = fopen(fileName, "r");
        if (!srcFilePtr) {
            printf("ERROR: Unable to open srcFile, reason:\"%s\"\n", strerror(errno));
            return;
        }
        pixels = (uint8_t *) stbi_load_from_file(srcFilePtr, &w, &h, NULL, 4);
        fclose(srcFilePtr);
        if (!pixels) {
            printf("ERROR: Unable to decode opened srcFile, reason:\"%s\"\n", stbi_failure_reason());
            return;
        }
        pitch = w * 4;
        // Convert from ABGR8888 to ARGB8888
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const size_t offset = (y * pitch) + (x << 2);
                const uint8_t r = pixels[offset + 0];
                const uint8_t g = pixels[offset + 1];
                const uint8_t b = pixels[offset + 2];
                const uint8_t a = pixels[offset + 3];
                pixels[offset + 0] = b;
                pixels[offset + 1] = g;
                pixels[offset + 2] = r;
                pixels[offset + 3] = a;
            }
        }
    }

    DLIRBench_Surface(int _w, int _h) : pixels(0), alloced_buffer(0), w(_w), h(_h), pitch(_w * 4) {
        alloced_buffer = calloc(1, (pitch * h) + 15);
        pixels = (uint8_t *)(((uintptr_t)alloced_buffer + 15) & ~ 0x0F);
    }

    ~DLIRBench_Surface() {
        if (alloced_buffer) {
            free(alloced_buffer);
            alloced_buffer = 0;
            pixels = 0;
        }
        if (pixels) {
            free(pixels);
            pixels = 0;
        }
    }
};


static inline void DLIRBench_Resize(const DLIRBench_Surface & src, const DLIRBench_Surface & dest) {
    DLIR_ResizeBilinear_ARGB8888(
        src.pixels, // const void * src,
        0, // uint32_t srcX,
        0, // uint32_t srcY,
        src.w, // uint32_t srcWidth,
        src.h, // uint32_t srcHeight,
        src.pitch, // uint32_t srcPitch,

        dest.pixels, // void * dest,
        0, // uint32_t destX,
        0, // uint32_t destY,
        dest.w, // uint32_t destWidth,
        dest.h, // uint32_t destHeight,
        dest.pitch, // uint32_t destPitch,

        0, // int32_t srcUpdateX,
        0, // int32_t srcUpdateY,
        src.w, // int32_t srcUpdateWidth,
        src.h // int32_t srcUpdateHeight
    );
}

static void DLIR_From_640x480(benchmark::State & state) {
    DLIRBench_Surface src("src_640x480.png");
    DLIRBench_Surface dest(state.range(0), state.range(1));
    if (src.pixels && dest.pixels) {
        while (state.KeepRunning()) {
            DLIRBench_Resize(src, dest);
        }
    }
}
BENCHMARK(DLIR_From_640x480)
    ->Unit(benchmark::kMillisecond)
    ->Args({512, 384})
    ->Args({640, 480})
    ->Args({800, 600})
    ->Args({1024, 768})
    ->Args({1280, 960})
    ->Args({1920, 1200})
    // ->Ranges({{8, 1920}, {8, 1200}})
    ;

BENCHMARK_MAIN();
