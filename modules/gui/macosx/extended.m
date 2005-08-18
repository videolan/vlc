/*****************************************************************************
 * extended.m: MacOS X Extended interface panel
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Felix Kühne <fkuehne@users.sf.net>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/


/*****************************************************************************
 * Note: 
 * the code used to bind with VLC's modules is heavily based upon 
 * ../wxwidgets/extrapanel.cpp, written by Clément Stenac.
 * the code used to insert/remove the views was inspired by intf.m, 
 * written by Derk-Jan Hartman and Benjamin Pracht. 
 * (all 3 are members of the VideoLAN team) 
 *****************************************************************************/


/*****************************************************************************
 * Preamble
 *****************************************************************************/

#import "extended.h"
#import "intf.h"
#import "vout.h"
#import <vlc/aout.h>
#import <aout_internal.h>
#import <vlc/vout.h>
#import <vlc/intf.h>

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
    [o_lbl_video setStringValue: _NS("Video")];
    [o_lbl_audio setStringValue: _NS("Audio")];
    [o_lbl_audioFlts setStringValue: _NS("Audio filters")];
    [o_lbl_videoFlts setStringValue: _NS("Video filters")];
    [o_lbl_adjustImage setStringValue: _NS("Adjust Image")];
    [o_btn_vidFlts_mrInfo setTitle: _NS("More Info")];
    [o_ckb_blur setTitle: _NS("Blurring")];
    [o_ckb_blur setToolTip: _NS("Creates a motion blurring on the image")];
    [o_ckb_distortion setTitle: _NS("Distortion")];
    [o_ckb_distortion setToolTip: _NS("Adds distorsion effects")];
    [o_ckb_imgClone setTitle: _NS("Image clone")];
    [o_ckb_imgClone setToolTip: _NS("Creates several clones of the image")];
    [o_ckb_imgCrop setTitle: _NS("Image cropping")];
    [o_ckb_imgCrop setToolTip: _NS("Crops the image")];
    [o_ckb_imgInvers setTitle: _NS("Image inversion")];
    [o_ckb_imgInvers setToolTip: _NS("Inverts the image colors")];
    [o_ckb_trnsform setTitle: _NS("Transformation")];
    [o_ckb_trnsform setToolTip: _NS("Rotates or flips the image")];
    [o_ckb_vlme_norm setTitle: _NS("Volume normalization")];
    [o_ckb_vlme_norm setToolTip: _NS("This filters prevents the audio output " \
        "power from going over a defined value.")];
    [o_ckb_hdphnVirt setTitle: _NS("Headphone virtualization")];
    [o_ckb_hdphnVirt setToolTip: _NS("This filter gives the feeling of a " \
        "5.1 speaker set when using a headphone.")];
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
    if( psz_vfilters && strstr( psz_vfilters, "adjust" ) )
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
    
    /* set the other video-filter-checkboxes to the correct values */
    if( psz_vfilters )
    {
        [o_ckb_blur setState: (int)strstr( psz_vfilters, "motionblur")];
        [o_ckb_distortion setState: (int)strstr( psz_vfilters, "distort")];
        [o_ckb_imgClone setState: (int)strstr( psz_vfilters, "clone")];
        [o_ckb_imgCrop setState: (int)strstr( psz_vfilters, "crop")];
        [o_ckb_imgInvers setState: (int)strstr( psz_vfilters, "invert")];
        [o_ckb_trnsform setState: (int)strstr( psz_vfilters, "transform")];
        
        free( psz_vfilters );
    }
    
    /* set the audio-filter-checkboxes to the values taken from the prefs */
    char * psz_afilters;
    psz_afilters = config_GetPsz( p_intf, "audio-filter" );
    if( psz_afilters )
    {
        [o_ckb_hdphnVirt setState: (int)strstr( psz_afilters, "headphone" ) ];
        [o_ckb_vlme_norm setState: (int)strstr( psz_afilters, "normvol" ) ];
        
        free( psz_afilters );
    }
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
    {
        [o_sld_saturation setIntValue: (int)(100 * f_value) ];
    }

    f_value = config_GetFloat( p_intf, "contrast" );
    if( f_value > 0 && f_value < 4 )
    {
        [o_sld_contrast setIntValue: (int)(100 * f_value) ];
    }

    f_value = config_GetFloat( p_intf, "brightness" );
    if( f_value > 0 && f_value < 2 )
    {
        [o_sld_brightness setIntValue: (int)(100 * f_value) ];
    }

    f_value = config_GetFloat( p_intf, "gamma" );
    if( f_value > 0 && f_value < 10 )
    {
        [o_sld_gamma setIntValue: (int)(10 * f_value) ];
    }

    [o_sld_maxLevel setFloatValue: (config_GetFloat(p_intf, "norm-max-level") \
        * 10)];

    [o_sld_opaque setFloatValue: (config_GetFloat( p_intf, \
        "macosx-opaqueness") * 100)];


    /* show the window */
    [o_extended_window displayIfNeeded];
    [o_extended_window makeKeyAndOrderFront:nil];
}

- (IBAction)adjImg_Enbl:(id)sender
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
        [self changeVFiltersString: "adjust" onOrOff: VLC_TRUE];
    }else{
        [o_btn_rstrDefaults setEnabled: NO];
        [o_sld_brightness setEnabled: NO];
        [o_sld_contrast setEnabled: NO];
        [o_sld_gamma setEnabled: NO];
        [o_sld_hue setEnabled: NO];
        [o_sld_saturation setEnabled: NO];
        [self changeVFiltersString: "adjust" onOrOff: VLC_FALSE];
    }
}

- (IBAction)adjImg_rstrDefaults:(id)sender
{
    /* reset the sliders */
    [o_sld_brightness setIntValue: 100];
    [o_sld_contrast setIntValue: 100];
    [o_sld_gamma setIntValue: 10];
    [o_sld_hue setIntValue: 0];
    [o_sld_saturation setIntValue: 100];
    
    /* transmit the values */
    [self adjImg_sliders: o_sld_brightness];
    [self adjImg_sliders: o_sld_contrast];
    [self adjImg_sliders: o_sld_gamma];
    [self adjImg_sliders: o_sld_hue];
    [self adjImg_sliders: o_sld_saturation];
}

- (IBAction)adjImg_sliders:(id)sender
{
    /* read-out the sliders' values and apply them */
    intf_thread_t * p_intf = VLCIntf;
    vout_thread_t *p_vout = (vout_thread_t *)vlc_object_find(p_intf,
                                 VLC_OBJECT_VOUT, FIND_ANYWHERE);
    if( p_vout == NULL )
    {
        if (sender == o_sld_brightness)
        {
            config_PutFloat( p_intf , "brightness" , [o_sld_brightness floatValue] / 100);
        } else if (sender == o_sld_contrast)
        {
            config_PutFloat( p_intf , "contrast" , [o_sld_contrast floatValue] / 100);
        } else if (sender == o_sld_gamma)
        {
            config_PutFloat( p_intf , "gamma" , [o_sld_gamma floatValue] / 10);
        } else if (sender == o_sld_hue)
        {
            config_PutInt( p_intf , "hue" , [o_sld_hue intValue]);
        } else if (sender == o_sld_saturation)
        {
            config_PutFloat( p_intf , "saturation" , [o_sld_saturation floatValue] / 100);
        } else {
            msg_Warn( p_intf, "cannot find adjust-image-subfilter related to " \
                "moved slider");
        }
    } else {
        vlc_value_t val;
        if (sender == o_sld_brightness)
        {
            val.f_float = [o_sld_brightness floatValue] / 100;
            var_Set( p_vout, "brightness", val );
            config_PutFloat( p_intf , "brightness" , [o_sld_brightness floatValue] / 100);
        } else if (sender == o_sld_contrast)
        {
            val.f_float = [o_sld_contrast floatValue] / 100;
            var_Set( p_vout, "contrast", val );
            config_PutFloat( p_intf , "contrast" , [o_sld_contrast floatValue] / 100);
        } else if (sender == o_sld_gamma)
        {
            val.f_float = [o_sld_gamma floatValue] / 10;
            var_Set( p_vout, "gamma", val );
            config_PutFloat( p_intf , "gamma" , [o_sld_gamma floatValue] / 10);
        } else if (sender == o_sld_hue)
        {
            val.i_int = [o_sld_hue intValue];
            var_Set( p_vout, "hue", val );
            config_PutInt( p_intf , "hue" , [o_sld_hue intValue]);
        } else if (sender == o_sld_saturation)
        {
            val.f_float = [o_sld_saturation floatValue] / 100;
            var_Set( p_vout, "saturation", val );
            config_PutFloat( p_intf , "saturation" , [o_sld_saturation floatValue] / 100);
        } else {
            msg_Warn( p_intf, "cannot find adjust-image-subfilter related to " \
                "moved slider");
        }
        vlc_object_release( p_vout );
    }
}

- (IBAction)adjImg_opaque:(id)sender
{
    /* change the opaqueness of the vouts */
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST, \
        FIND_ANYWHERE );
    vout_thread_t * p_vout = (vout_thread_t *)vlc_object_find( p_playlist, \
        VLC_OBJECT_VOUT, FIND_ANYWHERE );
    vout_thread_t * p_real_vout;
    
    vlc_value_t val;
    val.f_float = [o_sld_opaque floatValue] / 100;

    /* Try to set on the fly */
    if( p_vout )
    {
        if( p_vout->i_object_type == VLC_OBJECT_OPENGL )
        {
            p_real_vout = (vout_thread_t *) p_vout->p_parent;
        }
        else
        {
            p_real_vout = p_vout;
        }
        
        /* FIXME: insert the correct pointer here */
        /*[p_vout->p_sys->o_window setAlpha: var_CreateGetFloat( p_vout, \
            "macosx-opaqueness")];*/
        
        var_Set( p_vout, "macosx-opaqueness", val );
        vlc_object_release( p_real_vout );
        vlc_object_release( p_vout );
    }
    
    /* store to prefs */
    config_PutFloat( p_playlist , "macosx-opaqueness" , val.f_float );
    
    vlc_object_release( p_playlist );
}

- (IBAction)audFtls_hdphnVirt:(id)sender
{
    /* en-/disable headphone virtualisation */
    if ([o_ckb_hdphnVirt state] == NSOnState)
    {
        [self changeAFiltersString: "headphone" onOrOff: VLC_TRUE ];
    }else{
        [self changeAFiltersString: "headphone" onOrOff: VLC_FALSE ];
    }
}

- (IBAction)audFtls_maxLevelSld:(id)sender
{
    /* read-out the slider's value and apply it */
    intf_thread_t * p_intf = VLCIntf;
    aout_instance_t * p_aout= (aout_instance_t *)vlc_object_find(p_intf,
                                 VLC_OBJECT_AOUT, FIND_ANYWHERE);
    if( p_aout != NULL )
    {
        var_SetFloat( p_aout, "norm-max-level", [o_sld_maxLevel floatValue] / 10 );
        vlc_object_release( p_aout );
    }
    else
    {
        config_PutFloat( p_intf, "norm-max-level", [o_sld_maxLevel floatValue] /10 );
    }
}

- (IBAction)audFtls_vlmeNorm:(id)sender
{
    /* en-/disable volume normalisation */
    if ([o_ckb_vlme_norm state] == NSOnState)
    {
        [self changeAFiltersString: "normvol" onOrOff: YES ];
    }else{
        [self changeAFiltersString: "normvol" onOrOff: NO ];
    }
}

- (IBAction)extWin_exp_adjImg:(id)sender
{
    /* expand or collapse adjImg */
    NSRect o_win_rect = [o_extended_window frame];
    NSRect o_box_audFlts_rect = [o_box_audFlts frame];
    NSRect o_box_vidFlts_rect = [o_box_vidFlts frame];
    NSRect o_box_adjImg_rect = [o_box_adjImg frame];
    
    if (o_adjImg_expanded)
    {
        /* move the window contents upwards (partially done through settings
         * inside the nib) and resize the window */
        o_win_rect.size.height = o_win_rect.size.height - 171;
        o_win_rect.origin.y = [o_extended_window frame].origin.y + 171;
        o_box_audFlts_rect.origin.y = o_box_audFlts_rect.origin.y + 171;
        o_box_vidFlts_rect.origin.y = o_box_vidFlts_rect.origin.y + 171;
        
        /* remove the inserted view */
        [o_adjustImg_view removeFromSuperviewWithoutNeedingDisplay];
    }else{
    
        /* move the window contents downwards and resize the window */
        o_win_rect.size.height = o_win_rect.size.height + 171;
        o_win_rect.origin.y = [o_extended_window frame].origin.y - 171;
        o_box_audFlts_rect.origin.y = o_box_audFlts_rect.origin.y - 171;
        o_box_vidFlts_rect.origin.y = o_box_vidFlts_rect.origin.y - 171;
    }
    
    [o_box_audFlts setFrameFromContentFrame: o_box_audFlts_rect];
    [o_box_vidFlts setFrameFromContentFrame: o_box_vidFlts_rect];
    [o_extended_window displayIfNeeded];
    [o_extended_window setFrame: o_win_rect display:YES animate: YES];
    
    if (o_adjImg_expanded)
    {
        o_box_adjImg_rect.size.height = [o_box_adjImg frame].size.height - 171;
        msg_Dbg( VLCIntf, "collapsed adjust-image section");
        o_adjImg_expanded = NO;
    } else {
        /* insert view */
        o_box_adjImg_rect.size.height = [o_box_adjImg frame].size.height + 171;
        [o_adjustImg_view setFrame: NSMakeRect( 20, -10, 370, 181)];
        [o_adjustImg_view setNeedsDisplay:YES];
        [o_adjustImg_view setAutoresizesSubviews: YES];
        [[o_box_adjImg contentView] addSubview: o_adjustImg_view];
        msg_Dbg( VLCIntf, "expanded adjust-image section");
        o_adjImg_expanded = YES;
    }
    [o_box_adjImg setFrameFromContentFrame: o_box_adjImg_rect];
}

- (IBAction)extWin_exp_audFlts:(id)sender
{
    /* expand or collapse audFlts */
    NSRect o_win_rect = [o_extended_window frame];
    NSRect o_box_audFlts_rect = [o_box_audFlts frame];
    
    if (o_audFlts_expanded)
    {
        /* move the window contents upwards (partially done through settings
         * inside the nib) and resize the window */
        o_win_rect.size.height = o_win_rect.size.height - 66;
        o_win_rect.origin.y = [o_extended_window frame].origin.y + 66;
        
        /* remove the inserted view */
        [o_audioFlts_view removeFromSuperviewWithoutNeedingDisplay];
    }else{
        /* move the window contents downwards and resize the window */
        o_win_rect.size.height = o_win_rect.size.height + 66;
        o_win_rect.origin.y = [o_extended_window frame].origin.y - 66;
    }
    [o_extended_window displayIfNeeded];
    [o_extended_window setFrame: o_win_rect display:YES animate: YES];
    
    
    if (o_audFlts_expanded)
    {
        o_box_audFlts_rect.size.height = [o_box_audFlts frame].size.height - 66;
        msg_Dbg( VLCIntf, "collapsed audio-filters section");
        o_audFlts_expanded = NO;
    } else {
        /* insert view */
        o_box_audFlts_rect.size.height = [o_box_audFlts frame].size.height + 66;
        [o_audioFlts_view setFrame: NSMakeRect( 20, -20, 370, 76)];
        [o_audioFlts_view setNeedsDisplay:YES];
        [o_audioFlts_view setAutoresizesSubviews: YES];
        [[o_box_audFlts contentView] addSubview: o_audioFlts_view];
        msg_Dbg( VLCIntf, "expanded audio-filters section");
        o_audFlts_expanded = YES;
    }
    [o_box_audFlts setFrameFromContentFrame: o_box_audFlts_rect];
}

- (IBAction)extWin_exp_vidFlts:(id)sender
{
    /* expand or collapse vidFlts */
    NSRect o_win_rect = [o_extended_window frame];
    NSRect o_box_audFlts_rect = [o_box_audFlts frame];
    NSRect o_box_vidFlts_rect = [o_box_vidFlts frame];
    
    if (o_vidFlts_expanded)
    {
        /* move the window contents upwards (partially done through settings
         * inside the nib) and resize the window */
        o_win_rect.size.height = o_win_rect.size.height - 134;
        o_win_rect.origin.y = [o_extended_window frame].origin.y + 134;
        o_box_audFlts_rect.origin.y = o_box_audFlts_rect.origin.y + 134;
        
        /* remove the inserted view */
        [o_videoFilters_view removeFromSuperviewWithoutNeedingDisplay];
    }else{
    
        /* move the window contents downwards and resize the window */
        o_win_rect.size.height = o_win_rect.size.height + 134;
        o_win_rect.origin.y = [o_extended_window frame].origin.y - 134;
        o_box_audFlts_rect.origin.y = o_box_audFlts_rect.origin.y - 134;
    }
    
    [o_box_audFlts setFrameFromContentFrame: o_box_audFlts_rect];
    [o_extended_window displayIfNeeded];
    [o_extended_window setFrame: o_win_rect display:YES animate: YES];
    
    if (o_vidFlts_expanded)
    {
        o_box_vidFlts_rect.size.height = [o_box_vidFlts frame].size.height - 134;
        msg_Dbg( VLCIntf, "collapsed video-filters section");
        o_vidFlts_expanded = NO;
    } else {
        /* insert view */
        o_box_vidFlts_rect.size.height = [o_box_vidFlts frame].size.height + 134;
        [o_videoFilters_view setFrame: NSMakeRect( 20, -10, 370, 144)];
        [o_videoFilters_view setNeedsDisplay:YES];
        [o_videoFilters_view setAutoresizesSubviews: YES];
        [[o_box_vidFlts contentView] addSubview: o_videoFilters_view];
        msg_Dbg( VLCIntf, "expanded video-filters section");
        o_vidFlts_expanded = YES;
    }
    [o_box_vidFlts setFrameFromContentFrame: o_box_vidFlts_rect];
}

- (IBAction)vidFlts:(id)sender
{
    /* en-/disable video filters */
    if (sender == o_ckb_blur)
    {
        [self changeVFiltersString: "motionblur" onOrOff: [o_ckb_blur state]];
    }
    else if (sender == o_ckb_distortion)
    {
        [self changeVFiltersString: "distort" onOrOff: [o_ckb_distortion state]];
    }
    else if (sender == o_ckb_imgClone)
    {
        [self changeVFiltersString: "clone" onOrOff: [o_ckb_imgClone state]];
    }
    else if (sender == o_ckb_imgCrop)
    {
        [self changeVFiltersString: "crop" onOrOff: [o_ckb_imgCrop state]];
    }
    else if (sender == o_ckb_imgInvers)
    {
        [self changeVFiltersString: "invert" onOrOff: [o_ckb_imgInvers state]];
    }
    else if (sender == o_ckb_trnsform)
    {
        [self changeVFiltersString: "transform" onOrOff: [o_ckb_trnsform state]];
    } else {
        /* this shouldn't happen */
        msg_Warn (VLCIntf, "cannot find selected video-filter");
    }
}

- (IBAction)vidFlts_mrInfo:(id)sender
{
    /* show info sheet */
    NSBeginInformationalAlertSheet(_NS("More information"), _NS("OK"), @"", @"", \
        o_extended_window, nil, nil, nil, nil, _NS("Select the video effects " \
        "filters to apply. You must restart the stream for these settings to " \
        "take effect.\nTo configure the filters, go to the Preferences, and " \
        "go to Modules/Video Filters. You can then configure each filter.\n" \
        "If you want fine control over the filters ( to choose the order in " \
        "which they are applied ), you need to enter manually a filters " \
        "string (Preferences / Video / Filters)."));
}


/*****************************************************************************
 * methods to communicate changes to VLC's core
 *****************************************************************************/

- (void)changeVFiltersString:(char *)psz_name onOrOff:(vlc_bool_t )b_add 
{
    /* copied from ../wxwidgets/extrapanel.cpp
     * renamed to conform with Cocoa's rules */
     
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
    
    [self savePrefs];
}


- (void)changeAFiltersString: (char *)psz_name onOrOff: (vlc_bool_t )b_add;
{
    char *psz_parser, *psz_string;
    intf_thread_t * p_intf = VLCIntf;
    aout_instance_t * p_aout= (aout_instance_t *)vlc_object_find(p_intf,
                                 VLC_OBJECT_AOUT, FIND_ANYWHERE);

    if( p_aout )
    {
        psz_string = var_GetString( p_aout, "audio-filter" );
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
            p_aout->pp_inputs[i]->b_restart = VLC_TRUE;
            i = (i + 1);
        }
        vlc_object_release( p_aout );
    }
    free( psz_string );
    
    [self savePrefs];
}

- (void)savePrefs
{    
    /* save the preferences to make sure that our module-changes will up on
     * next launch again */
    playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST, \
        FIND_ANYWHERE );
    int returnedValue;
    
    returnedValue = config_SaveConfigFile( p_playlist, NULL);
    if (returnedValue == 0)
    {
        msg_Dbg(p_playlist, "VLCExtended: saved preferences successfully");
    } else {
        msg_Dbg(p_playlist, "VLCExtended: error while saving the preferences " \
            "(%i)" , returnedValue);
    }
    vlc_object_release( p_playlist );
}


/*****************************************************************************
 * delegate method
 *****************************************************************************/

- (BOOL)applicationShouldTerminate:(NSWindow *)sender
{
    /* save the prefs before shutting down */
    [self savePrefs];
    
    return YES;
}
@end
