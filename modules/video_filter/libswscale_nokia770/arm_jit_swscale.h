/*
 * Fast JIT powered scaler for ARM
 *
 * Copyright (C) 2007 Siarhei Siamashka <ssvb@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef ARM_JIT_SWSCALE_H
#define ARM_JIT_SWSCALE_H

#include <stdint.h>

struct SwsContextArmJit;

struct SwsContextArmJit *sws_arm_jit_create_omapfb_yuv422_scaler(int source_w, int source_h, int target_w, int target_h, int quality);
struct SwsContextArmJit *sws_arm_jit_create_omapfb_yuv420_scaler(int source_w, int source_h, int target_w, int target_h, int quality);
struct SwsContextArmJit *sws_arm_jit_create_omapfb_yuv420_scaler_armv6(int source_w, int source_h, int target_w, int target_h, int quality);

int sws_arm_jit_scale(struct SwsContextArmJit *context, uint8_t* src[], int srcStride[], int y, int h, uint8_t* dst[], int dstStride[]);

void sws_arm_jit_free(struct SwsContextArmJit *context);

#endif
