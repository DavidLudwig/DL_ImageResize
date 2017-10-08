; x86_4.nasm
; zlib-licensed, Oct 8 2017, David Ludwig, dludwig@pobox.com. See "COPYING" statement at the end of this file.

global _BilinearScale4_x86

; section .rodata
;   align 16
;   ARGB_TO_SOA: dq 0x02060a0e03070b0f, 0x0004080c0105090d


section .text

%define PARAM_START         0x08

%define FIXED 10
%define FIXED_2X 20
%define FRAC_BITS 0x3FF
%define FIXED_1 0x400

_BilinearScale4_x86:
    ; extern "C" void BilinearScale3_x86(
    ;     uint32_t * src,
    ;     uint32_t srcWidth,
    ;     uint32_t srcHeight,
    ;     uint32_t * dest,
    ;     uint32_t destWidth,
    ;     uint32_t destHeight
    ; );

    %define src                 [ebp + PARAM_START + 0x00]
    %define srcWidth            [ebp + PARAM_START + 0x04]
    %define srcHeight           [ebp + PARAM_START + 0x08]
    %define dest                [ebp + PARAM_START + 0x0c]
    %define destWidth           [ebp + PARAM_START + 0x10]
    %define destHeight          [ebp + PARAM_START + 0x14]

    ; Mac OS X compatible prologue
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    ; %define stackSpace          0x48
    %define stackSpace          0x14
    sub esp, stackSpace

    %define ratioDestXtoSrcX    [ebp - 0x10]
    %define ratioDestYtoSrcY    [ebp - 0x14]
    %define srcY                [ebp - 0x18]
    %define srcX                [ebp - 0x1c]
   ;%define diffY               [ebp - ]
   ;%define diffX               [ebp - ]        ; now edi
    %define destY               [ebp - 0x20]
   ;%define destX               [ebp - 0x28]
    ;%define srcIndex           [ebp - ]
    ;%define s0                  [ebp - ]
    ;%define s1                  [ebp - ]
    ;%define s2                  [ebp - ]
    ;%define s3                  [ebp - ]
    ;%define srcPitch            [ebp - 0x24]
    ;%define factor0             [ebp - ]
    ;%define factor1             [ebp - ]
    ;%define factor2             [ebp - ]
    ;%define factor3             [ebp - ]

    ; uint32_t srcPitch = (srcWidth * 4)
    mov esi, srcWidth               ; eax = srcWidth
    shl esi, 2                      ; eax *= 4
    ;mov dword srcPitch, eax         ; srcPitch = eax

    ; fixed ratioDestXtoSrcX = (((srcWidth - 1)<<FIXED) / destWidth);
    mov eax, srcWidth               ; eax = srcWidth
    mov ecx, destWidth              ; ecx = destWidth
    sub eax, 1                      ; eax -= 1
    shl eax, FIXED                  ; eax <<= FIXED
    mov edx, 0                      ; setup for div instruction
    div ecx                         ; eax = eax / ecx; edx = eax % ecx
    mov dword ratioDestXtoSrcX, eax ; ratioDestXtoSrcX = eax

    ; fixed ratioDestYtoSrcY = (((srcHeight - 1)<<FIXED) / destHeight);
    mov eax, srcHeight              ; eax = srcHeight
    mov ecx, destHeight             ; ecx = destHeight
    sub eax, 1                      ; eax -= 1
    shl eax, FIXED                  ; eax <<= FIXED
    mov edx, 0                      ; setup for div instruction
    div ecx                         ; eax = eax / ecx; edx = eax % ecx
    mov dword ratioDestYtoSrcY, eax ; ratioDestYtoSrcY = eax

    ; destY = 0
    mov dword destY, 0              ; destY = 0
.LoopDestY:
    ; srcY = (((fixedbig)ratioDestYToSrcY * (fixedbig)(destY<<FIXED))>>(FIXED*2);
    mov eax, destY                  ; eax = destY
    shl eax, FIXED                  ; eax <<= FIXED
    mov ebx, ratioDestYtoSrcY       ; ebx = ratioDestYtoSrcY
    mul ebx                         ; eax *= ebx
    shr eax, FIXED_2X               ; eax >>= FIXED_2X
    mov dword srcY, eax             ; srcY = eax

    ; diffY = (((fixedbig)ratioDestYToSrcY * (fixedbig)(destY<<FIXED))>>FIXED) & FRAC_BITS;
    mov eax, destY                      ; eax = destY
    shl eax, FIXED                  ; eax <<= FIXED
    mov ebx, ratioDestYtoSrcY       ; ebx = ratioDestYtoSrcY
    mul ebx                         ; eax *= ebx
    shr eax, FIXED                  ; eax >>= FIXED
    and eax, FRAC_BITS              ; eax &= FRAC_BITS
    ;mov dword diffY, eax            ; diffY = eax

    ; HAVE:
    ;	eax = diffY
    mov edx, FIXED_1
    sub edx, eax        ; edx = 1 - diffY

    ; xmm5 = diffY, diffY, 1-diffY, 1-diffY
    pinsrd xmm5, eax, 3
    pinsrd xmm5, eax, 2
    pinsrd xmm5, edx, 1
    pinsrd xmm5, edx, 0

    ; destX = 0
    ;mov dword destX, 0              ; destX = 0
    xor edi, edi
    ;mov edi, 0
.LoopDestX:

    ; REGS
    ;   eax - RESERVED for mul and div use
    ;   ebx
    ;   ecx
    ;   edx - RESERVED for div use
    ;   esi - srcPitch
    ;   edi - destX
    ;  xmm0
    ;  xmm1
    ;  xmm2
    ;  xmm3
    ;  xmm4
    ;  xmm5 - ASSIGNED: diffY, diffY, 1-diffY, 1-diffY
    ;  xmm6
    ;  xmm7

    ; srcX = (((fixedbig)ratioDestXToSrcX * (fixedbig)(destX<<FIXED))>>(FIXED*2);
    ;mov eax, destX                  ; eax = destX
    mov eax, edi
    shl eax, FIXED                  ; eax <<= FIXED
    mov ebx, ratioDestXtoSrcX       ; ebx = ratioDestXtoSrcX
    mul ebx                         ; eax *= ebx

    shr eax, FIXED_2X               ; eax >>= FIXED_2X
    mov dword srcX, eax             ; srcX = eax

    ; diffX = (((fixedbig)ratioDestXToSrcX * (fixedbig)(destX<<FIXED))>>FIXED) & FRAC_BITS;
    ;mov eax, destX                  ; eax = destX
    mov eax, edi
    shl eax, FIXED                  ; eax <<= FIXED
    mov ebx, ratioDestXtoSrcX       ; ebx = ratioDestXtoSrcX
    mul ebx                         ; eax *= ebx

    shr eax, FIXED                  ; eax >>= FIXED
    and eax, FRAC_BITS              ; eax &= FRAC_BITS
    ;mov dword diffX, eax            ; diffX = eax

    ;mov eax, diffX      ; eax = diffX
    mov ebx, FIXED_1
    sub ebx, eax        ; ebx = 1 - diffX

    ; xmm4 = diffX, 1-diffX, diffX, 1-diffX
    pinsrd xmm4, eax, 3
    pinsrd xmm4, ebx, 2
    pinsrd xmm4, eax, 1
    pinsrd xmm4, ebx, 0


    ; eax (srcIndex) = (srcY * srcPitch) + (srcX * 4);
    mov eax, srcY                   ; eax = srcY
    ;mov ebx, srcPitch               ; ebx = srcPitch
    mov ecx, srcX                   ; ecx = srcX
    mul esi                         ; eax (srcY) *= esi (srcPitch)
    shl ecx, 2                      ; ecx (srcX) *= 4
    add eax, ecx                    ; eax (srcY -> srcIndex) += ecx (srcX)
    ;mov dword srcIndex, eax        ; srcIndex = eax

    mov ebx, src                    ; ebx = src
    ;mov ecx, srcPitch               ; ecx = srcPitch
    ; mov edx, [ebx + eax]          ; edx = src[srcIndex]
    ; mov s0, edx                   ; s0 = edx

    add ebx, eax        ; ebx (src) += eax (srcIndex)
    ; [ebx] is first pixel of src
    movq xmm0, [ebx]    ; xmm0 = s0,s1,0,0 according to lldb-default, and in BGRA
                        ;  or 0,0,s1,s0 according to single hex value, and in ARGB

    add ebx, esi        ; ebx (src) += esi (srcPitch)
    ; [ebx] is 2nd row of src
    movq xmm1, [ebx]    ; xmm1 = 0,0,s3,s2 according to single hex value, and in ARGB
    pslldq xmm1, 8      ; xmm1 = s3,s2,0,0
    por xmm0, xmm1      ; xmm0 = s3,s2,s1,s0

    ; xmm0, current, value :      30313233 20212223 10111213 00010203
    ; xmm0, current, format:      A3R3G3B3 A2R2G2B2 A1R1G1B1 A0R0G0B0
    ; xmm0, current = s3,s2,s1,s0 in ARGB

    ; xmm0 = 0x ff0000ff ff00ff00 ff00ff00 ffff0000
    ;           BLUE     GREEN    GREEN    RED

    ; xmm1 to format:             RRRR3333 RRRR2222 RRRR1111 RRRR0000
    movdqa xmm1, xmm0   ; xmm1  = A3R3G3B3 A2R2G2B2 A1R1G1B1 A0R0G0B0
    pslld xmm1, 8       ; xmm1  = R3G3B300 R2G2B200 R1G1B100 R0G0B000
    psrld xmm1, 24      ; xmm1  = 000000R3 000000R2 000000R1 000000R0
    pslld xmm1, FIXED   ; xmm1  = {r3, r2, r1, r0}

    ; xmm2 to format:             GGGG3333 GGGG2222 GGGG1111 GGGG0000
    movdqa xmm2, xmm0   ; xmm2  = A3R3G3B3 A2R2G2B2 A1R1G1B1 A0R0G0B0
    pslld xmm2, 16      ; xmm2  = G3B30000 G2B20000 G1B10000 G0B00000
    psrld xmm2, 24      ; xmm2  = 000000G3 000000G2 000000G1 000000G0
    pslld xmm2, FIXED   ; xmm2  = {g3, g2, g1, g0}

    ; xmm3 to format:             BBBB3333 BBBB2222 BBBB1111 BBBB0000
    movdqa xmm3, xmm0   ; xmm3  = A3R3G3B3 A2R2G2B2 A1R1G1B1 A0R0G0B0
    pslld xmm3, 24      ; xmm3  = B3000000 B2000000 B1000000 B0000000
    psrld xmm3, 24      ; xmm3  = 000000B3 000000B2 000000B1 000000B0
    pslld xmm3, FIXED   ; xmm3  = {b3, b2, b1, b0}

    ; HAVE:
    ;   xmm1 = {r3, r2, r1, r0}
    ;   xmm2 = {g3, g2, g1, g0}
    ;   xmm3 = {b3, b2, b1, b0}
    ;   xmm4 = diffX, 1-diffX, diffX, 1-diffX
    ;   xmm5 = diffY, diffY, 1-diffY, 1-diffY

    ; fixedbig ffactor3 = ((             diffX) * (             diffY)) >> FIXED;
    ; fixedbig ffactor2 = (((1<<FIXED) - diffX) * (             diffY)) >> FIXED;
    ; fixedbig ffactor1 = ((             diffX) * ((1<<FIXED) - diffY)) >> FIXED;
    ; fixedbig ffactor0 = (((1<<FIXED) - diffX) * ((1<<FIXED) - diffY)) >> FIXED;

    ; xmm4 = {factor3, factor2, factor1, factor0}
    ;
    ; aka.
    ; xmm4 = ({diffX, 1-diffX, diffX, 1-diffX} * {diffY, diffY, 1-diffY, 1-diffY}) >> FIXED
    ;
    ; aka.
    ; xmm4 = ( |diffX  |   |diffY  | )    |FIXED|
    ;        ( |1-diffX|   |diffY  | )    |FIXED|
    ;        ( |diffX  | * |1-diffY| ) >> |FIXED|
    ;        ( |1-diffX|   |1-diffY| )    |FIXED|
    ;
    pmulld xmm4, xmm5
    psrld xmm4, FIXED

    ; HAVE:
    ;   xmm1 = {r3, r2, r1, r0}
    ;   xmm2 = {g3, g2, g1, g0}
    ;   xmm3 = {b3, b2, b1, b0}
    ;   xmm4 = {factor3, factor2, factor1, factor0}

    ; WANT:
    ; red = ( \
    ;     (((s0>>16) & 0xff)<<FIXED) * factor0 +
    ;     (((s1>>16) & 0xff)<<FIXED) * factor1 +
    ;     (((s2>>16) & 0xff)<<FIXED) * factor2 +
    ;     (((s3>>16) & 0xff)<<FIXED) * factor3
    ; ) >> FIXED;

    pmulld xmm1, xmm4
    pmulld xmm2, xmm4
    pmulld xmm3, xmm4
    psrld xmm1, FIXED
    psrld xmm2, FIXED
    psrld xmm3, FIXED

    ; HAVE:
    ;   xmm1: {r3 * factor3, r2 * factor2, r1 * factor1, r0 * factor0}
    ;   xmm2: {g3 * factor3, g2 * factor2, g1 * factor1, g0 * factor0}
    ;   xmm3: {b3 * factor3, b2 * factor2, b1 * factor1, b0 * factor0}

    ; xmm1: {rFinal, rFinal, rFinal, rFinal}
    phaddd xmm1, xmm1
    phaddd xmm1, xmm1

    ; xmm2: {gFinal, gFinal, gFinal, gFinal}
    phaddd xmm2, xmm2
    phaddd xmm2, xmm2

    ; xmm3: {bFinal, bFinal, bFinal, bFinal}
    phaddd xmm3, xmm3
    phaddd xmm3, xmm3

    ; eax = rFinal
    ; ebx = gFinal
    ; ecx = bFinal
    movd eax, xmm1
    movd ebx, xmm2
    movd ecx, xmm3

    ; eax, ebx, ecx: 32-bit fixed-point -> 8-bit values
    shr eax, FIXED
    shr ebx, FIXED
    shr ecx, FIXED
    and eax, 0xFF
    and ebx, 0xFF
    and ecx, 0xFF

    ; HAVE:
    ;   eax: 000000RR
    ;   ebx: 000000GG
    ;   ecx: 000000BB
    ;
    ; WANT:
    ;   eax: 00RR0000
    ;   ebx: 0000GG00
    ;   ecx: 000000BB
    shl eax, 16
    shl ebx, 8

    ; WANT:
    ;   edx: AARRGGBB
    mov edx, 0xFF000000     ; fully-opaque alpha
    or edx, eax
    or edx, ebx
    or edx, ecx

    ; *dest++ = edx
    ; mov edx, 0xFFFF0000
    mov ebx, dest           ; ebx = dest
    mov [ebx], edx          ; *ebx (dest) = edx (colorFinal)
    add ebx, 4              ; ebx += 4
    mov dest, ebx           ; dest = ebx

    ; ++destX
    mov ebx, destWidth
    ;mov eax, destX
    ;inc eax
    ;mov destX, eax
    inc edi
    cmp edi, ebx
    jne .LoopDestX

    ; ++destY
    mov ebx, destHeight
    mov eax, destY
    inc eax
    mov destY, eax
    cmp eax, ebx
    jne .LoopDestY


.done:

    ; Mac OS X compatible epilogue
    add esp, stackSpace
    pop edi
    pop esi
    pop ebx
    mov esp, ebp
    pop ebp
    ret


;
; COPYING
;
; Copyright (c) 2017 David Lee Ludwig
;
; This software is provided 'as-is', without any express or implied
; warranty. In no event will the authors be held liable for any damages
; arising from the use of this software.
;
; Permission is granted to anyone to use this software for any purpose,
; including commercial applications, and to alter it and redistribute it
; freely, subject to the following restrictions:
;
; 1. The origin of this software must not be misrepresented; you must not
;    claim that you wrote the original software. If you use this software
;    in a product, an acknowledgment in the product documentation would be
;    appreciated but is not required.
; 2. Altered source versions must be plainly marked as such, and must not be
;    misrepresented as being the original software.
; 3. This notice may not be removed or altered from any source distribution.
;
;     David Lee Ludwig <dludwig@pobox.com>
;
