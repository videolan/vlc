/*****************************************************************************
 * VLCKeyboardBlacklightControl.m: MacBook keyboard backlight control for VLC
 *****************************************************************************
 * Copyright (C) 2015 VLC authors and VideoLAN
 *
 *
 * Authors: Maxime Mouchet <max@maxmouchet.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#import "VLCKeyboardBacklightControl.h"
#import <IOKit/IOKitLib.h>

enum {
    kGetSensorReadingID = 0, // getSensorReading(int *, int *)
    kGetLEDBrightnessID = 1, // getLEDBrightness(int, int *)
    kSetLEDBrightnessID = 2, // setLEDBrightness(int, int, int *)
    kSetLEDFadeID = 3        // setLEDFade(int, int, int, int *)
};

@implementation VLCKeyboardBacklightControl {
    io_connect_t dataPort;
}

static float lastBrightnessLevel;

- (id)init {
    dataPort = [self getDataPort];
    lastBrightnessLevel = [self getBrightness];
    return self;
}

- (void)dealloc {
    if (dataPort)
        IOServiceClose(dataPort);
}

- (io_connect_t)getDataPort {
    if (dataPort) return dataPort;

    io_service_t serviceObject = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("AppleLMUController"));

    if (!serviceObject) return 0;

    kern_return_t kr = IOServiceOpen(serviceObject, mach_task_self(), 0, &dataPort);
    IOObjectRelease(serviceObject);

    if (kr != KERN_SUCCESS) return 0;

    return dataPort;
}

- (void)setBrightness:(float)brightness {
    if (!dataPort) return;

    UInt32 inputCount = 2;
    UInt64 inputValues[2] = { 0, brightness * 0xfff };

    UInt32 outputCount = 1;
    UInt64 outputValues[1];

    kern_return_t kr = IOConnectCallScalarMethod(dataPort,
                                                 kSetLEDBrightnessID,
                                                 inputValues,
                                                 inputCount,
                                                 outputValues,
                                                 &outputCount);

    if (kr != KERN_SUCCESS) return;
}

- (float)getBrightness {
    if (!dataPort) return 0.0;

    uint32_t inputCount = 1;
    uint64_t inputValues[1] = { 0 };

    uint32_t outputCount = 1;
    uint64_t outputValues[1];

    kern_return_t kr = IOConnectCallScalarMethod(dataPort,
                                                 kGetLEDBrightnessID,
                                                 inputValues,
                                                 inputCount,
                                                 outputValues,
                                                 &outputCount);

    float brightness = -1.0;
    if (kr == KERN_SUCCESS) {
        brightness = (float)outputValues[0] / 0xfff;
    }

    return brightness;
}

- (void)lightsUp {
    if (!dataPort) return;

    @synchronized(self) {
        float start = [self getBrightness];
        float target = lastBrightnessLevel;

        // Don't do anything if the user has put
        // backlight on again during playback.
        if (start != 0) return;

        for (float i = start; i <= target; i += 0.08) {
            [self setBrightness:i];
            [NSThread sleepForTimeInterval:0.05];
        }

        [self setBrightness:target];
    }
}

- (void)lightsDown {
    if (!dataPort) return;

    @synchronized(self) {
        float start = [self getBrightness];
        float target = 0;

        lastBrightnessLevel = start;

        for (float i = start; i >= target; i -= 0.08) {
            [self setBrightness:i];
            [NSThread sleepForTimeInterval:0.05];
        }

        [self setBrightness:target];
    }
}

- (void)switchLightsAsync:(BOOL)on {
    if (on) {
        [NSThread detachNewThreadSelector:@selector(lightsUp) toTarget:self withObject:nil];
    } else {
        [NSThread detachNewThreadSelector:@selector(lightsDown) toTarget:self withObject:nil];
    }
}

- (void)switchLightsInstantly:(BOOL)on {
    if (on) {
        // Don't do anything if the user has put backlight on again during playback.
        if ([self getBrightness] == 0) {
            [self setBrightness:lastBrightnessLevel];
        }
    } else {
        [self setBrightness:0];
    }
}

@end
