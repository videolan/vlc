//
//  main.h
//  FindHW
//
//  Created by Heiko Panther on Sun Sep 08 2002.
//

#import <Foundation/Foundation.h>
#import <CoreAudio/CoreAudio.h>

enum audiodeviceClasses
{
    audiodevice_class_ac3	=1<<0,	// compressed AC3
    audiodevice_class_pcm2	=1<<1,	// stereo PCM (uncompressed)
    audiodevice_class_pcm6	=1<<2	// 6-channel PCM (uncompressed)
};

// specifies a rule for finding if a Device belongs to a class from above.
// if value==0, the value is ignored when matching. All other values must match.
struct classificationRule
{
    UInt32 mFormatID;
    UInt32 mChannelsPerFrame;
    enum audiodeviceClasses characteristic;
    char qualifierString[16];
};
