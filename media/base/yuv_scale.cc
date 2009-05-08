// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/yuv_scale.h"

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef _DEBUG
#include "base/logging.h"
#else
#define DCHECK(a)
#endif

namespace media {

static void ScaleYV12ToRGB32Row(const uint8* y_buf,
                                const uint8* u_buf,
                                const uint8* v_buf,
                                uint8* rgb_buf,
                                int width,
                                int scaled_width);
static void HalfYV12ToRGB32Row(const uint8* y_buf,
                               const uint8* u_buf,
                               const uint8* v_buf,
                               uint8* rgb_buf,
                               int width);

extern "C" void ConvertYV12ToRGB32Row(const uint8* y_buf,
                                      const uint8* u_buf,
                                      const uint8* v_buf,
                                      uint8* rgb_buf,
                                      size_t width);

// Scale a frame of YV12 (aka YUV420) to 32 bit ARGB.
void ScaleYV12ToRGB32(const uint8* y_buf,
                      const uint8* u_buf,
                      const uint8* v_buf,
                      uint8* rgb_buf,
                      int width,
                      int height,
                      int scaled_width,
                      int scaled_height,
                      int y_pitch,
                      int uv_pitch,
                      int rgb_pitch,
                      Rotate view_rotate) {
  // Rotations that start at right side of image
  if ((view_rotate == ROTATE_180) ||
      (view_rotate == ROTATE_270) ||
      (view_rotate == MIRROR_ROTATE_0) ||
      (view_rotate == MIRROR_ROTATE_90)) {
    y_buf += width - 1;
    u_buf += width / 2 - 1;
    v_buf += width / 2 - 1;
    width = -width;
  }
  // Rotations that start at bottom of image
  if ((view_rotate == ROTATE_90) ||
      (view_rotate == ROTATE_180) ||
      (view_rotate == MIRROR_ROTATE_90) ||
      (view_rotate == MIRROR_ROTATE_180)) {
    y_buf += (height - 1) * y_pitch;
    u_buf += (height / 2 - 1) * uv_pitch;
    v_buf += (height / 2 - 1) * uv_pitch;
    height = -height;
  }
  // Only these rotations are implemented.
  DCHECK((view_rotate == ROTATE_0) ||
         (view_rotate == ROTATE_180) ||
         (view_rotate == MIRROR_ROTATE_0) ||
         (view_rotate == MIRROR_ROTATE_180));

#ifdef _OPENMP
#pragma omp parallel for
#endif
  for (int y = 0; y < scaled_height; ++y) {
    uint8* dest_pixel = rgb_buf + y * rgb_pitch;
    int scaled_y = (y * height / scaled_height);

    const uint8* y_ptr = y_buf + scaled_y * y_pitch;
    const uint8* u_ptr = u_buf + scaled_y / 2 * uv_pitch;
    const uint8* v_ptr = v_buf + scaled_y / 2 * uv_pitch;

    if (scaled_width == width) {
      ConvertYV12ToRGB32Row(y_ptr, u_ptr, v_ptr,
                            dest_pixel, scaled_width);
    } else if (scaled_width == (width / 2)) {
      HalfYV12ToRGB32Row(y_ptr, u_ptr, v_ptr,
                         dest_pixel, scaled_width);
    } else {
      ScaleYV12ToRGB32Row(y_ptr, u_ptr, v_ptr,
                          dest_pixel, width, scaled_width);
    }
  }
}

// Scale a frame of YV16 (aka YUV422) to 32 bit ARGB.
void ScaleYV16ToRGB32(const uint8* y_buf,
                      const uint8* u_buf,
                      const uint8* v_buf,
                      uint8* rgb_buf,
                      int width,
                      int height,
                      int scaled_width,
                      int scaled_height,
                      int y_pitch,
                      int uv_pitch,
                      int rgb_pitch,
                      Rotate view_rotate) {
  // Rotations that start at right side of image
  if ((view_rotate == ROTATE_180) ||
      (view_rotate == ROTATE_270) ||
      (view_rotate == MIRROR_ROTATE_0) ||
      (view_rotate == MIRROR_ROTATE_90)) {
    y_buf += width - 1;
    u_buf += width / 2 - 1;
    v_buf += width / 2 - 1;
    width = -width;
  }
  // Rotations that start at bottom of image
  if ((view_rotate == ROTATE_90) ||
      (view_rotate == ROTATE_180) ||
      (view_rotate == MIRROR_ROTATE_90) ||
      (view_rotate == MIRROR_ROTATE_180)) {
    y_buf += (height - 1) * y_pitch;
    u_buf += (height - 1) * uv_pitch;
    v_buf += (height - 1) * uv_pitch;
    height = -height;
  }
  // Only these rotations are implemented.
  DCHECK((view_rotate == ROTATE_0) ||
         (view_rotate == ROTATE_180) ||
         (view_rotate == MIRROR_ROTATE_0) ||
         (view_rotate == MIRROR_ROTATE_180));
#ifdef _OPENMP
#pragma omp parallel for
#endif
  for (int y = 0; y < scaled_height; ++y) {
    uint8* dest_pixel = rgb_buf + y * rgb_pitch;
    int scaled_y = (y * height / scaled_height);
    const uint8* y_ptr = y_buf + scaled_y * y_pitch;
    const uint8* u_ptr = u_buf + scaled_y * uv_pitch;
    const uint8* v_ptr = v_buf + scaled_y * uv_pitch;

    if (scaled_width == (width / 2)) {
      HalfYV12ToRGB32Row(y_ptr, u_ptr, v_ptr,
                         dest_pixel, scaled_width);
    } else {
      ScaleYV12ToRGB32Row(y_ptr, u_ptr, v_ptr,
                          dest_pixel, width, scaled_width);
    }
  }
}

// Reference version of YUV Scaler.
static const int kClipTableSize = 256;
static const int kClipOverflow = 128;

static uint8 g_rgb_clip_table[kClipOverflow
                            + kClipTableSize
                            + kClipOverflow] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 128 underflow values
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // cliped to 0.
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,  // Unclipped values.
  0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
  0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
  0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
  0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
  0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
  0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
  0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
  0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
  0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
  0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
  0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
  0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
  0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
  0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
  0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
  0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
  0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
  0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
  0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
  0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
  0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
  0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
  0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
  0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
  0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
  0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
  0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
  0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 128 overflow values
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // cliped to 255.
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

// Clip an rgb channel value to 0..255 range.
// Source is signed fixed point 8.8.
// Table allows for values to underflow or overflow by 128.
// Therefore source range is -128 to 384.
// Output clips to unsigned 0 to 255.
static inline uint32 clip(int32 value) {
//  DCHECK(((value >> 8) + kClipOverflow) >= 0);
//  DCHECK(((value >> 8) + kClipOverflow) <
//         (kClipOverflow + kClipTableSize + kClipOverflow));
  return static_cast<uint32>(g_rgb_clip_table[((value) >> 8) + kClipOverflow]);
}

// 28.4 fixed point is used.  A shift by 4 isolates the integer.
// A shift by 5 is used to further subsample the chrominence channels.
// & 15 isolates the fixed point fraction.  >> 2 to get the upper 2 bits,
// for 1/4 pixel accurate interpolation.

static void ScaleYV12ToRGB32Row(const uint8* y_buf,
                           const uint8* u_buf,
                           const uint8* v_buf,
                           uint8* rgb_buf,
                           int width,
                           int scaled_width) {
  int scaled_dx = width * 16 / scaled_width;
  int scaled_x = 0;
  for (int32 x = 0; x < scaled_width; ++x) {
    uint8 u = u_buf[scaled_x >> 5];
    uint8 v = v_buf[scaled_x >> 5];
    int32 d = static_cast<int32>(u) - 128;
    int32 e = static_cast<int32>(v) - 128;

    int32 cb =   (516 * d + 128);
    int32 cg = (- 100 * d - 208 * e + 128);
    int32 cr =             (409 * e + 128);

    uint8 y0 = y_buf[scaled_x >> 4];
    uint8 y1 = y_buf[(scaled_x >> 4) + 1];
    switch ((scaled_x & 15) >> 2) {
      case 1:  // 75% first pixel, 25% second pixel.
        y0 = (y0 + y0 + y0 + y1) >> 2;
        break;
      case 2:  // 50/50 blend
        y0 = (y0 + y1) >> 1;
        break;
      case 3:  // 25% first pixel, 75% second pixel.
        y0 = (y0 + y1 + y1 + y1) >> 2;
        break;
      default:
      case 0:  // 100% first pixel.
        break;
    }

    int32 C298a = ((static_cast<int32>(y0) - 16) * 298 + 128);
    *reinterpret_cast<uint32*>(rgb_buf) = clip(C298a + cb) |
                                         (clip(C298a + cg) << 8) |
                                         (clip(C298a + cr) << 16) |
                                          0xff000000;

    rgb_buf += 4;
    scaled_x += scaled_dx;
  }
}


static void HalfYV12ToRGB32Row(const uint8* y_buf,
                        const uint8* u_buf,
                        const uint8* v_buf,
                        uint8* rgb_buf,
                        int width) {
  for (int32 x = 0; x < width; ++x) {
    uint8 u = u_buf[x];
    uint8 v = v_buf[x];
    int32 d = static_cast<int32>(u) - 128;
    int32 e = static_cast<int32>(v) - 128;

    int32 cb =   (516 * d + 128);
    int32 cg = (- 100 * d - 208 * e + 128);
    int32 cr =             (409 * e + 128);

    uint8 y0 = y_buf[x * 2 + 0];
    uint8 y1 = y_buf[x * 2 + 1];
    int32 C298a = ((static_cast<int32>((y0 + y1) >> 1) - 16) * 298 + 128);
    *reinterpret_cast<uint32*>(rgb_buf) = clip(C298a + cb) |
                                         (clip(C298a + cg) << 8) |
                                         (clip(C298a + cr) << 16) |
                                         0xff000000;

    rgb_buf += 4;
  }
}

}  // namespace media

