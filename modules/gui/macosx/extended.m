/*****************************************************************************
 * extended.m: MacOS X Extended interface panel
 *****************************************************************************
 * Copyright (C) 2005-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne@videolan.org>
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


/*****************************************************************************
 * Preamble
 *****************************************************************************/

#import "extended.h"
#import "vout.h"
#import <vlc_aout.h>
#import <vlc_vout.h>
#import <vlc_interface.h>

/*****************************************************************************
 * VLCExtended implementation
 *****************************************************************************/

@implementation VLCExtended

static VLCExtended *_o_sharedInstance = nil;

+ (VLCExtended *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

- (id)init
{
    if (_o_sharedInstance) {
        [self dealloc];
    } else {
        _o_sharedInstance = [super init];
    }

    return _o_sharedInstance;
}

/*****************************************************************************
 * GUI methods
 *****************************************************************************/

- (void)initStrings
{
    /* localise GUI-strings */
    /* method is called from intf.m (in method showExtended) */
    [o_extended_window setTitle: _NS("Extended controls")];
    [o_btn_vidFlts_mrInfo setToolTip: _NS("Shows more information about the available video filters.")];
    [o_ckb_wave setTitle: _NS("Wave")];
    [o_ckb_ripple setTitle: _NS("Ripple")];
    [o_ckb_psycho setTitle: _NS("Psychedelic")];
    [o_ckb_gradient setTitle: _NS("Gradient")];
    [o_lbl_general setStringValue: _NS("General editing filters")];
    [o_lbl_distort setStringValue: _NS("Distortion filters")];
    [o_ckb_blur setTitle: _NS("Blur")];
    [o_ckb_blur setToolTip: _NS("Adds motion blurring to the image")];
    [o_ckb_imgClone setTitle: _NS("Image clone")];
    [o_ckb_imgClone setToolTip: _NS("Creates several copies of the Video "
                                    "output window" )];
    [o_ckb_imgCrop setTitle: _NS("Image cropping")];
    [o_ckb_imgCrop setToolTip: _NS("Crops a defined part of the image")];
    [o_ckb_imgInvers setTitle: _NS("Invert colors")];
    [o_ckb_imgInvers setToolTip: _NS("Inverts the colors of the image")];
    [o_ckb_trnsform setTitle: _NS("Transformation")];
    [o_ckb_trnsform setToolTip: _NS("Rotates or flips the image")];
    [o_ckb_intZoom setTitle: _NS("Interactive Zoom")];
    [o_ckb_intZoom setToolTip: _NS("Enables an interactive Zoom feature")];
    [o_ckb_vlme_norm setTitle: _NS("Volume normalization")];
    [o_ckb_vlme_norm setToolTip: _NS("Prevents the audio output from going "
                                     "over a predefined value.")];
    [o_ckb_hdphnVirt setTitle: _NS("Headphone virtualization")];
    [o_ckb_hdphnVirt setToolTip: _NS("Imitates the effect of surround sound "
                                     "when using headphones.")];
    [o_lbl_maxLevel setStringValue: _NS("Maximum level")];
    [o_btn_rstrDefaults setTitle: _NS("Restore Defaults")];
    [o_ckb_enblAdjustImg setTitle: _NS("Enable")];
    [o_lbl_brightness setStringValue: _NS("Brightness")];
    [o_lbl_contrast setStringValue: _NS("Contrast")];
    [o_lbl_gamma setStringValue: _NS("Gamma")];
    [o_lbl_hue setStringValue: _NS("Hue")];
    [o_lbl_saturation setStringValue: _NS("Saturation")];
    [o_lbl_opaque setStringValue: _NS("Opaqueness")];
 
}

- (void)awakeFromNib
{
    /* set the adjust-filter-sliders to the values from the prefs and enable
     * them, if wanted */
    char * psz_vfilters;
    intf_thread_t * p_intf = VLCIntf;
    psz_vfilters = config_GetPsz( p_intf, "vout-filter" );
    /* set the video-filter-checkboxes to the correct values */
    if( psz_vfilters )
    {
        [o_ckb_blur setState: (NSInteger)strstr( psz_vfilters, "motionblur")];
        [o_ckb_imgClone setState: (NSInteger)strstr( psz_vfilters, "clone")];
        [o_ckb_imgCrop setState: (NSInteger)strstr( psz_vfilters, "crop")];
        [o_ckb_trnsform setState: (NSInteger)strstr( psz_vfilters, "transform")];
        [o_ckb_intZoom setState: (NSInteger)strstr( psz_vfilters, "magnify")];

        free( psz_vfilters );
    }
 
    /* set the video-filter checkboxes to the correct values */
    char * psz_vifilters;
    psz_vifilters = config_GetPsz( p_intf, "video-filter" );
    if( psz_vifilters && strstr( psz_vifilters, "adjust" ) )
    {
        [o_ckb_enblAdjustImg setState: NSOnState];
        [o_btn_rstrDefaults setEnabled: YES];
        [o_sld_brightness setEnabled: YES];
        [o_sld_contrast setEnabled: YES];
        [o_sld_gamma setEnabled: YES];
        [o_sld_hue setEnabled: YES];
        [o_sld_saturation setEnabled: YES];
    }
    else
    {
        [o_ckb_enblAdjustImg setState: NSOffState];
        [o_btn_rstrDefaults setEnabled: NO];
        [o_sld_brightness setEnabled: NO];
        [o_sld_contrast setEnabled: NO];
        [o_sld_gamma setEnabled: NO];
        [o_sld_hue setEnabled: NO];
        [o_sld_saturation setEnabled: NO];
    }
    if( psz_vifilters )
    {
        [o_ckb_wave setState: (NSInteger)strstr( psz_vifilters, "wave")];
        [o_ckb_psycho setState: (NSInteger)strstr( psz_vifilters, "psychedelic")];
        [o_ckb_ripple setState: (NSInteger)strstr( psz_vifilters, "ripple")];
        [o_ckb_gradient setState: (NSInteger)strstr( psz_vifilters, "gradient")];
        [o_ckb_imgInvers setState: (NSInteger)strstr( psz_vifilters, "invert")];

        free( psz_vifilters );
    }
 
    /* set the audio-filter-checkboxes to the values taken from the prefs */
    char * psz_afilters;
    psz_afilters = config_GetPsz( p_intf, "audio-filter" );
    if( psz_afilters )
    {
        [o_ckb_hdphnVirt setState: (NSInteger)strstr( psz_afilters, "headphone" ) ];
        [o_ckb_vlme_norm setState: (NSInteger)strstr( psz_afilters, "normvol" ) ];
 
        free( psz_afilters );
    }

    /* fill the popup button according to our available views */
    [o_selector_pop removeAllItems];
    [o_selector_pop addItemWithTitle: _NS("Adjust Image")];
    [o_selector_pop addItemWithTitle: _NS("Video Filter")];
    [o_selector_pop addItemWithTitle: _NS("Audio Filter")];
    [o_selector_pop selectItemAtIndex: 0];

    /* make sure we draw a view on launch */
    [self viewSelectorAction: self];

    [self initStrings];
}

- (BOOL)configChanged
{
    return o_config_changed;
}

- (void)showPanel
{
    /* get the correct slider values from the prefs, in case they were changed
     * elsewhere */
    intf_thread_t * p_intf = VLCIntf;

    int i_value = config_GetInt( p_intf, "hue" );
    if( i_value > 0 && i_value < 360 )
    {
        [o_sld_hue setIntValue: i_value];
    }

    float f_value;
 
    f_value = config_GetFloat( p_intf, "saturation" );
    if( f_value > 0 && f_value < 5 )
        [o_sld_saturation setIntValue: (int)(100 * f_value) ];

    f_value = config_GetFloat( p_intf, "contrast" );
    if( f_value > 0 && f_value < 4 )
        [o_sld_contrast setIntValue: (int)(100 * f_value) ];

    f_value = config_GetFloat( p_intf, "brightness" );
    if( f_value > 0 && f_value < 2 )
        [o_sld_brightness setIntValue: (int)(100 * f_value) ];

    f_value = config_GetFloat( p_intf, "gamma" );
    if( f_value > 0 && f_value < 10 )
        [o_sld_gamma setIntValue: (int)(10 * f_value) ];

    f_value = config_GetFloat( p_intf, "norm-max-level" );
    if( f_value > 0 && f_value < 10 )
        [o_sld_maxLevel setFloatValue: f_value ];

    [o_sld_opaque setFloatValue: (config_GetFloat( p_intf,
        "macosx-opaqueness") * 100)];

    /* show the window */
    [o_extended_window displayIfNeeded];
    [o_extended_window makeKeyAndOrderFront:nil];
}

- (IBAction)viewSelectorAction:(id)sender
{
    NSView *o_toBeShown_view;
    /* check which view to show */
    if( [[[o_selector_pop selectedItem] title] isEqualToString: _NS("Adjust Image")] )
        o_toBeShown_view = o_adjustImg_view;
    else if( [[[o_selector_pop selectedItem] title] isEqualToString: _NS("Audio Filter")] )
        o_toBeShown_view = o_audioFlts_view;
    else if( [[[o_selector_pop selectedItem] title] isEqualToString: _NS("Video Filter")] )
        o_toBeShown_view = o_videoFilters_view;
    else
        msg_Err( VLCIntf, "invalid ui view requested" );
    
    NSRect o_win_rect, o_view_rect, o_old_view_rect;
    o_win_rect = [o_extended_window frame];
    o_view_rect = [o_toBeShown_view frame];
    
    if( o_currentlyshown_view != nil )
    {
        /* restore our window's height, if we've shown another category previously */
        o_old_view_rect = [o_currentlyshown_view frame];
        o_win_rect.size.height = o_win_rect.size.height - o_old_view_rect.size.height;
        o_win_rect.origin.y = ( o_win_rect.origin.y + o_old_view_rect.size.height ) - o_view_rect.size.height;
        
        /* remove our previous category view */
        [o_currentlyshown_view removeFromSuperviewWithoutNeedingDisplay];
    }
    
    o_win_rect.size.height = o_win_rect.size.height + o_view_rect.size.height;
    
    //[o_extended_window displayIfNeeded];
    [o_extended_window setFrame: o_win_rect display:YES animate: YES];
    
    [o_toBeShown_view setFrame: NSMakeRect( 0, 
                                              0, //[o_top_controls_box frame].size.height, 
                                              o_view_rect.size.width, 
                                              o_view_rect.size.height )];
    [o_toBeShown_view setNeedsDisplay: YES];
    [o_toBeShown_view setAutoresizesSubviews: YES];
    [[o_extended_window contentView] addSubview: o_toBeShown_view];

    /* keep our current category for further reference */
    [o_currentlyshown_view release];
    o_currentlyshown_view = o_toBeShown_view;
    [o_currentlyshown_view retain];
}

- (IBAction)enableAdjustImage:(id)sender
{
    /* en-/disable the sliders */
    if ([o_ckb_enblAdjustImg state] == NSOnState)
    {
        [o_btn_rstrDefaults setEnabled: YES];
        [o_sld_brightness setEnabled: YES];
        [o_sld_contrast setEnabled: YES];
        [o_sld_gamma setEnabled: YES];
        [o_sld_hue setEnabled: YES];
        [o_sld_saturation setEnabled: YES];
        [self changeVideoFiltersString: "adjust" onOrOff: true];
    }
    else
    {
        [o_btn_rstrDefaults setEnabled: NO];
        [o_sld_brightness setEnabled: NO];
        [o_sld_contrast setEnabled: NO];
        [o_sld_gamma setEnabled: NO];
        [o_sld_hue setEnabled: NO];
        [o_sld_saturation setEnabled: NO];
        [self changeVideoFiltersString: "adjust" onOrOff: false];
    }
}

- (IBAction)restoreDefaultsForAdjustImage:(id)sender
{
    /* reset the sliders */
    [o_sld_brightness setIntValue: 100];
    [o_sld_contrast setIntValue: 100];
    [o_sld_gamma setIntValue: 10];
    [o_sld_hue setIntValue: 0];
    [o_sld_saturation setIntValue: 100];
    [o_sld_opaque setIntValue: 100];

    /* transmit the values */
    [self sliderActionAdjustImage: o_sld_brightness];
    [self sliderActionAdjustImage: o_sld_contrast];
    [self sliderActionAdjustImage: o_sld_gamma];
    [self sliderActionAdjustImage: o_sld_hue];
    [self sliderActionAdjustImage: o_sld_saturation];
    [self opaqueSliderAction: o_sld_opaque];
}

- (IBAction)sliderActionAdjustImage:(id)sender
{
    /* read-out the sliders' values and apply them */
    intf_thread_t * p_intf = VLCIntf;
    vout_thread_t *p_vout = (vout_thread_t *)vlc_object_find(p_intf, VLC_OBJECT_VOUT, FIND_ANYWHERE);
    vlc_object_t *p_filter;

    if( p_vout == NULL )
    {
        msg_Dbg( p_intf, "no vout present, saving settings anyway" );
        if (sender == o_sld_brightness)
        {
            config_PutFloat( p_intf , "brightness" , [o_sld_brightness floatValue] / 100);
        } 
        else if (sender == o_sld_contrast)
        {
            config_PutFloat( p_intf , "contrast" , [o_sld_contrast floatValue] / 100);
        } 
        else if (sender == o_sld_gamma)
        {
            config_PutFloat( p_intf , "gamma" , [o_sld_gamma floatValue] / 10);
        } 
        else if (sender == o_sld_hue)
        {
            config_PutInt( p_intf , "hue" , [o_sld_hue intValue]);
        } 
        else if (sender == o_sld_saturation)
        {
            config_PutFloat( p_intf , "saturation" , [o_sld_saturation floatValue] / 100);
        } 
        else
        {
            msg_Warn( p_intf, "the corresponding subfilter coundn't be found" );
        }
    } 
    else
    {
        msg_Dbg( p_intf, "we found a vout to adjust, let's look for the filter" );
        p_filter = (vlc_object_t *)vlc_object_find_name( p_intf, "adjust", FIND_ANYWHERE );

        if(! p_filter )
        {
            msg_Err( p_intf, "we're unable to find the adjust filter!" );
            vlc_object_release( p_vout );
            return;
        }

        if (sender == o_sld_brightness)
        {
            var_SetFloat( p_filter, "brightness", [o_sld_brightness floatValue] / 100 );
            config_PutFloat( p_intf, "brightness", [o_sld_brightness floatValue] / 100 );
        } 
        else if (sender == o_sld_contrast)
        {
            var_SetFloat( p_filter, "contrast", [o_sld_contrast floatValue] / 100 );
            config_PutFloat( p_intf, "contrast", [o_sld_contrast floatValue] / 100 );
        } 
        else if (sender == o_sld_gamma)
        {
            var_SetFloat( p_filter, "gamma", [o_sld_gamma floatValue] / 10 );
            config_PutFloat( p_intf, "gamma", [o_sld_gamma floatValue] / 10 );
        } 
        else if (sender == o_sld_hue)
        {
            var_SetInteger( p_filter, "hue", [o_sld_hue intValue] );
            config_PutInt( p_intf , "hue" , [o_sld_hue intValue] );
        } 
        else if (sender == o_sld_saturation)
        {
            var_SetFloat( p_filter, "saturation", [o_sld_saturation floatValue] / 100 );
            config_PutFloat( p_intf , "saturation" , [o_sld_saturation floatValue] / 100 );
        } 
        else
        {
            msg_Warn( p_intf, "couldn't find variable for slider!" );
        }
        vlc_object_release( p_filter );
        vlc_object_release( p_vout );
    }

    o_config_changed = YES;
}

/* change the opaqueness of the vouts */
- (IBAction)opaqueSliderAction:(id)sender
{
    vlc_value_t val;
    id o_window = [NSApp keyWindow];
    NSArray *o_windows = [NSApp orderedWindows];
    NSEnumerator *o_enumerator = [o_windows objectEnumerator];
    playlist_t * p_playlist = pl_Hold( VLCIntf );
    vout_thread_t *p_vout = vlc_object_find( VLCIntf, VLC_OBJECT_VOUT, FIND_ANYWHERE );
    vout_thread_t *p_real_vout;

    val.f_float = [o_sld_opaque floatValue] / 100;

    if( p_vout != NULL )
    {
        p_real_vout = [VLCVoutView realVout: p_vout];
        var_Set( p_real_vout, "macosx-opaqueness", val );

        while ((o_window = [o_enumerator nextObject]))
        {
            if( [[o_window className] isEqualToString: @"VLCVoutWindow"] ||
                [[[VLCMain sharedInstance] embeddedList]
                                    windowContainsEmbedded: o_window])
            {
                [o_window setAlphaValue: val.f_float];
            }
            break;
        }
        vlc_object_release( p_vout );
    }

    /* store to prefs */
    config_PutFloat( p_playlist , "macosx-opaqueness" , val.f_float );

    pl_Release( VLCIntf );

    o_config_changed = YES;
}

- (IBAction)enableHeadphoneVirtualizer:(id)sender
{
    /* en-/disable headphone virtualisation */
    if ([o_ckb_hdphnVirt state] == NSOnState)
        [self changeAFiltersString: "headphone_channel_mixer" onOrOff: true ];
    else
        [self changeAFiltersString: "headphone_channel_mixer" onOrOff: false ];
}

- (IBAction)sliderActionMaximumAudioLevel:(id)sender
{
    /* read-out the slider's value and apply it */
    intf_thread_t * p_intf = VLCIntf;
    aout_instance_t * p_aout= (aout_instance_t *)vlc_object_find(p_intf, VLC_OBJECT_AOUT, FIND_ANYWHERE);

    if( p_aout != NULL )
    {
        var_SetFloat( p_aout, "norm-max-level", [o_sld_maxLevel floatValue] );
        vlc_object_release( p_aout );
    }

    config_PutFloat( p_intf, "norm-max-level", [o_sld_maxLevel floatValue] );

    o_config_changed = YES;
}

- (IBAction)enableVolumeNormalization:(id)sender
{
    /* en-/disable volume normalisation */
    if( [o_ckb_vlme_norm state] == NSOnState )
        [self changeAFiltersString: "normvol" onOrOff: YES ];
    else
        [self changeAFiltersString: "normvol" onOrOff: NO ];
}

- (IBAction)videoFilterAction:(id)sender
{
    /* en-/disable video filters */
    if (sender == o_ckb_blur)
        [self changeVideoFiltersString: "motionblur" onOrOff: [o_ckb_blur state]];

    else if (sender == o_ckb_imgClone)
        [self changeVoutFiltersString: "clone" onOrOff: [o_ckb_imgClone state]];

    else if (sender == o_ckb_imgCrop)
        [self changeVoutFiltersString: "crop" onOrOff: [o_ckb_imgCrop state]];

    else if (sender == o_ckb_imgInvers)
        [self changeVideoFiltersString: "invert" onOrOff: [o_ckb_imgInvers state]];

    else if (sender == o_ckb_trnsform)
        [self changeVoutFiltersString: "transform" onOrOff: [o_ckb_trnsform state]];

    else if (sender == o_ckb_intZoom )
        [self changeVoutFiltersString: "magnify" onOrOff: [o_ckb_intZoom state]];

    else if (sender == o_ckb_wave )
        [self changeVideoFiltersString: "wave" onOrOff: [o_ckb_wave state]];

    else if (sender == o_ckb_gradient )
        [self changeVideoFiltersString: "gradient" onOrOff: [o_ckb_gradient state]];

    else if (sender == o_ckb_psycho )
        [self changeVideoFiltersString: "psychedelic" onOrOff: [o_ckb_psycho state]];

    else if (sender == o_ckb_ripple )
        [self changeVideoFiltersString: "ripple" onOrOff: [o_ckb_ripple state]];

    else
        msg_Err( VLCIntf, "cannot find switched video-filter" ); /* this can't happen */
}

- (IBAction)moreInfoVideoFilters:(id)sender
{
    /* show info sheet */
    NSBeginInformationalAlertSheet(_NS("About the video filters"), 
                                   _NS("OK"), 
                                   @"", 
                                   @"",
                                   o_extended_window, 
                                   nil, 
                                   nil, 
                                   nil, 
                                   nil, 
                                   _NS("This panel allows on-the-fly selection of various video effects.\n"
                                       "These filters can be configured individually in the Preferences, in "
                                       "the subsections of Video/Filters.\n"
                                       "To choose the order in which the filter are applied, a filter "
                                       "option string can be set in the Preferences, Video / Filters section."));
}


/*****************************************************************************
 * methods to communicate changes to VLC's core
 *****************************************************************************/

- (void)changeVoutFiltersString:(char *)psz_name onOrOff:(bool )b_add
{
    /* copied from ../wxwidgets/extrapanel.cpp
     * renamed to conform with Cocoa's rules */
    /* this method only changes 1st generation video filters (the ones that
     * can't be used for transcoding). Have a look at changeVideoFiltersString
     * for the 2nd generation filters. */
 
    vout_thread_t *p_vout;
    intf_thread_t * p_intf = VLCIntf;
 
    char *psz_parser, *psz_string;
    psz_string = config_GetPsz( p_intf, "vout-filter" );
 
    if( !psz_string ) psz_string = strdup("");

    psz_parser = strstr( psz_string, psz_name );

    if( b_add )
    {
        if( !psz_parser )
        {
            psz_parser = psz_string;
            asprintf( &psz_string, (*psz_string) ? "%s:%s" : "%s%s",
                            psz_string, psz_name );
            free( psz_parser );
        }
        else
        {
            return;
        }
    }
    else
    {
        if( psz_parser )
        {
            memmove( psz_parser, psz_parser + strlen(psz_name) +
                            (*(psz_parser + strlen(psz_name)) == ':' ? 1 : 0 ),
                            strlen(psz_parser + strlen(psz_name)) + 1 );

            /* Remove trailing : : */
            if( *(psz_string+strlen(psz_string ) -1 ) == ':' )
            {
                *(psz_string+strlen(psz_string ) -1 ) = '\0';
            }
         }
         else
         {
             free( psz_string );
             return;
         }
    }
    /* Vout is not kept, so put that in the config */
    config_PutPsz( p_intf, "vout-filter", psz_string );

    /* Try to set on the fly */
    p_vout = (vout_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                              FIND_ANYWHERE );
    if( p_vout )
    {
        var_SetString( p_vout, "vout-filter", psz_string );
        vlc_object_release( p_vout );
    }

    free( psz_string );

    o_config_changed = YES;
}


- (void)changeVideoFiltersString:(char *)psz_name onOrOff:(bool )b_add
{
    /* same as changeVoutFiltersString but addressing the "video-filter"
     * variable which represents the video filter 2 modules */
 
    vout_thread_t *p_vout;
    intf_thread_t * p_intf = VLCIntf;
 
    char *psz_parser, *psz_string;
    psz_string = config_GetPsz( p_intf, "video-filter" );
 
    if( !psz_string ) psz_string = strdup("");

    psz_parser = strstr( psz_string, psz_name );

    if( b_add )
    {
        if( !psz_parser )
        {
            psz_parser = psz_string;
            asprintf( &psz_string, (*psz_string) ? "%s:%s" : "%s%s",
                            psz_string, psz_name );
            free( psz_parser );
        }
        else
        {
            return;
        }
    }
    else
    {
        if( psz_parser )
        {
            memmove( psz_parser, psz_parser + strlen(psz_name) +
                            (*(psz_parser + strlen(psz_name)) == ':' ? 1 : 0 ),
                            strlen(psz_parser + strlen(psz_name)) + 1 );

            /* Remove trailing : : */
            if( *(psz_string+strlen(psz_string ) -1 ) == ':' )
            {
                *(psz_string+strlen(psz_string ) -1 ) = '\0';
            }
         }
         else
         {
             free( psz_string );
             return;
         }
    }
    /* Vout is not kept, so put that in the config */
    config_PutPsz( p_intf, "video-filter", psz_string );

    /* Try to set on the fly */
    p_vout = (vout_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                              FIND_ANYWHERE );
    if( p_vout )
    {
        var_SetString( p_vout, "video-filter", psz_string );
        vlc_object_release( p_vout );
    }

    free( psz_string );

    o_config_changed = YES;
}

- (void)changeAFiltersString: (char *)psz_name onOrOff: (bool )b_add;
{
    /* copied from ../wxwidgets/extrapanel.cpp
     * renamed to conform with Cocoa's rules */

    char *psz_parser, *psz_string;
    intf_thread_t * p_intf = VLCIntf;
    aout_instance_t * p_aout= (aout_instance_t *)vlc_object_find(p_intf,
                                 VLC_OBJECT_AOUT, FIND_ANYWHERE);

    if( p_aout )
    {
        psz_string = var_GetNonEmptyString( p_aout, "audio-filter" );
    }
    else
    {
        psz_string = config_GetPsz( p_intf, "audio-filter" );
    }

    if( !psz_string ) psz_string = strdup("");

    psz_parser = strstr( psz_string, psz_name );

    if( b_add )
    {
        if( !psz_parser )
        {
            psz_parser = psz_string;
            asprintf( &psz_string, (*psz_string) ? "%s:%s" : "%s%s",
                            psz_string, psz_name );
            free( psz_parser );
        }
        else
        {
            if( p_aout ) vlc_object_release( p_aout );
            return;
        }
    }
    else
    {
        if( psz_parser )
        {
            memmove( psz_parser, psz_parser + strlen(psz_name) +
                            (*(psz_parser + strlen(psz_name)) == ':' ? 1 : 0 ),
                            strlen(psz_parser + strlen(psz_name)) + 1 );

            if( *(psz_string+strlen(psz_string ) -1 ) == ':' )
            {
                *(psz_string+strlen(psz_string ) -1 ) = '\0';
            }
         }
         else
         {
             free( psz_string );
             if( p_aout ) vlc_object_release( p_aout );
             return;
         }
    }

    if( p_aout == NULL )
    {
        config_PutPsz( p_intf, "audio-filter", psz_string );
    }
    else
    {
        var_SetString( p_aout, "audio-filter", psz_string );
        int i = 0;
        while( i < p_aout->i_nb_inputs )
        {
            p_aout->pp_inputs[i]->b_restart = true;
            i = (i + 1);
        }
        vlc_object_release( p_aout );
    }
    free( psz_string );

    o_config_changed = YES;
}

- (void)savePrefs
{
    /* save the preferences to make sure that our module-changes will up on
     * next launch again */
    playlist_t * p_playlist = pl_Hold( VLCIntf );
    int returnedValue;
    NSArray * theModules;
    theModules = [[NSArray alloc] initWithObjects: @"main", 
        @"headphone",
        @"transform", 
        @"adjust", 
        @"invert", 
        @"motionblur", 
        @"distort",
        @"clone", 
        @"crop", 
        @"normvol", 
        @"headphone_channel_mixer", 
        @"macosx",
        nil];
    unsigned int x = 0;
 
    while ( x != [theModules count] )
    {
        returnedValue = config_SaveConfigFile( p_playlist, [[theModules
            objectAtIndex: x] UTF8String] );

        if (returnedValue != 0)
        {
            msg_Err(p_playlist, "unable to save the preferences of the "
            "extended control attribute '%s' (%i)",
            [[theModules objectAtIndex: x] UTF8String] , returnedValue);
            [theModules release];
            pl_Release( VLCIntf );
 
            return;
        }

        x = ( x + 1 );
    }
 
    msg_Dbg( VLCIntf, "VLCExtended: saved certain preferences successfully" );
 
    [theModules release];
    pl_Release( VLCIntf );
}
@end
