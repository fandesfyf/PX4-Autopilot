/****************************************************************************
 *
 *   Copyright (C) 2020 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#include <stdint.h>

static constexpr float SAMPLING_RES = 10;
static constexpr float SAMPLING_MIN_LAT = -80;
static constexpr float SAMPLING_MAX_LAT = 80;
static constexpr float SAMPLING_MIN_LON = -180;
static constexpr float SAMPLING_MAX_LON = 180;

static constexpr int LAT_DIM = 17;
static constexpr int LON_DIM = 37;


// Magnetic declination data in degrees
// Model: WMM-2020,
// Version: 0.5.1.11,
// Date: 2020.4563,
static constexpr const int8_t declination_table[17][37] {
	{  127, 117, 106,  96,  87,  78,  69,  61,  53,  46,  38,  30,  23,  15,   8,   0,  -7, -15, -23, -31, -40, -49, -58, -67, -77, -86, -97,-107,-119,-128,-128,-128,-128, 127, 127, 127, 127, },
	{   86,  78,  71,  66,  61,  56,  51,  46,  41,  35,  29,  23,  16,  10,   4,  -1,  -7, -14, -20, -28, -36, -44, -52, -60, -68, -77, -85, -93,-102,-113,-128,-128, 127, 127, 112,  96,  86, },
	{   48,  47,  45,  43,  42,  41,  39,  37,  33,  28,  23,  16,  10,   4,  -1,  -6, -10, -14, -20, -27, -34, -42, -50, -57, -63, -68, -72, -75, -76, -73, -61, -21,  27,  43,  48,  49,  48, },
	{   31,  31,  31,  31,  30,  30,  30,  29,  27,  24,  18,  11,   3,  -4,  -9, -12, -15, -17, -21, -27, -34, -41, -47, -53, -56, -58, -57, -52, -44, -30, -14,   2,  14,  22,  27,  30,  31, },
	{   22,  23,  23,  23,  23,  22,  22,  22,  22,  19,  13,   5,  -4, -12, -17, -20, -21, -22, -22, -25, -31, -37, -42, -45, -46, -44, -39, -31, -21, -11,  -3,   4,  10,  15,  19,  21,  22, },
	{   17,  17,  18,  18,  17,  17,  17,  16,  16,  13,   7,  -1, -11, -18, -22, -24, -25, -24, -22, -20, -22, -26, -30, -32, -32, -28, -23, -16,  -9,  -3,   0,   4,   7,  11,  14,  16,  17, },
	{   13,  14,  14,  14,  14,  13,  13,  12,  11,   9,   3,  -6, -14, -21, -24, -25, -24, -21, -16, -11,  -9, -11, -15, -17, -18, -16, -12,  -8,  -3,   0,   1,   3,   6,   8,  11,  12,  13, },
	{   11,  11,  11,  11,  11,  10,  10,   9,   8,   5,  -1,  -9, -16, -21, -23, -22, -18, -14,  -9,  -5,  -2,  -2,  -4,  -7,  -9,  -8,  -6,  -4,  -1,   1,   1,   2,   4,   7,   9,  10,  11, },
	{   10,  10,   9,   9,   9,   9,   8,   8,   6,   3,  -3, -10, -16, -20, -20, -17, -13,  -8,  -5,  -1,   1,   2,   0,  -2,  -4,  -4,  -3,  -2,   0,   0,   0,   1,   3,   5,   7,   9,  10, },
	{    9,   9,   9,   9,   9,   9,   8,   7,   5,   1,  -5, -11, -16, -18, -17, -14,  -9,  -5,  -2,   0,   2,   3,   2,   0,  -1,  -2,  -2,  -1,  -1,  -1,  -1,  -1,   0,   3,   6,   8,   9, },
	{    8,   9,   9,  10,  10,  10,  10,   8,   5,   0,  -6, -12, -15, -16, -14, -11,  -7,  -3,   0,   1,   3,   4,   3,   2,   1,   0,   0,  -1,  -1,  -2,  -3,  -4,  -2,   0,   3,   6,   8, },
	{    6,   9,  10,  11,  12,  12,  11,   9,   5,  -1,  -8, -13, -15, -15, -13, -10,  -6,  -2,   0,   2,   4,   5,   4,   4,   3,   2,   1,   0,  -1,  -4,  -6,  -7,  -6,  -3,   0,   3,   6, },
	{    4,   8,  11,  13,  14,  15,  13,  10,   5,  -2,  -9, -14, -16, -15, -13, -10,  -6,  -2,   1,   3,   4,   6,   6,   6,   6,   5,   4,   1,  -2,  -5,  -8, -10,  -9,  -7,  -3,   1,   4, },
	{    3,   7,  11,  14,  16,  17,  16,  12,   5,  -3, -12, -17, -19, -18, -15, -11,  -7,  -3,   0,   3,   6,   8,   9,  10,  11,  10,   7,   3,  -2,  -7, -11, -13, -12,  -9,  -6,  -1,   3, },
	{    2,   7,  12,  16,  18,  19,  18,  13,   5,  -7, -17, -23, -24, -23, -19, -15, -10,  -5,  -1,   4,   8,  11,  14,  17,  18,  17,  13,   7,  -1,  -9, -14, -15, -15, -12,  -8,  -3,   2, },
	{    1,   6,  12,  16,  20,  21,  20,  13,  -1, -17, -28, -33, -33, -30, -25, -20, -14,  -8,  -2,   4,  10,  16,  21,  25,  28,  28,  25,  17,   4,  -8, -15, -18, -17, -14,  -9,  -4,   1, },
	{   -2,   4,   9,  13,  15,  13,   5, -12, -32, -44, -48, -47, -43, -37, -31, -24, -16,  -9,  -1,   7,  14,  22,  29,  35,  41,  45,  47,  45,  35,  15,  -5, -15, -17, -16, -12,  -7,  -2, },
};

// Magnetic inclination data in degrees
// Model: WMM-2020,
// Version: 0.5.1.11,
// Date: 2020.4563,
static constexpr const int8_t inclination_table[17][37] {
	{  -78, -78, -77, -76, -75, -73, -72, -71, -70, -69, -68, -67, -67, -66, -66, -66, -65, -65, -66, -66, -66, -67, -68, -69, -70, -71, -73, -74, -75, -76, -77, -78, -79, -79, -79, -79, -78, },
	{  -81, -79, -77, -75, -74, -72, -69, -67, -65, -64, -62, -61, -60, -60, -60, -60, -60, -60, -60, -61, -61, -62, -64, -66, -68, -70, -73, -75, -78, -80, -83, -85, -86, -86, -84, -83, -81, },
	{  -78, -76, -74, -72, -70, -68, -65, -63, -60, -57, -55, -54, -54, -55, -56, -57, -58, -59, -59, -58, -59, -60, -61, -63, -66, -69, -73, -76, -80, -83, -86, -87, -86, -84, -82, -80, -78, },
	{  -72, -70, -68, -66, -64, -62, -60, -57, -54, -51, -49, -48, -49, -52, -55, -58, -60, -61, -61, -60, -59, -59, -61, -63, -66, -69, -73, -76, -78, -80, -81, -80, -79, -77, -76, -74, -72, },
	{  -64, -62, -60, -58, -57, -55, -53, -50, -47, -44, -41, -41, -43, -48, -53, -58, -62, -65, -66, -65, -63, -61, -61, -63, -65, -68, -71, -73, -74, -74, -73, -72, -71, -70, -68, -66, -64, },
	{  -55, -53, -51, -49, -46, -44, -42, -40, -37, -33, -31, -31, -35, -42, -49, -55, -61, -65, -67, -68, -66, -63, -61, -61, -62, -64, -65, -66, -66, -65, -64, -63, -62, -61, -59, -57, -55, },
	{  -42, -40, -37, -35, -32, -30, -28, -25, -22, -18, -15, -17, -23, -32, -41, -49, -55, -60, -63, -63, -61, -58, -54, -53, -53, -54, -55, -55, -54, -53, -51, -51, -50, -49, -47, -45, -42, },
	{  -25, -22, -20, -17, -15, -12, -10,  -7,  -3,   1,   3,   1,  -6, -17, -29, -38, -45, -48, -50, -50, -48, -44, -40, -38, -38, -38, -39, -39, -38, -36, -35, -35, -35, -34, -32, -29, -25, },
	{   -5,  -2,   1,   3,   5,   8,  10,  13,  16,  20,  21,  18,  11,   0, -11, -21, -27, -30, -30, -29, -27, -23, -19, -17, -16, -17, -17, -18, -17, -16, -15, -16, -16, -15, -13,  -9,  -5, },
	{   15,  18,  21,  22,  25,  27,  29,  31,  34,  36,  36,  34,  28,  19,   9,   1,  -4,  -5,  -5,  -4,  -1,   2,   6,   8,   8,   8,   7,   7,   7,   7,   7,   6,   5,   5,   7,  11,  15, },
	{   31,  34,  36,  38,  40,  42,  44,  46,  48,  49,  48,  46,  41,  35,  29,  23,  20,  19,  20,  21,  23,  26,  28,  30,  30,  30,  30,  29,  29,  29,  28,  27,  25,  25,  26,  28,  31, },
	{   43,  45,  47,  49,  51,  53,  55,  57,  58,  59,  58,  56,  52,  48,  45,  42,  40,  39,  40,  41,  43,  44,  46,  47,  47,  47,  47,  47,  47,  47,  46,  44,  42,  40,  40,  41,  43, },
	{   53,  54,  56,  57,  59,  61,  64,  66,  67,  68,  67,  65,  62,  59,  57,  55,  54,  54,  55,  56,  57,  58,  59,  59,  60,  60,  61,  61,  61,  60,  59,  57,  55,  53,  52,  52,  53, },
	{   62,  63,  64,  65,  67,  69,  71,  73,  74,  75,  74,  72,  70,  68,  67,  66,  65,  65,  65,  66,  66,  67,  68,  68,  69,  70,  70,  71,  71,  70,  69,  67,  65,  63,  62,  62,  62, },
	{   71,  71,  72,  73,  75,  76,  78,  80,  81,  81,  80,  79,  77,  75,  74,  73,  73,  73,  73,  73,  73,  74,  74,  75,  76,  77,  78,  79,  79,  78,  77,  75,  73,  72,  71,  71,  71, },
	{   79,  79,  80,  81,  82,  83,  84,  85,  86,  85,  85,  83,  82,  81,  80,  79,  79,  78,  78,  78,  79,  79,  80,  80,  81,  82,  83,  84,  84,  84,  83,  82,  81,  80,  79,  79,  79, },
	{   86,  86,  86,  87,  87,  88,  88,  88,  88,  88,  87,  86,  86,  85,  84,  84,  83,  83,  83,  83,  83,  84,  84,  85,  85,  86,  87,  87,  88,  88,  88,  88,  87,  87,  86,  86,  86, },
};

// Magnetic strength data in micro-Tesla or centi-Gauss
// Model: WMM-2020,
// Version: 0.5.1.11,
// Date: 2020.4563,
static constexpr const int8_t strength_table[17][37] {
	{   61,  60,  59,  58,  57,  56,  55,  54,  53,  51,  50,  49,  48,  48,  47,  46,  46,  46,  46,  47,  47,  48,  50,  51,  52,  54,  55,  57,  58,  59,  60,  61,  61,  61,  61,  61,  61, },
	{   63,  62,  60,  59,  57,  55,  53,  51,  49,  46,  44,  43,  41,  40,  39,  38,  37,  37,  38,  38,  40,  41,  44,  46,  49,  52,  55,  58,  61,  63,  64,  65,  66,  66,  65,  64,  63, },
	{   62,  60,  58,  56,  54,  52,  49,  46,  43,  40,  38,  35,  34,  32,  31,  30,  30,  30,  30,  31,  33,  35,  38,  42,  46,  51,  55,  59,  62,  64,  66,  67,  67,  66,  65,  64,  62, },
	{   59,  56,  54,  52,  49,  47,  44,  41,  38,  35,  32,  29,  27,  27,  26,  26,  25,  25,  25,  26,  28,  30,  34,  39,  44,  49,  54,  58,  61,  64,  65,  66,  65,  64,  63,  61,  59, },
	{   54,  52,  49,  47,  44,  42,  40,  37,  34,  30,  27,  25,  24,  24,  24,  24,  24,  24,  24,  24,  25,  28,  32,  37,  43,  48,  53,  56,  59,  61,  62,  62,  62,  60,  59,  56,  54, },
	{   49,  46,  44,  42,  40,  37,  35,  33,  30,  28,  25,  23,  22,  23,  23,  24,  25,  25,  25,  26,  26,  28,  32,  36,  42,  47,  51,  54,  56,  57,  57,  57,  56,  55,  53,  51,  49, },
	{   43,  41,  39,  37,  35,  33,  32,  30,  28,  26,  24,  23,  22,  23,  24,  25,  26,  27,  28,  29,  29,  30,  32,  36,  40,  45,  48,  51,  52,  52,  52,  51,  50,  49,  47,  45,  43, },
	{   38,  36,  35,  33,  32,  31,  30,  29,  28,  27,  26,  25,  24,  24,  25,  26,  28,  30,  31,  32,  32,  32,  33,  35,  39,  42,  45,  46,  47,  46,  45,  45,  44,  43,  41,  40,  38, },
	{   34,  33,  32,  32,  31,  31,  31,  30,  30,  30,  29,  28,  27,  27,  27,  28,  29,  31,  32,  33,  33,  33,  34,  35,  38,  40,  41,  43,  43,  42,  41,  40,  39,  38,  36,  35,  34, },
	{   33,  33,  32,  32,  33,  33,  34,  34,  35,  35,  34,  33,  31,  30,  30,  30,  31,  32,  33,  34,  35,  35,  36,  37,  39,  40,  41,  42,  42,  41,  40,  39,  37,  36,  34,  33,  33, },
	{   34,  34,  34,  35,  36,  37,  38,  40,  40,  41,  40,  38,  37,  35,  34,  34,  35,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  45,  45,  43,  41,  39,  37,  35,  34,  34, },
	{   37,  37,  38,  39,  40,  42,  44,  46,  47,  47,  46,  45,  43,  41,  40,  39,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  50,  50,  51,  50,  49,  46,  44,  41,  39,  38,  37, },
	{   42,  42,  43,  44,  46,  48,  50,  51,  52,  53,  52,  50,  48,  47,  45,  45,  44,  44,  45,  46,  47,  48,  49,  50,  52,  53,  55,  56,  56,  56,  54,  52,  49,  46,  44,  43,  42, },
	{   48,  48,  49,  50,  52,  53,  55,  56,  57,  57,  56,  55,  53,  51,  50,  49,  48,  48,  48,  49,  50,  51,  52,  53,  55,  57,  58,  60,  60,  60,  58,  56,  54,  52,  50,  49,  48, },
	{   54,  54,  54,  55,  56,  57,  58,  58,  59,  58,  58,  57,  56,  54,  53,  52,  51,  51,  51,  51,  52,  53,  54,  55,  57,  59,  60,  61,  62,  62,  61,  59,  58,  56,  55,  54,  54, },
	{   57,  57,  57,  57,  58,  58,  58,  58,  58,  58,  57,  57,  56,  55,  55,  54,  53,  53,  53,  53,  54,  54,  55,  56,  58,  59,  60,  60,  61,  61,  61,  60,  59,  59,  58,  58,  57, },
	{   58,  58,  58,  57,  57,  57,  57,  57,  57,  57,  56,  56,  56,  56,  55,  55,  55,  55,  55,  55,  55,  56,  56,  57,  57,  58,  58,  58,  59,  59,  59,  59,  59,  58,  58,  58,  58, },
};
