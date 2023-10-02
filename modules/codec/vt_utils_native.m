#import "vt_utils.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

bool cvpx_system_has_metal_device()
{
#if TARGET_OS_IPHONE
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    return device != nil;
#else
    NSArray <id<MTLDevice>> *devices = MTLCopyAllDevices();
    return devices.count > 0;
#endif
}