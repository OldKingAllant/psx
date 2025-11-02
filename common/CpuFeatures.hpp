#pragma once

//Found at https://stackoverflow.com/questions/6121792/how-to-check-if-a-cpu-supports-the-sse3-instruction-set
//From "Mysticial" . No license was present, so
//I'll leave only the forun link

#ifdef _WIN32
#include <intrin.h>
//  Windows
#define psx_cpuid(info, x)    __cpuidex(info, x, 0)
#else
//  GCC Intrinsics
#include <cpuid.h>
void psx_cpuid(int info[4], int InfoType) {
    __cpuid_count(InfoType, 0, info[0], info[1], info[2], info[3]);
}
#endif

namespace psx {
    struct SupportedCpuFeatures {
        //  Misc.
        bool HW_MMX;
        bool HW_x64;
        bool HW_ABM;      // Advanced Bit Manipulation
        bool HW_RDRAND;
        bool HW_BMI1;
        bool HW_BMI2;
        bool HW_ADX;
        bool HW_PREFETCHWT1;

        //  SIMD: 128-bit
        bool HW_SSE;
        bool HW_SSE2;
        bool HW_SSE3;
        bool HW_SSSE3;
        bool HW_SSE41;
        bool HW_SSE42;
        bool HW_SSE4a;
        bool HW_AES;
        bool HW_SHA;

        //  SIMD: 256-bit
        bool HW_AVX;
        bool HW_XOP;
        bool HW_FMA3;
        bool HW_FMA4;
        bool HW_AVX2;

        //  SIMD: 512-bit
        bool HW_AVX512F;    //  AVX512 Foundation
        bool HW_AVX512CD;   //  AVX512 Conflict Detection
        bool HW_AVX512PF;   //  AVX512 Prefetch
        bool HW_AVX512ER;   //  AVX512 Exponential + Reciprocal
        bool HW_AVX512VL;   //  AVX512 Vector Length Extensions
        bool HW_AVX512BW;   //  AVX512 Byte + Word
        bool HW_AVX512DQ;   //  AVX512 Doubleword + Quadword
        bool HW_AVX512IFMA; //  AVX512 Integer 52-bit Fused Multiply-Add
        bool HW_AVX512VBMI; //  AVX512 Vector Byte Manipulation Instructions

        static SupportedCpuFeatures Get() {
            SupportedCpuFeatures features{};

            int info[4];
            psx_cpuid(info, 0);
            int nIds = info[0];

            psx_cpuid(info, 0x80000000);
            unsigned nExIds = info[0];

            //  Detect Features
            if (nIds >= 0x00000001) {
                psx_cpuid(info, 0x00000001);
                features.HW_MMX = (info[3] & ((int)1 << 23)) != 0;
                features.HW_SSE = (info[3] & ((int)1 << 25)) != 0;
                features.HW_SSE2 = (info[3] & ((int)1 << 26)) != 0;
                features.HW_SSE3 = (info[2] & ((int)1 << 0)) != 0;

                features.HW_SSSE3 = (info[2] & ((int)1 << 9)) != 0;
                features.HW_SSE41 = (info[2] & ((int)1 << 19)) != 0;
                features.HW_SSE42 = (info[2] & ((int)1 << 20)) != 0;
                features.HW_AES = (info[2] & ((int)1 << 25)) != 0;

                features.HW_AVX = (info[2] & ((int)1 << 28)) != 0;
                features.HW_FMA3 = (info[2] & ((int)1 << 12)) != 0;

                features.HW_RDRAND = (info[2] & ((int)1 << 30)) != 0;
            }
            if (nIds >= 0x00000007) {
                psx_cpuid(info, 0x00000007);
                features.HW_AVX2 = (info[1] & ((int)1 << 5)) != 0;

                features.HW_BMI1 = (info[1] & ((int)1 << 3)) != 0;
                features.HW_BMI2 = (info[1] & ((int)1 << 8)) != 0;
                features.HW_ADX = (info[1] & ((int)1 << 19)) != 0;
                features.HW_SHA = (info[1] & ((int)1 << 29)) != 0;
                features.HW_PREFETCHWT1 = (info[2] & ((int)1 << 0)) != 0;

                features.HW_AVX512F = (info[1] & ((int)1 << 16)) != 0;
                features.HW_AVX512CD = (info[1] & ((int)1 << 28)) != 0;
                features.HW_AVX512PF = (info[1] & ((int)1 << 26)) != 0;
                features.HW_AVX512ER = (info[1] & ((int)1 << 27)) != 0;
                features.HW_AVX512VL = (info[1] & ((int)1 << 31)) != 0;
                features.HW_AVX512BW = (info[1] & ((int)1 << 30)) != 0;
                features.HW_AVX512DQ = (info[1] & ((int)1 << 17)) != 0;
                features.HW_AVX512IFMA = (info[1] & ((int)1 << 21)) != 0;
                features.HW_AVX512VBMI = (info[2] & ((int)1 << 1)) != 0;
            }
            if (nExIds >= 0x80000001) {
                psx_cpuid(info, 0x80000001);
                features.HW_x64 = (info[3] & ((int)1 << 29)) != 0;
                features.HW_ABM = (info[2] & ((int)1 << 5)) != 0;
                features.HW_SSE4a = (info[2] & ((int)1 << 6)) != 0;
                features.HW_FMA4 = (info[2] & ((int)1 << 16)) != 0;
                features.HW_XOP = (info[2] & ((int)1 << 11)) != 0;
            }

            return features;
        }
    };
}


