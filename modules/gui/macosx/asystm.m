//
//  asystm.m
//  
//
//  Created by Heiko Panther on Tue Sep 10 2002.
//

#import "asystm.h"
#define MAXINT 0x7fffffff

#define DEBUG_ASYSTM 1

// this is a basic set of rules
#define gNumClassificationRules 4

const struct classificationRule gClassificationRules[gNumClassificationRules]=
{
{	// The old AC3 format type
    'IAC3',
    0,
    audiodevice_class_ac3,
    "Digital AC3"
},
{	// The new AC3 format type
    kAudioFormat60958AC3,
    0,
    audiodevice_class_ac3,
    "Digital AC3"
},
{
    kAudioFormatLinearPCM,
    2,
    audiodevice_class_pcm2,
    "Stereo PCM"
},
{
    kAudioFormatLinearPCM,
    6,
    audiodevice_class_pcm6,
    "6 Channel PCM"
}
};

MacOSXAudioSystem *gTheMacOSXAudioSystem; // Remove this global, access audio system froma aout some other way

@implementation MacOSXSoundOption

- (id)initWithName:(NSString*)_name deviceID:(AudioDeviceID)devID streamIndex:(UInt32)strInd formatID:(UInt32)formID chans:(UInt32)chans;
{
    id me=0;
    if((me=[super init]))
    {
	name = _name;
	[name retain];
	deviceID=devID;
	streamIndex=strInd;
	mFormatID=formID;
	mChannels=chans;
    }
    return(me);
}

- (NSString*)name {return name;};

- (AudioDeviceID)deviceID {return deviceID;};

- (UInt32)streamIndex {return streamIndex;};

- (UInt32)mFormatID {return mFormatID;};

- (UInt32)mChannels {return mChannels;};

- (void)dealloc {[name release];};

@end


@implementation MacOSXAudioSystem

OSStatus listenerProc (AudioDeviceID		inDevice,
		       UInt32			inChannel,
		       Boolean			isInput,
		       AudioDevicePropertyID	inPropertyID,
		       void*			inClientData)
{
    intf_thread_t * p_intf = [NSApp getIntf];
    msg_Dbg(p_intf, "**********	Property Listener called! device %d, channel %d, isInput %d, propertyID %4.4s",
	    inDevice, inChannel, isInput, &inPropertyID);
    return 0;
};

OSStatus streamListenerProc (AudioStreamID		inStream,
		       UInt32			inChannel,
		       AudioDevicePropertyID	inPropertyID,
		       void*			inClientData)
{
    intf_thread_t * p_intf = [NSApp getIntf];
    msg_Dbg(p_intf, "**********	Property Listener called! stream %d, channel %d, propertyID %4.4s",
	    inStream, inChannel, &inPropertyID);
    return 0;
};

- (id)initWithGUI:(VLCMain*)_main
{
    id me=0;
    if((me=[super init]))
    {
	gTheMacOSXAudioSystem=self;
	main=_main;
	[main retain];
	intf_thread_t * p_intf = [NSApp getIntf];
	selectedOutput=0;

	// find audio devices
	// find out how many audio devices there are, if any
	OSStatus	status = noErr;
	UInt32 		theSize;
	Boolean		outWritable;
	AudioDeviceID	*deviceList = NULL;
	UInt32 i;

	status = AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices, &theSize, &outWritable);
	if(status != noErr)
	{
	    msg_Err(p_intf, "AudioHardwareGetPropertyInfo failed");
	};

	// calculate the number of device available
	UInt32 devicesAvailable = theSize / sizeof(AudioDeviceID);
	// Bail if there aren't any devices
	if(devicesAvailable < 1)
	{
	    msg_Err(p_intf, "no devices found");
	}
	if(DEBUG_ASYSTM) msg_Dbg(p_intf, "Have %i devices!", devicesAvailable);

	// make space for the devices we are about to get
	deviceList = (AudioDeviceID*)malloc(theSize);
	// get an array of AudioDeviceIDs
	status = AudioHardwareGetProperty(kAudioHardwarePropertyDevices, &theSize, (void *) deviceList);
	if(status != noErr)
	{
	    msg_Err(p_intf, "could not get Device list");
	};

	// Build a menu
	NSMenuItem *newItem;
	newItem = [[NSMenuItem allocWithZone:[NSMenu menuZone]] initWithTitle:@"Sound output" action:NULL keyEquivalent:@""];
	newMenu = [[NSMenu allocWithZone:[NSMenu menuZone]] initWithTitle:@"Sound output"];
	[newItem setSubmenu:newMenu];
	[[NSApp mainMenu] addItem:newItem];
	[newItem release];

	// check which devices can do what class of audio
	//    struct mosx_AudioDeviceData deviceData;
	for(i=0; i<devicesAvailable; i++)
	    [self CheckDevice:deviceList[i] isInput:false];	// only check the output part

	[newMenu release];
	free(deviceList);
    };
    return me;
};

- (AudioStreamID) getStreamIDForIndex:(UInt32)streamIndex device:(AudioDeviceID)deviceID
{
    // Does not currently use the stream index, but just returns the stream ID of the first stream.
    // Get the stream ID
    Boolean isInput=false, outWritable;
    UInt32 theSize;
    OSStatus err =  AudioDeviceGetPropertyInfo(deviceID, 0, isInput, kAudioDevicePropertyStreams,  &theSize, &outWritable);
    AudioStreamID *streamList = (AudioStreamID*)malloc(theSize);
    err = AudioDeviceGetProperty(deviceID, 0, isInput, kAudioDevicePropertyStreams, &theSize, streamList);
    AudioStreamID streamID = streamList[streamIndex - 1];
    free(streamList);
    return streamID;
}

- (void)CheckDevice:(AudioDeviceID)deviceID isInput:(bool)isInput
{
    OSStatus err;
    UInt32 		theSize;
    Boolean		outWritable;
    AudioBufferList	*bufferList = 0;
    UInt32 i, j;
    intf_thread_t * p_intf = [NSApp getIntf];
    char deviceName[32];	// Make this a CFString!

    // Add property listener
    err=AudioDeviceAddPropertyListener(deviceID, 1, isInput, kAudioDevicePropertyStreams, listenerProc, 0);
    if(err) msg_Err(p_intf, "Add Property Listener failed, err=%4.4s", &err);
    err=AudioDeviceAddPropertyListener(deviceID, 1, isInput, kAudioDevicePropertyStreamConfiguration, listenerProc, 0);
    if(err) msg_Err(p_intf, "Add Property Listener failed, err=%4.4s", &err);
    err=AudioDeviceAddPropertyListener(deviceID, 1, isInput, kAudioDevicePropertyStreamFormat, listenerProc, 0);
    if(err) msg_Err(p_intf, "Add Property Listener failed, err=%4.4s", &err);

    // Get the device name
    err = AudioDeviceGetPropertyInfo(deviceID, 0, isInput, kAudioDevicePropertyDeviceName,  &theSize, &outWritable);
    theSize=sizeof(deviceName);
    err = AudioDeviceGetProperty(deviceID, 0, isInput, kAudioDevicePropertyDeviceName, &theSize, deviceName);

    // Get the stream configuration
    err =  AudioDeviceGetPropertyInfo(deviceID, 0, isInput, kAudioDevicePropertyStreamConfiguration,  &theSize, &outWritable);
    bufferList = (AudioBufferList*)malloc(theSize);
    err = AudioDeviceGetProperty(deviceID, 0, isInput, kAudioDevicePropertyStreamConfiguration, &theSize, bufferList);

    if(DEBUG_ASYSTM) msg_Dbg(p_intf, "\nFound a %s, examing its %s, it has %i streams.", deviceName, (isInput?"Input":"Output"), bufferList->mNumberBuffers);

    // find details of each stream
    for (i=0; i < bufferList->mNumberBuffers; i++)
    {
	short streamIndex=i+1;
	UInt32 nActFormats;
	AudioStreamBasicDescription *formatsAvailable;

	AudioStreamID streamID=[self getStreamIDForIndex:streamIndex device:deviceID];

	// Add property listener
	err=AudioStreamAddPropertyListener(streamID, 0, kAudioDevicePropertyStreams, streamListenerProc, 0);
	if(err) msg_Err(p_intf, "Add Property Listener failed, err=%4.4s", &err);
	err=AudioStreamAddPropertyListener(streamID, 0, kAudioDevicePropertyStreamConfiguration, streamListenerProc, 0);
	if(err) msg_Err(p_intf, "Add Property Listener failed, err=%4.4s", &err);
	err=AudioStreamAddPropertyListener(streamID, 0, kAudioDevicePropertyStreamFormat, streamListenerProc, 0);
	if(err) msg_Err(p_intf, "Add Property Listener failed, err=%4.4s", &err);
	err=AudioStreamAddPropertyListener(streamID, 0, kAudioStreamPropertyPhysicalFormat, streamListenerProc, 0);
	if(err) msg_Err(p_intf, "Add Property Listener failed, err=%4.4s", &err);

	// Get the # of actual formats in the current stream
	err =  AudioStreamGetPropertyInfo(streamID, 0, kAudioStreamPropertyPhysicalFormats,  &theSize, &outWritable);
	nActFormats = theSize / sizeof(AudioStreamBasicDescription);
	if(DEBUG_ASYSTM) msg_Dbg(p_intf, "stream index %i, streamID %i, nActFormats %d", streamIndex, streamID, nActFormats);

	// Get the format specifications
	formatsAvailable=(AudioStreamBasicDescription*) malloc(theSize);
	err = AudioStreamGetProperty(streamID, 0, kAudioStreamPropertyPhysicalFormats, &theSize, formatsAvailable);
	if(err) msg_Err(p_intf, "AudioDeviceGetProperty err %d", err);
	
	// now classify the device and add a menu entry for each device class it matches
	for(j=0; j<gNumClassificationRules; j++)
	{
	    UInt32 numChans=MAXINT, format=0;
	    for(i=0; i<theSize/sizeof(AudioStreamBasicDescription); i++)
	    {
		if(DEBUG_ASYSTM) msg_Dbg(p_intf, "Finding formats: %4.4s - %d chans, %d Hz, %d bits/sample, %d bytes/frame",
	  &formatsAvailable[i].mFormatID, formatsAvailable[i].mChannelsPerFrame,
	  (UInt32)formatsAvailable[i].mSampleRate,
	  formatsAvailable[i].mBitsPerChannel, formatsAvailable[i].mBytesPerFrame);

		if(formatsAvailable[i].mFormatID != gClassificationRules[j].mFormatID && gClassificationRules[j].mFormatID!=0) continue;
		if(formatsAvailable[i].mChannelsPerFrame < gClassificationRules[j].mChannelsPerFrame && gClassificationRules[j].mChannelsPerFrame!=0) continue;
		// we want to choose the format with the smallest allowable channel number for this class
		if(formatsAvailable[i].mChannelsPerFrame < numChans)
		{
		    numChans=formatsAvailable[i].mChannelsPerFrame;
		    format=i;
		};
	    };
	    if(numChans!=MAXINT) // we found a good setting
	    {
		if(DEBUG_ASYSTM) msg_Dbg(p_intf, "classified into %d", gClassificationRules[j].characteristic);
		// make a sound option object
		char menuentry[48];
		snprintf(menuentry, 48, "%.32s: %.16s", deviceName, gClassificationRules[j].qualifierString);
		MacOSXSoundOption *device=[[MacOSXSoundOption alloc] initWithName:[NSString stringWithCString:menuentry] deviceID:deviceID streamIndex:streamIndex formatID:formatsAvailable[format].mFormatID chans:formatsAvailable[format].mChannelsPerFrame];
		[self registerSoundOption:device];
	    };
	};
	free(formatsAvailable);
    }

    free(bufferList);
};

- (void)registerSoundOption:(MacOSXSoundOption*)option {
    NSMenuItem *newItem;
    newItem = [[NSMenuItem allocWithZone:[NSMenu menuZone]] initWithTitle:[option name] action:NULL keyEquivalent:@""];
    [newItem setImage:[NSImage imageNamed:@"eomt_browsedata"]];
    [newItem setTarget:self];
    [newItem setAction:@selector(selectAction:)];
    [newItem setRepresentedObject:option];
    [newMenu addItem:newItem];
    if(selectedOutput==0) [self selectAction:newItem];
    [newItem release];
};
    
- (void)selectAction:(id)sender {
    [selectedOutput setState:NSOffState];
    selectedOutput=sender;
    [sender setState:NSOnState];
};

static void printStreamDescription(char *description, AudioStreamBasicDescription *format)
{
    intf_thread_t * p_intf = [NSApp getIntf];
    if(DEBUG_ASYSTM) msg_Dbg(p_intf, "%s: mSampleRate %ld, mFormatID %4.4s, mFormatFlags %ld, mBytesPerPacket %ld, mFramesPerPacket %ld, mBytesPerFrame %ld, mChannelsPerFrame %ld, mBitsPerChannel %ld",
	    description,
	    (UInt32)format->mSampleRate, &format->mFormatID,
	    format->mFormatFlags, format->mBytesPerPacket,
	    format->mFramesPerPacket, format->mBytesPerFrame,
	    format->mChannelsPerFrame, format->mBitsPerChannel);
};


- (AudioDeviceID)getSelectedDeviceSetToRate:(int)preferredSampleRate{
    // I know the selected device, stream, and the required format ID. Now find a format
    // that comes closest to the preferred rate
    // For sample size, it is assumed that 16 bits will always be enough.
    // Note that the caller is not guranteed to get the rate she preferred.
    AudioStreamBasicDescription *formatsAvailable;
    MacOSXSoundOption *selectedOption=[selectedOutput representedObject];
    bool foundFormat=false;
    UInt32 theSize;
    Boolean outWritable;
    OSStatus err;
    UInt32 i;
    intf_thread_t * p_intf = [NSApp getIntf];
    AudioDeviceID deviceID=[selectedOption deviceID];

    // get the streamID (it might have changed)
    AudioStreamID streamID=[self getStreamIDForIndex:[selectedOption streamIndex] device:deviceID];
	
    // Find the actual formats
    err =  AudioStreamGetPropertyInfo(streamID, 0, kAudioStreamPropertyPhysicalFormats,  &theSize, &outWritable);
    formatsAvailable=(AudioStreamBasicDescription*) malloc(theSize);
    err = AudioStreamGetProperty(streamID, 0, kAudioStreamPropertyPhysicalFormats, &theSize, formatsAvailable);
    if(err)
    {
	msg_Err(p_intf, "Error %4.4s getting the stream formats", &err);
	return 0;
    };
    
    UInt32 formtmp=[selectedOption mFormatID];
    if(DEBUG_ASYSTM) msg_Dbg(p_intf, "looking for:   %4.4s - %d chans, %d Hz", &formtmp,
	    [selectedOption mChannels], preferredSampleRate);

    // Check if there's a "best match" which has our required rate
    for(i=0; i<theSize/sizeof(AudioStreamBasicDescription); i++)
    {
	if(DEBUG_ASYSTM) msg_Dbg(p_intf, "actual:   %4.4s - %d chans, %d Hz, %d bits/sample, %d bytes/frame",
	 &formatsAvailable[i].mFormatID, formatsAvailable[i].mChannelsPerFrame,
	 (int)formatsAvailable[i].mSampleRate,
	 formatsAvailable[i].mBitsPerChannel, formatsAvailable[i].mBytesPerFrame);

	if(formatsAvailable[i].mChannelsPerFrame<0 || formatsAvailable[i].mChannelsPerFrame>100) {
	    msg_Err(p_intf, "bogus format! index %i", i);
	    return 0;
	};
	
	if( formatsAvailable[i].mFormatID == [selectedOption mFormatID]
     && formatsAvailable[i].mChannelsPerFrame == [selectedOption mChannels]
     && (int)formatsAvailable[i].mSampleRate == preferredSampleRate)
	{
	    if(DEBUG_ASYSTM) msg_Dbg(p_intf, "Found the perfect format!!");
	    foundFormat=true;
	    break;
	};
    };

    int rate=MAXINT, format=0;
    if(!foundFormat)
    {
	for(i=0; i<theSize/sizeof(AudioStreamBasicDescription); i++)
	{
	    // We don't have one... check if there's one with a higher sample rate.
	    // Upsampling should be better than downsampling.
	    // Select the smallest of the higher sample rates, to save resources.
	    int actrate=(int)formatsAvailable[i].mSampleRate;
	    if( formatsAvailable[i].mFormatID == [selectedOption mFormatID]
	 && formatsAvailable[i].mChannelsPerFrame == [selectedOption mChannels]
	 &&  actrate > preferredSampleRate)
	    {
		if(actrate < rate)
		{
		    rate=actrate;
		    format=i;
		}
	    };
	};
	if(rate!=MAXINT)	// This means we have found a rate!! Yes!
	{
	    if(DEBUG_ASYSTM) msg_Dbg(p_intf, "Only got a format with higher sample rate");
	    foundFormat=true;
	    i=format;
	};
    };
    
    if(!foundFormat)
    {
	rate=0;
	for(i=0; i<theSize/sizeof(AudioStreamBasicDescription); i++)
	{
	    // Our last chance: select the highest lower sample rate.
	    int actrate=(int)formatsAvailable[i].mSampleRate;
	    if( actrate >= preferredSampleRate) // We must have done something wrong
	    {
		if(DEBUG_ASYSTM) msg_Err(p_intf, "Found a rate that should have been selected previously.");
		free(formatsAvailable);
		return 0;
	    };

	    if(actrate > rate)
	    {
		rate=actrate;
		format=i;
	    }
	};

	if(rate!=0)	// This means we have found a rate!! Yes!
	{
	    if(DEBUG_ASYSTM) msg_Dbg(p_intf, "Only got a format with lower sample rate");
	    foundFormat=true;
	    i=format;
	}
	else // We must have done something wrong
	{
	    msg_Err(p_intf, "Found no rate which is equal, higher or lower to requested rate. That means our device either: a) didn't exist in the first place or b) has been removed since.");
	    free(formatsAvailable);
	    return 0;
	};
    };
    AudioStreamBasicDescription desc=formatsAvailable[i];
    free(formatsAvailable);

    // Set the device stream format
    Boolean isWriteable;

    err = AudioStreamGetPropertyInfo(streamID, 0, kAudioStreamPropertyPhysicalFormat, &theSize, &isWriteable);
    if(err) msg_Err(p_intf, "GetPropertyInfo (stream format) error %4.4s - theSize %d", &err, theSize);
    if(DEBUG_ASYSTM) msg_Dbg(p_intf, "size %d, writable %d", theSize, isWriteable);

    if(DEBUG_ASYSTM) printStreamDescription("want to set", &desc);

    err = AudioStreamSetProperty(streamID, 0, 0, kAudioStreamPropertyPhysicalFormat, theSize, &desc);
    if(err) msg_Err(p_intf, "SetProperty (stream format) error %4.4s - theSize %d", &err, theSize);

    // Because of the format change, the streamID has changed!
    // That's why we return the deviceID.
    return deviceID;
};

    
- (void)dealloc
{
    [main release];
};


@end
