// DL_ImageResize.cpp
// zlib-licensed, Oct 8 2017, David Ludwig, dludwig@pobox.com. See "COPYING" statement at the end of this file.

#include "DL_ImageResize.h"

#include <stdint.h>

// Fixed point math variables:
#define DLIR_FIXED 10
typedef uint32_t DLIR_Fixed;
typedef uint32_t DLIR_FixedBig;

#define DLIR_min(A, B) (((A) < (B)) ? (A) : (B))
#define DLIR_max(A, B) (((A) > (B)) ? (A) : (B))



#if 0 // __SSE4_1__

#include <emmintrin.h>
#include <smmintrin.h>

    __m128i data;

    inline DLIR_Vec4u32() = default;
    inline DLIR_Vec4u32(__m128i src) : data(src) {}
    inline DLIR_Vec4u32(uint32_t scalar) : data(_mm_set1_epi32(scalar)) {}
    inline DLIR_Vec4u32(uint32_t scalar0, uint32_t scalar1, uint32_t scalar2, uint32_t scalar3) : data(_mm_setr_epi32(scalar0,scalar1,scalar2,scalar3)) {}

    inline DLIR_Vec4u32  operator +  (const DLIR_Vec4u32 & b) const { return _mm_add_epi32(data, b.data);   }
    inline DLIR_Vec4u32  operator -  (const DLIR_Vec4u32 & b) const { return _mm_sub_epi32(data, b.data);   }
    inline DLIR_Vec4u32  operator *  (const DLIR_Vec4u32 & b) const { return _mm_mullo_epi32(data, b.data); }
    inline DLIR_Vec4u32  operator &  (uint32_t scalar)        const { return _mm_and_si128(data, _mm_set1_epi32(scalar)); }
    inline DLIR_Vec4u32  operator |  (const DLIR_Vec4u32 & b) const { return _mm_or_si128(data, b.data);    }
    inline DLIR_Vec4u32  operator << (int imm8)               const { return _mm_slli_epi32(data, imm8);    }
    inline DLIR_Vec4u32  operator >> (int imm8)               const { return _mm_srli_epi32(data, imm8);    }
    // inline uint32_t getu32      (int imm8)                    const { return _mm_extract_epi32(data, imm8); }

    // inline void store_si128_aligned  (uint32_t * dest)        const { _mm_store_si128(((__m128i *)(dest)), data); }
    inline void store_si128_unaligned(uint8_t * dest)         const { _mm_storeu_si128(((__m128i *)(dest)), data); }
};

inline DLIR_Vec4u32 operator + (uint32_t scalar, const DLIR_Vec4u32 & b) { return DLIR_Vec4u32(scalar) + b; }
inline DLIR_Vec4u32 operator - (uint32_t scalar, const DLIR_Vec4u32 & b) { return DLIR_Vec4u32(scalar) - b; }

#if 0
// Slower, SSE2-compatible version:
#define DLIR_Vec4u32_loadFromOffsets4x32(dest, src, offsets, extraOffset) { \
    uint32_t __attribute__((aligned(16))) rawOffsets[4]; \
    _mm_store_si128((__m128i *)rawOffsets, offsets.data); \
    dest = { \
        *((uint32_t *)(src + rawOffsets[0] + extraOffset)), \
        *((uint32_t *)(src + rawOffsets[1] + extraOffset)), \
        *((uint32_t *)(src + rawOffsets[2] + extraOffset)), \
        *((uint32_t *)(src + rawOffsets[3] + extraOffset)), \
    }; \
}
#else
// Faster, SSE4.1 version:
#define DLIR_Vec4u32_loadFromOffsets4x32(dest, src, offsets, extraOffset)   \
dest = {                                                                    \
    *((uint32_t *)(src + _mm_extract_epi32(offsets.data, 0) + extraOffset)),\
    *((uint32_t *)(src + _mm_extract_epi32(offsets.data, 1) + extraOffset)),\
    *((uint32_t *)(src + _mm_extract_epi32(offsets.data, 2) + extraOffset)),\
    *((uint32_t *)(src + _mm_extract_epi32(offsets.data, 3) + extraOffset)) \
}
#endif

inline void DLIR_Vec4u32_store_RGB(
    uint8_t * dest,
    uint32_t destOffset,
    const DLIR_Vec4u32 & vfxRed,
    const DLIR_Vec4u32 & vfxGreen,
    const DLIR_Vec4u32 & vfxBlue
) {

#if DLIR_FIXED == 10
    DLIR_Vec4u32 final = \
        DLIR_Vec4u32(0xff000000) |
        ((vfxRed   <<  6) & 0x00ff0000) |
        ((vfxGreen >>  2) & 0x0000ff00) |
        ((vfxBlue  >> 10) & 0x000000ff);
#else
    DLIR_Vec4u32 final = \
        DLIR_Vec4u32(0xff000000) |
        (((vfxRed   >> DLIR_FIXED) << 16) & 0x00ff0000) |
        (((vfxGreen >> DLIR_FIXED) <<  8) & 0x0000ff00) |
        (((vfxBlue  >> DLIR_FIXED)      ) & 0x000000ff);
#endif

    final.store_si128_unaligned(dest + destOffset);
    // final.store_si128_aligned(dest + destOffset);
}

void DLIR_ResizeBilinear_ARGB8888_Body_SSE41(
    uint8_t * pSrc,
    int nSrcWidth,
    int nSrcHeight,
    int nSrcPitch,
    uint8_t * pDest,
    int nDestWidth,
    int nDestHeight,
    int nDestPitch,
    int nDestXMax,
    int nDestYMax
)
{
    DLIR_Vec4u32  vfxSrc0, vfxSrc1, vfxSrc2, vfxSrc3;
    uint32_t nSrcY;

    DLIR_Vec4u32  vnSrcX, vnSrcIndex;
    const DLIR_Vec4u32  vfxRatioDestXToSrcX = ((nSrcWidth - 1)<<DLIR_FIXED) / nDestWidth;
    const DLIR_Fixed    fxRatioDestYToSrcY = (((nSrcHeight - 1)<<DLIR_FIXED) / nDestHeight);
    DLIR_FixedBig fxDiffY;
    DLIR_Vec4u32  vfxDiffX;
    DLIR_Vec4u32  vfxBlue, vfxGreen, vfxRed;

    for (uint32_t nDestY = 0; nDestY < nDestYMax; nDestY++) {
#if 1   // DavidL: This path measures slightly faster, on my test systems
        nSrcY = ((DLIR_FixedBig)fxRatioDestYToSrcY * (DLIR_FixedBig)(nDestY<<DLIR_FIXED))>>(DLIR_FIXED*2);
        fxDiffY = (((DLIR_FixedBig)fxRatioDestYToSrcY * (DLIR_FixedBig)(nDestY<<DLIR_FIXED))>>DLIR_FIXED) - (nSrcY << DLIR_FIXED);
#else
        nSrcY = ((DLIR_FixedBig)fxRatioDestYToSrcY * nDestY)>>DLIR_FIXED;
        fxDiffY = ((DLIR_FixedBig)fxRatioDestYToSrcY * (DLIR_FixedBig)nDestY) - (nSrcY << DLIR_FIXED);
#endif

        for (uint32_t nDestX = 0; nDestX < nDestXMax; nDestX += 4) {
            DLIR_Vec4u32 vnDestX = DLIR_Vec4u32(nDestX, nDestX + 1, nDestX + 2, nDestX + 3);
            vnSrcX   = (vfxRatioDestXToSrcX * vnDestX) >> DLIR_FIXED;
            vfxDiffX = (vfxRatioDestXToSrcX * vnDestX) - (vnSrcX << DLIR_FIXED);

            vnSrcIndex = (nSrcY * nSrcPitch) + (vnSrcX << 2);
            DLIR_Vec4u32_loadFromOffsets4x32(vfxSrc0, pSrc, vnSrcIndex, 0);
            DLIR_Vec4u32_loadFromOffsets4x32(vfxSrc1, pSrc, vnSrcIndex, 4);
            DLIR_Vec4u32_loadFromOffsets4x32(vfxSrc2, pSrc, vnSrcIndex, nSrcPitch);
            DLIR_Vec4u32_loadFromOffsets4x32(vfxSrc3, pSrc, vnSrcIndex, nSrcPitch + 4);

            const DLIR_Vec4u32 vfxone = (1<<DLIR_FIXED);
            DLIR_Vec4u32 vfxFactor3 = ((         vfxDiffX) * (         fxDiffY)) >> DLIR_FIXED;
            DLIR_Vec4u32 vfxFactor2 = ((vfxone - vfxDiffX) * (         fxDiffY)) >> DLIR_FIXED;
            DLIR_Vec4u32 vfxFactor1 = ((         vfxDiffX) * (vfxone - fxDiffY)) >> DLIR_FIXED;
            DLIR_Vec4u32 vfxFactor0 = ((vfxone - vfxDiffX) * (vfxone - fxDiffY)) >> DLIR_FIXED;

            vfxBlue = (
                (((vfxSrc0    ) & 0xff)) * vfxFactor0 +
                (((vfxSrc1    ) & 0xff)) * vfxFactor1 +
                (((vfxSrc2    ) & 0xff)) * vfxFactor2 +
                (((vfxSrc3    ) & 0xff)) * vfxFactor3
            );

            vfxGreen = (
                (((vfxSrc0>> 8) & 0xff)) * vfxFactor0 +
                (((vfxSrc1>> 8) & 0xff)) * vfxFactor1 +
                (((vfxSrc2>> 8) & 0xff)) * vfxFactor2 +
                (((vfxSrc3>> 8) & 0xff)) * vfxFactor3
            );

            vfxRed = (
                (((vfxSrc0>>16) & 0xff)) * vfxFactor0 +
                (((vfxSrc1>>16) & 0xff)) * vfxFactor1 +
                (((vfxSrc2>>16) & 0xff)) * vfxFactor2 +
                (((vfxSrc3>>16) & 0xff)) * vfxFactor3
            );
            
            DLIR_Vec4u32_store_RGB(pDest, (nDestX<<2), vfxRed, vfxGreen, vfxBlue);
        }
        pDest += nDestPitch;
    }
}

#endif // __SSE4_1__

/*
void DLIR_CalcSrcRectFromDest(
    uint32_t destX,
    uint32_t destY,
    uint32_t destWidth,
    uint32_t destHeight,
    uint32_t * srcX,
    uint32_t * srcY,
    uint32_t * srcWidth,
    uint32_t * srcHeight
) {
    const DLIR_Fixed fxRatioDestXToSrcX = (((srcWidth - 1)<<DLIR_FIXED) / destWidth);
    const DLIR_Fixed fxRatioDestYToSrcY = (((srcHeight - 1)<<DLIR_FIXED) / destHeight);
    if (srcX) {
        *srcX = (fxRatioDestXToSrcX * srcX) >> DLIR_FIXED;
    }
    if (srcWidth) {
        *srcWidth = (fxRatioDestXToSrcX * srcWidth) >> DLIR_FIXED;
    }
    if (srcY) {
        *srcY = (fxRatioDestYToSrcY * srcY) >> DLIR_FIXED;
    }
    if (srcHeight) {
        *srcHeight = (fxRatioDestYToSrcY * srcHeight) >> DLIR_FIXED;
    }
}
*/


static void DLIR_ResizeBilinear_ARGB8888_Body_Plain(
    uint8_t * pSrc,
    uint32_t nSrcWidth,
    uint32_t nSrcHeight,
    uint32_t nSrcPitch,
    uint8_t * pDest,
    uint32_t nDestWidth,
    uint32_t nDestHeight,
    uint32_t nDestPitch,
    int nDestXMin,
    int nDestYMin,
    int nDestXMax,
    int nDestYMax
) {
    uint32_t fxSrc0, fxSrc1, fxSrc2, fxSrc3;
    uint32_t nSrcX, nSrcY, nSrcIndex;

    const DLIR_Fixed fxRatioDestXToSrcX = ((nSrcWidth<<DLIR_FIXED) / nDestWidth);
    const DLIR_Fixed fxRatioDestYToSrcY = ((nSrcHeight<<DLIR_FIXED) / nDestHeight);
    DLIR_FixedBig diffX, diffY;
    DLIR_FixedBig fxBlue, fxGreen, fxRed;
    
    for (int nDestY = nDestYMin; nDestY <= nDestYMax; nDestY++) {
        nSrcY = ((DLIR_FixedBig)fxRatioDestYToSrcY * (DLIR_FixedBig)(nDestY<<DLIR_FIXED))>>(DLIR_FIXED*2);
        diffY = (((DLIR_FixedBig)fxRatioDestYToSrcY * (DLIR_FixedBig)(nDestY<<DLIR_FIXED))>>DLIR_FIXED) - (nSrcY << DLIR_FIXED);

        for (int nDestX = nDestXMin; nDestX <= nDestXMax; nDestX++) {
            nSrcX =  ((DLIR_FixedBig)fxRatioDestXToSrcX * (DLIR_FixedBig)(nDestX<<DLIR_FIXED))>>(DLIR_FIXED*2);
            diffX = (((DLIR_FixedBig)fxRatioDestXToSrcX * (DLIR_FixedBig)(nDestX<<DLIR_FIXED))>>DLIR_FIXED) - (nSrcX << DLIR_FIXED);

            nSrcIndex = (nSrcY * nSrcPitch) + (nSrcX << 2);
            fxSrc0 = *((uint32_t *)(pSrc + nSrcIndex));
            fxSrc1 = *((uint32_t *)(pSrc + nSrcIndex + 4));
            fxSrc2 = *((uint32_t *)(pSrc + nSrcIndex + nSrcPitch));
            fxSrc3 = *((uint32_t *)(pSrc + nSrcIndex + nSrcPitch + 4));

            DLIR_FixedBig fxFactor3 = ((                  diffX) * (                  diffY)) >> DLIR_FIXED;
            DLIR_FixedBig fxFactor2 = (((1<<DLIR_FIXED) - diffX) * (                  diffY)) >> DLIR_FIXED;
            DLIR_FixedBig fxFactor1 = ((                  diffX) * ((1<<DLIR_FIXED) - diffY)) >> DLIR_FIXED;
            DLIR_FixedBig fxFactor0 = (((1<<DLIR_FIXED) - diffX) * ((1<<DLIR_FIXED) - diffY)) >> DLIR_FIXED;

            fxBlue = (
                (((fxSrc0    ) & 0xff)<<DLIR_FIXED) * fxFactor0 +
                (((fxSrc1    ) & 0xff)<<DLIR_FIXED) * fxFactor1 +
                (((fxSrc2    ) & 0xff)<<DLIR_FIXED) * fxFactor2 +
                (((fxSrc3    ) & 0xff)<<DLIR_FIXED) * fxFactor3
            ) >> DLIR_FIXED;
            
            fxGreen = (
                (((fxSrc0>> 8) & 0xff)<<DLIR_FIXED) * fxFactor0 +
                (((fxSrc1>> 8) & 0xff)<<DLIR_FIXED) * fxFactor1 +
                (((fxSrc2>> 8) & 0xff)<<DLIR_FIXED) * fxFactor2 +
                (((fxSrc3>> 8) & 0xff)<<DLIR_FIXED) * fxFactor3
            ) >> DLIR_FIXED;
            
            fxRed = (
                (((fxSrc0>>16) & 0xff)<<DLIR_FIXED) * fxFactor0 +
                (((fxSrc1>>16) & 0xff)<<DLIR_FIXED) * fxFactor1 +
                (((fxSrc2>>16) & 0xff)<<DLIR_FIXED) * fxFactor2 +
                (((fxSrc3>>16) & 0xff)<<DLIR_FIXED) * fxFactor3
            ) >> DLIR_FIXED;
            
            *((uint32_t *)(pDest + (nDestX<<2))) =
                0xff000000 |
                (((fxRed  >>DLIR_FIXED) << 16) & 0xff0000) |
                (((fxGreen>>DLIR_FIXED) <<  8) &   0xff00) |
                 ((fxBlue >>DLIR_FIXED) <<  0);
        }

        pDest += nDestPitch;
//        pSrc += nSrcPitch;
    }
}


static void DLIR_ResizeBilinear_ARGB8888_Bottom_Plain(
    uint8_t * pSrc,
    uint32_t nSrcWidth,
    uint32_t nSrcHeight,
    uint32_t nSrcPitch,
    uint8_t * pDest,
    uint32_t nDestWidth,
    uint32_t nDestHeight,
    uint32_t nDestPitch,
    int nDestXMin,
    int nDestYMin,
    int nDestXMax,
    int nDestYMax
) {
    uint32_t fxSrc0, fxSrc1, fxSrc2, fxSrc3;
    uint32_t nSrcX, nSrcY, nSrcIndex;

    const DLIR_Fixed fxRatioDestXToSrcX = ((nSrcWidth<<DLIR_FIXED) / nDestWidth);
    const DLIR_Fixed fxRatioDestYToSrcY = ((nSrcHeight<<DLIR_FIXED) / nDestHeight);
    DLIR_FixedBig diffX, diffY;
    DLIR_FixedBig fxBlue, fxGreen, fxRed;
    
    for (int nDestY = nDestYMin; nDestY <= nDestYMax; nDestY++) {
        nSrcY = ((DLIR_FixedBig)fxRatioDestYToSrcY * (DLIR_FixedBig)(nDestY<<DLIR_FIXED))>>(DLIR_FIXED*2);
        diffY = (((DLIR_FixedBig)fxRatioDestYToSrcY * (DLIR_FixedBig)(nDestY<<DLIR_FIXED))>>DLIR_FIXED) - (nSrcY << DLIR_FIXED);

        for (int nDestX = nDestXMin; nDestX <= nDestXMax; nDestX++) {
            nSrcX =  ((DLIR_FixedBig)fxRatioDestXToSrcX * (DLIR_FixedBig)(nDestX<<DLIR_FIXED))>>(DLIR_FIXED*2);
            diffX = (((DLIR_FixedBig)fxRatioDestXToSrcX * (DLIR_FixedBig)(nDestX<<DLIR_FIXED))>>DLIR_FIXED) - (nSrcX << DLIR_FIXED);

            nSrcIndex = (nSrcY * nSrcPitch) + (nSrcX << 2);
            fxSrc0 = fxSrc2 = *((uint32_t *)(pSrc + nSrcIndex));
            fxSrc1 = fxSrc3 = *((uint32_t *)(pSrc + nSrcIndex + 4));

            DLIR_FixedBig fxFactor3 = ((                  diffX) * (                  diffY)) >> DLIR_FIXED;
            DLIR_FixedBig fxFactor2 = (((1<<DLIR_FIXED) - diffX) * (                  diffY)) >> DLIR_FIXED;
            DLIR_FixedBig fxFactor1 = ((                  diffX) * ((1<<DLIR_FIXED) - diffY)) >> DLIR_FIXED;
            DLIR_FixedBig fxFactor0 = (((1<<DLIR_FIXED) - diffX) * ((1<<DLIR_FIXED) - diffY)) >> DLIR_FIXED;

            fxBlue = (
                (((fxSrc0    ) & 0xff)<<DLIR_FIXED) * fxFactor0 +
                (((fxSrc1    ) & 0xff)<<DLIR_FIXED) * fxFactor1 +
                (((fxSrc2    ) & 0xff)<<DLIR_FIXED) * fxFactor2 +
                (((fxSrc3    ) & 0xff)<<DLIR_FIXED) * fxFactor3
            ) >> DLIR_FIXED;
            
            fxGreen = (
                (((fxSrc0>> 8) & 0xff)<<DLIR_FIXED) * fxFactor0 +
                (((fxSrc1>> 8) & 0xff)<<DLIR_FIXED) * fxFactor1 +
                (((fxSrc2>> 8) & 0xff)<<DLIR_FIXED) * fxFactor2 +
                (((fxSrc3>> 8) & 0xff)<<DLIR_FIXED) * fxFactor3
            ) >> DLIR_FIXED;
            
            fxRed = (
                (((fxSrc0>>16) & 0xff)<<DLIR_FIXED) * fxFactor0 +
                (((fxSrc1>>16) & 0xff)<<DLIR_FIXED) * fxFactor1 +
                (((fxSrc2>>16) & 0xff)<<DLIR_FIXED) * fxFactor2 +
                (((fxSrc3>>16) & 0xff)<<DLIR_FIXED) * fxFactor3
            ) >> DLIR_FIXED;
            
            *((uint32_t *)(pDest + (nDestX<<2))) =
                0xff000000 |
                (((fxRed  >>DLIR_FIXED) << 16) & 0xff0000) |
                (((fxGreen>>DLIR_FIXED) <<  8) &   0xff00) |
                 ((fxBlue >>DLIR_FIXED) <<  0);
        }

        pDest += nDestPitch;
//        pSrc += nSrcPitch;
    }
}


static void DLIR_ResizeBilinear_ARGB8888_Right_Plain(
    uint8_t * pSrc,
    uint32_t nSrcWidth,
    uint32_t nSrcHeight,
    uint32_t nSrcPitch,
    uint8_t * pDest,
    uint32_t nDestWidth,
    uint32_t nDestHeight,
    uint32_t nDestPitch,
    int nDestXMin,
    int nDestYMin,
    int nDestXMax,
    int nDestYMax
) {
    uint32_t fxSrc0, fxSrc1, fxSrc2, fxSrc3;
    uint32_t nSrcX, nSrcY, nSrcIndex;

    // const DLIR_Fixed fxRatioDestXToSrcX = (((nSrcWidth - 1)<<DLIR_FIXED) / nDestWidth);
    // const DLIR_Fixed fxRatioDestYToSrcY = (((nSrcHeight - 1)<<DLIR_FIXED) / nDestHeight);
    const DLIR_Fixed fxRatioDestXToSrcX = ((nSrcWidth<<DLIR_FIXED) / nDestWidth);
    const DLIR_Fixed fxRatioDestYToSrcY = ((nSrcHeight<<DLIR_FIXED) / nDestHeight);
    DLIR_FixedBig diffX, diffY;
    DLIR_FixedBig fxBlue, fxGreen, fxRed;
    
    for (int nDestY = nDestYMin; nDestY <= nDestYMax; nDestY++) {
        nSrcY = ((DLIR_FixedBig)fxRatioDestYToSrcY * (DLIR_FixedBig)(nDestY<<DLIR_FIXED))>>(DLIR_FIXED*2);
        diffY = (((DLIR_FixedBig)fxRatioDestYToSrcY * (DLIR_FixedBig)(nDestY<<DLIR_FIXED))>>DLIR_FIXED) - (nSrcY << DLIR_FIXED);

        for (int nDestX = nDestXMin; nDestX <= nDestXMax; nDestX++) {
            nSrcX =  ((DLIR_FixedBig)fxRatioDestXToSrcX * (DLIR_FixedBig)(nDestX<<DLIR_FIXED))>>(DLIR_FIXED*2);
            diffX = (((DLIR_FixedBig)fxRatioDestXToSrcX * (DLIR_FixedBig)(nDestX<<DLIR_FIXED))>>DLIR_FIXED) - (nSrcX << DLIR_FIXED);

            nSrcIndex = (nSrcY * nSrcPitch) + (nSrcX << 2);
            fxSrc0 = fxSrc1 = *((uint32_t *)(pSrc + nSrcIndex));
            fxSrc2 = fxSrc3 = *((uint32_t *)(pSrc + nSrcIndex + nSrcPitch));

            DLIR_FixedBig fxFactor3 = ((                  diffX) * (                  diffY)) >> DLIR_FIXED;
            DLIR_FixedBig fxFactor2 = (((1<<DLIR_FIXED) - diffX) * (                  diffY)) >> DLIR_FIXED;
            DLIR_FixedBig fxFactor1 = ((                  diffX) * ((1<<DLIR_FIXED) - diffY)) >> DLIR_FIXED;
            DLIR_FixedBig fxFactor0 = (((1<<DLIR_FIXED) - diffX) * ((1<<DLIR_FIXED) - diffY)) >> DLIR_FIXED;

            fxBlue = (
                (((fxSrc0    ) & 0xff)<<DLIR_FIXED) * fxFactor0 +
                (((fxSrc1    ) & 0xff)<<DLIR_FIXED) * fxFactor1 +
                (((fxSrc2    ) & 0xff)<<DLIR_FIXED) * fxFactor2 +
                (((fxSrc3    ) & 0xff)<<DLIR_FIXED) * fxFactor3
            ) >> DLIR_FIXED;
            
            fxGreen = (
                (((fxSrc0>> 8) & 0xff)<<DLIR_FIXED) * fxFactor0 +
                (((fxSrc1>> 8) & 0xff)<<DLIR_FIXED) * fxFactor1 +
                (((fxSrc2>> 8) & 0xff)<<DLIR_FIXED) * fxFactor2 +
                (((fxSrc3>> 8) & 0xff)<<DLIR_FIXED) * fxFactor3
            ) >> DLIR_FIXED;
            
            fxRed = (
                (((fxSrc0>>16) & 0xff)<<DLIR_FIXED) * fxFactor0 +
                (((fxSrc1>>16) & 0xff)<<DLIR_FIXED) * fxFactor1 +
                (((fxSrc2>>16) & 0xff)<<DLIR_FIXED) * fxFactor2 +
                (((fxSrc3>>16) & 0xff)<<DLIR_FIXED) * fxFactor3
            ) >> DLIR_FIXED;
            
            *((uint32_t *)(pDest + (nDestX<<2))) =
                0xff000000 |
                (((fxRed  >>DLIR_FIXED) << 16) & 0xff0000) |
                (((fxGreen>>DLIR_FIXED) <<  8) &   0xff00) |
                 ((fxBlue >>DLIR_FIXED) <<  0);
        }

        pDest += nDestPitch;
//        pSrc += nSrcPitch;
    }
}

static void DLIR_ResizeBilinear_ARGB8888_BottomRight_Plain(
    uint8_t * pSrc,
    uint32_t nSrcWidth,
    uint32_t nSrcHeight,
    uint32_t nSrcPitch,
    uint8_t * pDest,
    uint32_t nDestWidth,
    uint32_t nDestHeight,
    uint32_t nDestPitch,
    int nDestXMin,
    int nDestYMin,
    int nDestXMax,
    int nDestYMax
) {
    uint32_t fxSrc0, fxSrc1, fxSrc2, fxSrc3;
    uint32_t nSrcX, nSrcY, nSrcIndex;

    const DLIR_Fixed fxRatioDestXToSrcX = ((nSrcWidth<<DLIR_FIXED) / nDestWidth);
    const DLIR_Fixed fxRatioDestYToSrcY = ((nSrcHeight<<DLIR_FIXED) / nDestHeight);
    DLIR_FixedBig diffX, diffY;
    DLIR_FixedBig fxBlue, fxGreen, fxRed;
    
    for (int nDestY = nDestYMin; nDestY <= nDestYMax; nDestY++) {
        nSrcY = ((DLIR_FixedBig)fxRatioDestYToSrcY * (DLIR_FixedBig)(nDestY<<DLIR_FIXED))>>(DLIR_FIXED*2);
        diffY = (((DLIR_FixedBig)fxRatioDestYToSrcY * (DLIR_FixedBig)(nDestY<<DLIR_FIXED))>>DLIR_FIXED) - (nSrcY << DLIR_FIXED);

        for (int nDestX = nDestXMin; nDestX <= nDestXMax; nDestX++) {
            nSrcX =  ((DLIR_FixedBig)fxRatioDestXToSrcX * (DLIR_FixedBig)(nDestX<<DLIR_FIXED))>>(DLIR_FIXED*2);
            diffX = (((DLIR_FixedBig)fxRatioDestXToSrcX * (DLIR_FixedBig)(nDestX<<DLIR_FIXED))>>DLIR_FIXED) - (nSrcX << DLIR_FIXED);

            nSrcIndex = (nSrcY * nSrcPitch) + (nSrcX << 2);
            fxSrc0 = fxSrc1 = fxSrc2 = fxSrc3 = *((uint32_t *)(pSrc + nSrcIndex));

            DLIR_FixedBig fxFactor3 = ((                  diffX) * (                  diffY)) >> DLIR_FIXED;
            DLIR_FixedBig fxFactor2 = (((1<<DLIR_FIXED) - diffX) * (                  diffY)) >> DLIR_FIXED;
            DLIR_FixedBig fxFactor1 = ((                  diffX) * ((1<<DLIR_FIXED) - diffY)) >> DLIR_FIXED;
            DLIR_FixedBig fxFactor0 = (((1<<DLIR_FIXED) - diffX) * ((1<<DLIR_FIXED) - diffY)) >> DLIR_FIXED;

            fxBlue = (
                (((fxSrc0    ) & 0xff)<<DLIR_FIXED) * fxFactor0 +
                (((fxSrc1    ) & 0xff)<<DLIR_FIXED) * fxFactor1 +
                (((fxSrc2    ) & 0xff)<<DLIR_FIXED) * fxFactor2 +
                (((fxSrc3    ) & 0xff)<<DLIR_FIXED) * fxFactor3
            ) >> DLIR_FIXED;
            
            fxGreen = (
                (((fxSrc0>> 8) & 0xff)<<DLIR_FIXED) * fxFactor0 +
                (((fxSrc1>> 8) & 0xff)<<DLIR_FIXED) * fxFactor1 +
                (((fxSrc2>> 8) & 0xff)<<DLIR_FIXED) * fxFactor2 +
                (((fxSrc3>> 8) & 0xff)<<DLIR_FIXED) * fxFactor3
            ) >> DLIR_FIXED;
            
            fxRed = (
                (((fxSrc0>>16) & 0xff)<<DLIR_FIXED) * fxFactor0 +
                (((fxSrc1>>16) & 0xff)<<DLIR_FIXED) * fxFactor1 +
                (((fxSrc2>>16) & 0xff)<<DLIR_FIXED) * fxFactor2 +
                (((fxSrc3>>16) & 0xff)<<DLIR_FIXED) * fxFactor3
            ) >> DLIR_FIXED;
            
            *((uint32_t *)(pDest + (nDestX<<2))) =
                0xff000000 |
                (((fxRed  >>DLIR_FIXED) << 16) & 0xff0000) |
                (((fxGreen>>DLIR_FIXED) <<  8) &   0xff00) |
                 ((fxBlue >>DLIR_FIXED) <<  0);
        }

        pDest += nDestPitch;
    }
}


extern "C"
void DLIR_ResizeBilinear_ARGB8888(
    void * _src,
    uint32_t srcX,
    uint32_t srcY,
    uint32_t srcWidth,
    uint32_t srcHeight,
    uint32_t srcPitch,
    void * _dest,
    uint32_t destX,
    uint32_t destY,
    uint32_t destWidth,
    uint32_t destHeight,
    uint32_t destPitch,
    int32_t srcUpdateX,
    int32_t srcUpdateY,
    int32_t srcUpdateWidth,
    int32_t srcUpdateHeight
) {
    const DLIR_Fixed fxRatioSrcXtoDestX = (destWidth << DLIR_FIXED) / (srcWidth); // - 1);
    const DLIR_Fixed fxRatioSrcYtoDestY = (destHeight << DLIR_FIXED) / (srcHeight); // - 1);

    //
    //                                   -1 here is for margin
    //                                   on the left/top side
    //                                             |
    //                                            vvv
    int32_t destUpdateX = (DLIR_max(0, srcUpdateX - 1) * fxRatioSrcXtoDestX) >> DLIR_FIXED;
    int32_t destUpdateY = (DLIR_max(0, srcUpdateY - 1) * fxRatioSrcYtoDestY) >> DLIR_FIXED;

    //
    //                                                                                         +1 (in fixed-point) is to
    //                                                                                         deal with rounding errors
    //                                                                                                     |
    //                                                     +2 here is +1 for margin on                     |
    //                                                     the left/top side, and +1 for                   |
    //                                                     right/bottom side                               |
    //                                                                 |                                   |
    //                                                                vvv                          vvvvvvvvvvvvvvvv                                                               
    int32_t destUpdateWidth  = ((DLIR_min(srcWidth,  srcUpdateWidth  + 2) * fxRatioSrcXtoDestX) + (1 << DLIR_FIXED)) >> DLIR_FIXED;
    int32_t destUpdateHeight = ((DLIR_min(srcHeight, srcUpdateHeight + 2) * fxRatioSrcYtoDestY) + (1 << DLIR_FIXED)) >> DLIR_FIXED;

    // Prevent destUpdateX and destUpdateY from being less than 0
    destUpdateX = DLIR_max(0, destUpdateX);
    destUpdateY = DLIR_max(0, destUpdateY);

    // Prevent destUpdateX2 and destUpdateY2 from being more than width and height.
    // Also, note whether special considerations need to be taken for the right-column,
    // or the bottom-row.
    int32_t destUpdateX2 = DLIR_min(
        destWidth,
        (destUpdateX + destUpdateWidth)
    );
    bool doRightMostColumn = false;
    if (destUpdateX2 == destWidth) {
        destUpdateX2--;
        doRightMostColumn = true;
    }
    destUpdateX2--;

    int32_t destUpdateY2 = DLIR_min(
         destHeight,
         (destUpdateY + destUpdateHeight)
    );
    bool doBottomMostRow = false;
    if (destUpdateY2 == destHeight) {
        destUpdateY2--;
        doBottomMostRow = true;
    }
    destUpdateY2--;

    // Pointers to src and dest's start, adjusting for initial X and Y, will
    // be passed to appropriate scalers.
    uint8_t * src = (uint8_t *)_src;
    uint8_t * dest = (uint8_t *)_dest;
    
    src += (srcPitch * srcY);
    src += (srcX << 2);

    dest += (destPitch * destY);
    dest += (destX << 2);
    dest += (destUpdateY * destPitch);

    // Handle bulk of scaling (usually, unless we have really narrow-or-small 
    // dests)
    DLIR_ResizeBilinear_ARGB8888_Body_Plain(
        src,
        srcWidth,
        srcHeight,
        srcPitch,
        dest,
        destWidth,
        destHeight,
        destPitch,

        destUpdateX,
        destUpdateY,
        destUpdateX2,
        destUpdateY2
    );

    //
    // Do special scaling for left-column and/or bottom-row (if and when they
    // fall on memory boundaries).  We don't want to read from invalid locations
    // offset from src's start!
    //

    if (doBottomMostRow) {
        DLIR_ResizeBilinear_ARGB8888_Bottom_Plain(
            src,
            srcWidth,
            srcHeight,
            srcPitch,
            dest + (destPitch * (destUpdateY2+1)),
            destWidth,
            destHeight,
            destPitch,

            destUpdateX,
            destUpdateY2+1,
            destUpdateX2,
            destUpdateY2+1
        );
    }

    if (doRightMostColumn) {
        DLIR_ResizeBilinear_ARGB8888_Right_Plain(
            src,
            srcWidth,
            srcHeight,
            srcPitch,
            dest,
            destWidth,
            destHeight,
            destPitch,

            destUpdateX2+1,
            destUpdateY,
            destUpdateX2+1,
            destUpdateY2
        );
    }

    if (doBottomMostRow || doRightMostColumn) {
        DLIR_ResizeBilinear_ARGB8888_BottomRight_Plain(
            src,
            srcWidth,
            srcHeight,
            srcPitch,
            dest + (destPitch * (destUpdateY2+1)),
            destWidth,
            destHeight,
            destPitch,

            destUpdateX2+1,
            destUpdateY2+1,
            destUpdateX2+1,
            destUpdateY2+1
        );
    }
    
#if 0 //__SSE4_1__
    {
        const uint32_t fracPart = (destWidth % 4);
        const uint32_t intPart = destWidth - fracPart;

        if (intPart > 0) {
            DLIR_ResizeBilinear_ARGB8888_Body_SSE41(
                src,
                srcWidth,
                srcHeight,
                srcPitch,
                dest,
                destWidth,
                destHeight,
                destPitch,
                0,
                destHeight,
                0,
                intPart
            );
        }
        if (fracPart > 0) {
            DLIR_ResizeBilinear_ARGB8888_Body_Plain(
                src,
                srcWidth,
                srcHeight,
                srcPitch,
                dest,
                destWidth,
                destHeight,
                destPitch,
                0,
                destHeight,
                intPart,
                intPart + fracPart
            );
        }
        return;
    }
#endif

}

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
