/*****************************************************************************
 * nsspeechsynthesizer.m: Simple text to Speech renderer for Mac OS X
 *****************************************************************************
 * Copyright (C) 2015 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan # org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_subpicture.h>

#import <Cocoa/Cocoa.h>

static int Create (vlc_object_t *);
static void Destroy(vlc_object_t *);
static int RenderText(filter_t *,
                      subpicture_region_t *,
                      subpicture_region_t *,
                      const vlc_fourcc_t *);

vlc_module_begin ()
set_description(N_("Speech synthesis for Mac OS X"))
set_category(CAT_VIDEO)
set_subcategory(SUBCAT_VIDEO_SUBPIC)

set_capability("text renderer", 0)
set_callbacks(Create, Destroy)
vlc_module_end ()

typedef struct filter_sys_t
{
    NSSpeechSynthesizer *speechSynthesizer;
    NSString *currentLocale;
    NSString *lastString;
} filter_sys_t;

static int  Create (vlc_object_t *p_this)
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    p_filter->p_sys = p_sys = malloc(sizeof(filter_sys_t));
    if (!p_sys)
        return VLC_ENOMEM;

    p_sys->currentLocale = p_sys->lastString = @"";
    p_sys->speechSynthesizer = [[NSSpeechSynthesizer alloc] init];

    p_filter->pf_render = RenderText;

    return VLC_SUCCESS;
}

static void Destroy(vlc_object_t *p_this)
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    [p_sys->speechSynthesizer stopSpeaking];
    [p_sys->speechSynthesizer release];
    p_sys->speechSynthesizer = nil;

    [p_sys->lastString release];
    p_sys->lastString = nil;

    [p_sys->currentLocale release];
    p_sys->currentLocale = nil;

    free(p_sys);
}

static NSString * languageCodeForString(NSString *string) {
    return (NSString *)CFStringTokenizerCopyBestStringLanguage((CFStringRef)string, CFRangeMake(0, [string length]));
}

static int RenderText(filter_t *p_filter,
                      subpicture_region_t *p_region_out,
                      subpicture_region_t *p_region_in,
                      const vlc_fourcc_t *p_chroma_list)
{
    @autoreleasepool {
        filter_sys_t *p_sys = p_filter->p_sys;
        text_segment_t *p_segment = p_region_in->p_text;

        if (!p_segment)
            return VLC_EGENERIC;

        for ( const text_segment_t *s = p_segment; s != NULL; s = s->p_next ) {
            if ( !s->psz_text )
                continue;

            if (strlen(s->psz_text) == 0)
                continue;

            NSString *stringToSpeech = [NSString stringWithUTF8String:s->psz_text];

            if ([p_sys->lastString isEqualToString:stringToSpeech])
                continue;

            if ([stringToSpeech isEqualToString:@"\n"])
                continue;

            p_sys->lastString = [stringToSpeech retain];

            msg_Dbg(p_filter, "Speaking '%s'", [stringToSpeech UTF8String]);

            NSString *detectedLocale = languageCodeForString(stringToSpeech);
            if (detectedLocale != nil) {
                if (![detectedLocale isEqualToString:p_sys->currentLocale]) {
                    p_sys->currentLocale = [detectedLocale retain];
                    msg_Dbg(p_filter, "switching speaker locale to '%s'", [p_sys->currentLocale UTF8String]);
                    NSArray *voices = [NSSpeechSynthesizer availableVoices];
                    NSUInteger count = voices.count;
                    NSRange range = NSMakeRange(0, 2);

                    for (NSUInteger i = 0; i < count; i++) {
                        NSDictionary *voiceAttributes = [NSSpeechSynthesizer attributesForVoice: [voices objectAtIndex:i]];
                        NSString *voiceLanguage = [voiceAttributes objectForKey:@"VoiceLanguage"];
                        if ([p_sys->currentLocale isEqualToString:[voiceLanguage substringWithRange:range]]) {
                            NSString *voiceName = [voiceAttributes objectForKey:@"VoiceName"];
                            msg_Dbg(p_filter, "switched to voice '%s'", [voiceName UTF8String]);
                            if ([voiceName isEqualToString:@"Agnes"] || [voiceName isEqualToString:@"Albert"])
                                continue;
                            [p_sys->speechSynthesizer setVoice: [voices objectAtIndex:i]];
                            break;
                        }
                    }
                }
            }

            [p_sys->speechSynthesizer startSpeakingString:stringToSpeech];
        }

        return VLC_SUCCESS;
    }
}
