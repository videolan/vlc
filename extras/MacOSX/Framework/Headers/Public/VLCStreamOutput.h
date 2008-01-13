//
//  VLCStreamOutput.h
//  VLCKit
//
//  Created by Pierre d'Herbemont on 1/12/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>

extern NSString * VLCDefaultStreamOutputRTSP;
extern NSString * VLCDefaultStreamOutputRTP;
extern NSString * VLCDefaultStreamOutputRTP;

@interface VLCStreamOutput : NSObject {
    NSMutableDictionary * options;
}

- (id)initWithOptionDictionary:(NSDictionary *)dictionary;
+ (id)streamOutputWithOptionDictionary:(NSDictionary *)dictionary;

+ (id)rtpBroadcastStreamOutputWithSAPAnnounce:(NSString *)announceName;
+ (id)rtpBroadcastStreamOutput;
+ (id)ipodStreamOutputWithFilePath:(NSString *)filePath;

@end
