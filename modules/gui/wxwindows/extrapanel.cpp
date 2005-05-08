/*****************************************************************************
 * extrapanel.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2004, 2003 VideoLAN
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <aout_internal.h>
#include <vlc/vout.h>
#include <vlc/intf.h>

#include <math.h>

#include "wxwindows.h"

/*****************************************************************************
 * Local class declarations.
 *****************************************************************************/

/* FIXME */
#define SMOOTH_TIP N_( "If this setting is not zero, the bands will move " \
                "together when you move one. The higher the value is, the " \
                "more correlated their movement will be." )

static int IntfBandsCallback( vlc_object_t *, char const *,
                              vlc_value_t, vlc_value_t, void * );
static int IntfPreampCallback( vlc_object_t *, char const *,
                               vlc_value_t, vlc_value_t, void * );
static void ChangeFiltersString( intf_thread_t *, aout_instance_t *,
                                 char *, vlc_bool_t );
static void ChangeVFiltersString( intf_thread_t *, char *, vlc_bool_t );


/* IDs for the controls and the menu commands */
enum
{
    Notebook_Event,

    Adjust_Event,
    RestoreDefaults_Event,

    Hue_Event,
    Contrast_Event,
    Brightness_Event,
    Saturation_Event,
    Gamma_Event,
    Ratio_Event,

    FiltersInfo_Event,

    Filter0_Event, Filter1_Event, Filter2_Event, Filter3_Event, Filter4_Event,
    Filter5_Event, Filter6_Event, Filter7_Event, Filter8_Event, Filter9_Event,

    EqEnable_Event,
    Eq2Pass_Event,
    EqRestore_Event,

    Smooth_Event,

    Preamp_Event,

    Band0_Event,Band1_Event,Band2_Event,Band3_Event,Band4_Event,
    Band5_Event,Band6_Event,Band7_Event,Band8_Event,Band9_Event,

    NormVol_Event, NVSlider_Event, HeadPhone_Event
};

BEGIN_EVENT_TABLE( ExtraPanel, wxPanel )
    EVT_IDLE( ExtraPanel::OnIdle )

    /* Equalizer */
    EVT_CHECKBOX( EqEnable_Event, ExtraPanel::OnEnableEqualizer )
    EVT_CHECKBOX( Eq2Pass_Event, ExtraPanel::OnEq2Pass )
    EVT_BUTTON( EqRestore_Event, ExtraPanel::OnEqRestore )

    EVT_COMMAND_SCROLL( Preamp_Event, ExtraPanel::OnPreamp )
    EVT_COMMAND_SCROLL( Smooth_Event, ExtraPanel::OnEqSmooth )

    EVT_COMMAND_SCROLL(Band0_Event, ExtraPanel::OnChangeEqualizer)
    EVT_COMMAND_SCROLL(Band1_Event, ExtraPanel::OnChangeEqualizer)
    EVT_COMMAND_SCROLL(Band2_Event, ExtraPanel::OnChangeEqualizer)
    EVT_COMMAND_SCROLL(Band3_Event, ExtraPanel::OnChangeEqualizer)
    EVT_COMMAND_SCROLL(Band4_Event, ExtraPanel::OnChangeEqualizer)
    EVT_COMMAND_SCROLL(Band5_Event, ExtraPanel::OnChangeEqualizer)
    EVT_COMMAND_SCROLL(Band6_Event, ExtraPanel::OnChangeEqualizer)
    EVT_COMMAND_SCROLL(Band7_Event, ExtraPanel::OnChangeEqualizer)
    EVT_COMMAND_SCROLL(Band8_Event, ExtraPanel::OnChangeEqualizer)
    EVT_COMMAND_SCROLL(Band9_Event, ExtraPanel::OnChangeEqualizer)

    /* Video */
    EVT_CHECKBOX( Adjust_Event, ExtraPanel::OnEnableAdjust )
    EVT_BUTTON( RestoreDefaults_Event, ExtraPanel::OnRestoreDefaults )

    EVT_COMMAND_SCROLL(Hue_Event, ExtraPanel::OnAdjustUpdate)
    EVT_COMMAND_SCROLL(Contrast_Event, ExtraPanel::OnAdjustUpdate)
    EVT_COMMAND_SCROLL(Brightness_Event, ExtraPanel::OnAdjustUpdate)
    EVT_COMMAND_SCROLL(Saturation_Event, ExtraPanel::OnAdjustUpdate)
    EVT_COMMAND_SCROLL(Gamma_Event, ExtraPanel::OnAdjustUpdate)

    EVT_BUTTON( FiltersInfo_Event, ExtraPanel::OnFiltersInfo )

    EVT_CHECKBOX( Filter0_Event, ExtraPanel::OnSelectFilter )
    EVT_CHECKBOX( Filter1_Event, ExtraPanel::OnSelectFilter )
    EVT_CHECKBOX( Filter2_Event, ExtraPanel::OnSelectFilter )
    EVT_CHECKBOX( Filter3_Event, ExtraPanel::OnSelectFilter )
    EVT_CHECKBOX( Filter4_Event, ExtraPanel::OnSelectFilter )
    EVT_CHECKBOX( Filter5_Event, ExtraPanel::OnSelectFilter )
    EVT_CHECKBOX( Filter6_Event, ExtraPanel::OnSelectFilter )
    EVT_CHECKBOX( Filter7_Event, ExtraPanel::OnSelectFilter )
    EVT_CHECKBOX( Filter8_Event, ExtraPanel::OnSelectFilter )
    EVT_CHECKBOX( Filter9_Event, ExtraPanel::OnSelectFilter )

    /* Audio */
    EVT_CHECKBOX( NormVol_Event, ExtraPanel::OnNormvol )
    EVT_CHECKBOX( HeadPhone_Event, ExtraPanel::OnHeadphone )

    EVT_COMMAND_SCROLL( NVSlider_Event, ExtraPanel::OnNormvolSlider )

END_EVENT_TABLE()

struct filter {
    char *psz_filter;
    char *psz_name;
    char *psz_help;
};

static const struct filter vfilters[] =
{
    { "clone", N_("Image clone"), N_("Creates several clones of the image") },
    { "distort", N_("Distortion"), N_("Adds distorsion effects") },
    { "invert", N_("Image inversion") , N_("Inverts the image colors") },
    { "crop", N_("Image cropping"), N_("Crops the image") },
    { "motionblur", N_("Blurring"), N_("Creates a motion blurring on the image") },
    { "transform",  N_("Transformation"), N_("Rotates or flips the image") },
    { NULL, NULL, NULL } /* Do not remove this line */
};

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
ExtraPanel::ExtraPanel( intf_thread_t *_p_intf, wxWindow *_p_parent ):
        wxPanel( _p_parent , -1, wxDefaultPosition, wxDefaultSize )
{

    p_intf = _p_intf;
    p_parent = _p_parent;
    SetAutoLayout( TRUE );

    wxBoxSizer *extra_sizer = new wxBoxSizer( wxHORIZONTAL );

    notebook = new wxNotebook( this, Notebook_Event );

#if (!wxCHECK_VERSION(2,5,0))
    wxNotebookSizer *notebook_sizer = new wxNotebookSizer( notebook );
#endif

    notebook->AddPage( VideoPanel( notebook ), wxU(_("Video")) );
    notebook->AddPage( EqzPanel( notebook ), wxU(_("Equalizer")) );
    notebook->AddPage( AudioPanel( notebook ), wxU(_("Audio")) );

#if (!wxCHECK_VERSION(2,5,0))
    extra_sizer->Add( notebook_sizer, 1, wxEXPAND, 0 );
#else
    extra_sizer->Add( notebook, 1, wxEXPAND, 0 );
#endif

    SetSizerAndFit( extra_sizer );
    extra_sizer->Layout();
}

ExtraPanel::~ExtraPanel()
{
}

/* Video Panel constructor */
wxPanel *ExtraPanel::VideoPanel( wxWindow *parent )
{
    char *psz_filters;

    wxPanel *panel = new wxPanel( parent, -1 );
    wxBoxSizer *panel_sizer = new wxBoxSizer( wxHORIZONTAL );

    /* Create static box to surround the adjust controls */
    wxStaticBox *adjust_box =
           new wxStaticBox( panel, -1, wxU(_("Adjust Image")) );
    wxStaticBoxSizer *adjust_sizer =
        new wxStaticBoxSizer( adjust_box, wxVERTICAL );
    adjust_sizer->SetMinSize( -1, 50 );

    /* Create flex grid */
    wxFlexGridSizer *adjust_gridsizer =
        new wxFlexGridSizer( 6, 2, 0, 0);
    adjust_gridsizer->AddGrowableCol(1);

    /* Create the adjust button */
    wxCheckBox * adjust_check = new wxCheckBox( panel, Adjust_Event,
                                                 wxU(_("Enable")));

    /* Create the restore to defaults button */
    restoredefaults_button =
        new wxButton( panel, RestoreDefaults_Event,
        wxU(_("Restore Defaults")), wxDefaultPosition);

    wxStaticText *hue_text = new wxStaticText( panel, -1,
                                       wxU(_("Hue")) );
    hue_slider = new wxSlider ( panel, Hue_Event, 0, 0,
                                360, wxDefaultPosition, wxDefaultSize );

    wxStaticText *contrast_text = new wxStaticText( panel, -1,
                                       wxU(_("Contrast")) );
    contrast_slider = new wxSlider ( panel, Contrast_Event, 0, 0,
                                200, wxDefaultPosition, wxDefaultSize);

    wxStaticText *brightness_text = new wxStaticText( panel, -1,
                                       wxU(_("Brightness")) );
    brightness_slider = new wxSlider ( panel, Brightness_Event, 0, 0,
                           200, wxDefaultPosition, wxDefaultSize) ;

    wxStaticText *saturation_text = new wxStaticText( panel, -1,
                                          wxU(_("Saturation")) );
    saturation_slider = new wxSlider ( panel, Saturation_Event, 0, 0,
                           300, wxDefaultPosition, wxDefaultSize );

    wxStaticText *gamma_text = new wxStaticText( panel, -1,
                                          wxU(_("Gamma")) );
    gamma_slider = new wxSlider ( panel, Gamma_Event, 0, 0,
                           100, wxDefaultPosition, wxDefaultSize );

    adjust_gridsizer->Add( adjust_check, 1, wxEXPAND|wxALL, 2 );
    adjust_gridsizer->Add( restoredefaults_button, 1, wxEXPAND|wxALL, 2 );
    adjust_gridsizer->Add( hue_text, 1, wxEXPAND|wxALL, 2 );
    adjust_gridsizer->Add( hue_slider, 1, wxEXPAND|wxALL, 2 );
    adjust_gridsizer->Add( contrast_text, 1, wxEXPAND|wxALL, 2 );
    adjust_gridsizer->Add( contrast_slider, 1, wxEXPAND|wxALL, 2 );
    adjust_gridsizer->Add( brightness_text, 1, wxEXPAND|wxALL, 2 );
    adjust_gridsizer->Add( brightness_slider, 1, wxEXPAND|wxALL, 2 );
    adjust_gridsizer->Add( saturation_text, 1, wxEXPAND|wxALL, 2 );
    adjust_gridsizer->Add( saturation_slider, 1, wxEXPAND|wxALL, 2 );
    adjust_gridsizer->Add( gamma_text, 1, wxEXPAND|wxALL, 2 );
    adjust_gridsizer->Add( gamma_slider, 1, wxEXPAND|wxALL, 2 );

    adjust_sizer->Add( adjust_gridsizer, 1, wxEXPAND|wxALL, 2);

    panel_sizer->Add( adjust_sizer , 1, wxTOP, 2 );

#if 0
    /* Create sizer to surround the other controls */
    wxBoxSizer *other_sizer = new wxBoxSizer( wxVERTICAL );

    wxStaticBox *video_box =
            new wxStaticBox( panel, -1, wxU(_("Video Options")) );
    /* Create the sizer for the frame */
    wxStaticBoxSizer *video_sizer =
       new wxStaticBoxSizer( video_box, wxVERTICAL );
    video_sizer->SetMinSize( -1, 50 );

    static const wxString ratio_array[] =
    {
        wxT("4:3"),
        wxT("16:9"),
    };

    wxBoxSizer *ratio_sizer = new wxBoxSizer( wxHORIZONTAL );
    wxStaticText *ratio_text = new wxStaticText( panel, -1,
                                          wxU(_("Aspect Ratio")) );

    ratio_combo = new wxComboBox( panel, Ratio_Event, wxT(""),
                                  wxDefaultPosition, wxSize( 80 , -1),
                                  WXSIZEOF(ratio_array), ratio_array,
                                  0 );

    ratio_sizer->Add( ratio_text, 0, wxALL, 2 );
    ratio_sizer->Add( ratio_combo, 0, wxALL, 2 );
    ratio_sizer->Layout();

    video_sizer->Add( ratio_sizer  , 0 , wxALL , 2 );
    video_sizer->Layout();
#endif

    wxStaticBox *filter_box =
                  new wxStaticBox( panel, -1, wxU(_("Video Filters")) );
    wxStaticBoxSizer *filter_sizer =
                   new wxStaticBoxSizer( filter_box, wxHORIZONTAL );

    wxBoxSizer *t_col_sizer = new wxBoxSizer( wxVERTICAL );


    for( int i = 0 ; vfilters[i].psz_filter != NULL ; i++ )
    {
        wxCheckBox *box = new wxCheckBox( panel, Filter0_Event + i,
                                          wxU( _( vfilters[i].psz_name ) ) );
        t_col_sizer->Add( box, 0, wxALL, 4 );
        box->SetToolTip( wxU( _( vfilters[i].psz_help ) ) );
    }

    filter_sizer->Add( t_col_sizer );
    filter_sizer->Add( new wxButton( panel, FiltersInfo_Event,
                            wxU(_("More info" ) ) ), 0, wxALL, 4 );
#if 0
    other_sizer->Add( video_sizer, 0, wxALL | wxEXPAND , 0);
    other_sizer->Add( filter_sizer, 0, wxALL | wxEXPAND , 0);
    other_sizer->Layout();
    panel_sizer->Add(other_sizer , 1 );
#endif

    panel_sizer->Add( filter_sizer, 1, wxTOP|wxLEFT, 2 );

    panel->SetSizerAndFit( panel_sizer );

    /* Layout the whole panel */
    panel_sizer->Layout();

    panel_sizer->SetSizeHints( panel );

    /* Write down initial values */
    psz_filters = config_GetPsz( p_intf, "vout-filter" );
    if( psz_filters && strstr( psz_filters, "adjust" ) )
    {
        adjust_check->SetValue( 1 );
        restoredefaults_button->Enable();
        saturation_slider->Enable();
        contrast_slider->Enable();
        brightness_slider->Enable();
        hue_slider->Enable();
        gamma_slider->Enable();
    }
    else
    {
        adjust_check->SetValue( 0 );
        restoredefaults_button->Disable();
        saturation_slider->Disable();
        contrast_slider->Disable();
        brightness_slider->Disable();
        hue_slider->Disable();
        gamma_slider->Disable();
    }
    if( psz_filters ) free( psz_filters );

    int i_value = config_GetInt( p_intf, "hue" );
    if( i_value > 0 && i_value < 360 )
        hue_slider->SetValue( i_value );
    float f_value;
    f_value = config_GetFloat( p_intf, "saturation" );
    if( f_value > 0 && f_value < 5 )
        saturation_slider->SetValue( (int)(100 * f_value) );
    f_value = config_GetFloat( p_intf, "contrast" );
    if( f_value > 0 && f_value < 4 )
        contrast_slider->SetValue( (int)(100 * f_value) );
    f_value = config_GetFloat( p_intf, "brightness" );
    if( f_value > 0 && f_value < 2 )
        brightness_slider->SetValue( (int)(100 * f_value) );
    f_value = config_GetFloat( p_intf, "gamma" );
    if( f_value > 0 && f_value < 10 )
        gamma_slider->SetValue( (int)(10 * f_value) );

    b_update = VLC_FALSE;

    return panel;
}

/* Audio panel constructor */
wxPanel *ExtraPanel::AudioPanel( wxWindow *parent )
{
    char *psz_filters;

    wxPanel *panel = new wxPanel( parent, -1 );
    wxBoxSizer *panel_sizer = new wxBoxSizer( wxHORIZONTAL );

    /* Create static box to surround the adjust controls */
    wxStaticBox *filter_box =
           new wxStaticBox( panel, -1, wxU(_("Audio filters")) );
    wxStaticBoxSizer *filter_sizer =
        new wxStaticBoxSizer( filter_box, wxVERTICAL );
    filter_sizer->SetMinSize( -1, 50 );

    wxCheckBox * headphone_check = new wxCheckBox( panel, HeadPhone_Event,
                                    wxU(_("Headphone virtualization")));
    headphone_check->SetToolTip( wxU(_("This filter gives the feeling of a "
             "5.1 speaker set when using a headphone." ) ) );

    wxCheckBox * normvol_check = new wxCheckBox( panel, NormVol_Event,
                                    wxU(_("Volume normalization")));
    normvol_check->SetToolTip( wxU(_("This filter prevents the audio output "
                         "power from going over a defined value." ) ) );

    wxStaticText *normvol_label = new wxStaticText( panel, -1,
                                   wxU( _("Maximum level") ) );

    wxSlider *normvol_slider = new wxSlider ( panel, NVSlider_Event, 20, 5,
                           100, wxDefaultPosition, wxSize( 100, -1 ) );

    filter_sizer->Add( headphone_check, 0, wxALL, 4 );
    filter_sizer->Add( normvol_check, 0, wxALL, 4 );
    filter_sizer->Add( normvol_label, 0, wxALL, 4 );
    filter_sizer->Add( normvol_slider, 0, wxALL, 4 );

    panel_sizer->Add( filter_sizer, 1, wxTOP, 2 );
    panel->SetSizerAndFit( panel_sizer );
    panel_sizer->Layout();
    panel_sizer->SetSizeHints( panel );

    /* Write down initial values */
    psz_filters = config_GetPsz( p_intf, "audio-filter" );
    if( psz_filters )
    {
        headphone_check->SetValue( strstr( psz_filters, "headphone" ) );
        normvol_check->SetValue( strstr( psz_filters, "normvol" ) );
        free( psz_filters );
    }
    else
    {
        headphone_check->SetValue( 0 );
        normvol_check->SetValue( 0 );
    }

    return panel;
}


static const wxString band_frequencies[] =
{
    wxT(" 60 Hz"),
    wxT("170 Hz"),
    wxT("310 Hz"),
    wxT("600 Hz"),
    wxT(" 1 kHz"),
    wxT(" 3 kHz"),
    wxT(" 6 kHz"),
    wxT("12 kHz"),
    wxT("14 kHz"),
    wxT("16 kHz")
};

/* Equalizer Panel constructor */
wxPanel *ExtraPanel::EqzPanel( wxWindow *parent )
{
    wxPanel *panel = new wxPanel( parent, -1 );
    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );

    /* Create static box to surround the adjust controls */
    wxBoxSizer *top_sizer =
        new wxBoxSizer( wxHORIZONTAL );

    /* Create the enable button */
    eq_chkbox =  new wxCheckBox( panel, EqEnable_Event,
                            wxU(_("Enable") ) );
    eq_chkbox->SetToolTip( wxU(_("Enable the equalizer. You can either "
    "manually change the bands or use a preset (Audio Menu->Equalizer)." ) ) );
    top_sizer->Add( eq_chkbox, 0, wxALL, 2 );

    eq_2p_chkbox =  new wxCheckBox( panel, Eq2Pass_Event,
                            wxU(_("2 Pass") ) );

    eq_2p_chkbox->SetToolTip( wxU(_("If you enable this settting, the "
     "equalizer filter will be applied twice. The effect will be sharper.") ) );

    top_sizer->Add( eq_2p_chkbox, 0, wxALL, 2 );

    top_sizer->Add( 0, 0, 1, wxALL, 2 );

    eq_restoredefaults_button = new wxButton( panel, EqRestore_Event,
                                  wxU( _("Restore Defaults") ) );
    top_sizer->Add( eq_restoredefaults_button, 0, wxALL, 2 );
    top_sizer->Add( 0, 0, 1, wxALL, 2 );

    smooth_text = new wxStaticText( panel, -1, wxU( "Smooth :" ));
    smooth_text->SetToolTip( wxU( SMOOTH_TIP ) );
    top_sizer->Add( smooth_text, 0, wxALL, 2 );

    smooth_slider =new wxSlider( panel, Smooth_Event, 0, 0, 10 ,
                    wxDefaultPosition, wxSize( 100, -1 ) );
    smooth_slider->SetToolTip( wxU( SMOOTH_TIP ) );
    top_sizer->Add( smooth_slider, 0, wxALL, 2 );
    i_smooth = 0;

    /* Create flex grid */
    wxFlexGridSizer *eq_gridsizer =
        new wxFlexGridSizer( 2, 12, 0, 0);
    eq_gridsizer->AddGrowableRow( 0 );
    eq_gridsizer->AddGrowableCol( 1 );

    preamp_slider = new wxSlider( panel, Preamp_Event, 80, 0, 400,
                    wxDefaultPosition, wxSize( -1 , 90 ) , wxSL_VERTICAL );
    eq_gridsizer->Add( preamp_slider, 1, wxEXPAND|wxALL, 2 );

    eq_gridsizer->Add( 0, 0, 1, wxALL, 2 );

    for( int i = 0 ; i < 10 ; i++ )
    {
        band_sliders[i] = new wxSlider( panel, Band0_Event + i, 200, 0, 400,
                    wxDefaultPosition, wxSize( -1 , 90 ) , wxSL_VERTICAL );

        i_values[i] = 200;
        eq_gridsizer->Add( band_sliders[i], 1, wxEXPAND|wxALL, 2 );
    }

    preamp_text = new wxStaticText( panel, -1, wxT( "Preamp\n12.0dB" ) );
    wxFont font= preamp_text->GetFont();
    font.SetPointSize(7);
    preamp_text->SetFont( font );
    eq_gridsizer->Add( preamp_text, wxALL, 2 );

    eq_gridsizer->Add( 0, 0, 1 );

    for( int i = 0 ; i < 10 ; i++ )
    {
        band_texts[i] = new wxStaticText( panel, -1,
                                band_frequencies[i] + wxU("\n0.0dB" ) ) ;
        eq_gridsizer->Add( band_texts[i], 1, wxEXPAND|wxALL, 2 );
        wxFont font= band_texts[i]->GetFont();
        font.SetPointSize(7);
        band_texts[i]->SetFont( font );
    }

    panel_sizer->Add( top_sizer , 0 , wxTOP | wxEXPAND, 5 );
    panel_sizer->Add( eq_gridsizer , 0 , wxEXPAND, 0 );

    panel->SetSizer( panel_sizer );

    panel_sizer->Layout();

    panel_sizer->SetSizeHints( panel );

    CheckAout();

    eq_2p_chkbox->Disable();
    eq_restoredefaults_button->Disable();
    smooth_slider->Disable();
    smooth_text->Disable();
    preamp_slider->Disable();
    preamp_text->Disable();
    for( int i_index=0; i_index < 10; i_index++ )
    {
        band_sliders[i_index]->Disable();
        band_texts[i_index]->Disable();
    }

    return panel;
}

/*******************************************************
 * Event handlers
 *******************************************************/

/* Keep aout up to date and update the bands if needed */
void ExtraPanel::OnIdle( wxIdleEvent &event )
{
    CheckAout();
    if( b_update == VLC_TRUE )
    {
        if( b_my_update == VLC_TRUE )
        {
            b_update = b_my_update = VLC_FALSE;
            return;
        }
        char *p = psz_bands;
        for( int i = 0; i < 10; i++ )
        {
                float f;
                char psz_val[5];
                int i_val;
                /* Read dB -20/20*/
                f = strtof( p, &p );
                i_val= (int)( ( f + 20 ) * 10 );
                band_sliders[i]->SetValue( 400 - i_val );
                i_values[i] = 400 - i_val;
                sprintf( psz_val, "%.1f", f );
                band_texts[i]->SetLabel( band_frequencies[i] + wxT("\n") +
                                                wxU( psz_val ) + wxT("dB") );
                if( p == NULL )
                {
                    break;
                }
                p++;
                if( *p == 0 )
                    break;
        }
        char psz_val[5];
        int i_val = (int)( ( f_preamp + 20 ) * 10 );
        sprintf( psz_val, "%.1f", f_preamp );
        preamp_slider->SetValue( 400 - i_val );
        const wxString preamp = wxT("Preamp\n");
        preamp_text->SetLabel( preamp + wxU( psz_val ) + wxT( "dB" ) );
        eq_chkbox->SetValue( TRUE );
        b_update = VLC_FALSE;
    }
}

/*************************
 *  Equalizer Panel events
 *************************/
void ExtraPanel::OnEnableEqualizer( wxCommandEvent &event )
{
    int i_index;
    aout_instance_t *p_aout= (aout_instance_t *)vlc_object_find(p_intf,
                                 VLC_OBJECT_AOUT, FIND_ANYWHERE);
    ChangeFiltersString( p_intf,p_aout, "equalizer",
                         event.IsChecked() ? VLC_TRUE : VLC_FALSE );

    if( event.IsChecked() )
    {
        eq_2p_chkbox->Enable();
        eq_restoredefaults_button->Enable();
        smooth_slider->Enable();
        smooth_text->Enable();
        preamp_slider->Enable();
        preamp_text->Enable();
        for( i_index=0; i_index < 10; i_index++ )
        {
            band_sliders[i_index]->Enable();
            band_texts[i_index]->Enable();
        }
    } else {
        eq_2p_chkbox->Disable();
        eq_restoredefaults_button->Disable();
        smooth_slider->Disable();
        smooth_text->Disable();
        preamp_slider->Disable();
        preamp_text->Disable();
        for( i_index=0; i_index < 10; i_index++ )
        {
            band_sliders[i_index]->Disable();
            band_texts[i_index]->Disable();
        }
    }

    if( p_aout != NULL )
        vlc_object_release( p_aout );
}

void ExtraPanel::OnEqRestore( wxCommandEvent &event )
{
    aout_instance_t *p_aout= (aout_instance_t *)vlc_object_find(p_intf,
                                 VLC_OBJECT_AOUT, FIND_ANYWHERE);
    if( p_aout == NULL )
    {
        vlc_value_t val;
        vlc_bool_t b_previous = eq_chkbox->IsChecked();
        val.f_float = 12.0;
        IntfPreampCallback( NULL, NULL, val,val, this );
        config_PutFloat( p_intf, "equalizer-preamp", 12.0 );
        val.psz_string = strdup( "0 0 0 0 0 0 0 0 0 0" );
        IntfBandsCallback( NULL, NULL, val,val, this );
        config_PutPsz( p_intf, "equalizer-bands",
                                "0 0 0 0 0 0 0 0 0 0");
        config_PutPsz( p_intf, "equalizer-preset","flat" );
        eq_chkbox->SetValue( b_previous );
    }
    else
    {
        var_SetFloat( p_aout, "equalizer-preamp", 12.0 );
        config_PutFloat( p_intf, "equalizer-preamp", 12.0 );
        var_SetString( p_aout, "equalizer-bands",
                                "0 0 0 0 0 0 0 0 0 0");
        config_PutPsz( p_intf, "equalizer-bands",
                                "0 0 0 0 0 0 0 0 0 0");
        var_SetString( p_aout , "equalizer-preset" , "flat" );
        config_PutPsz( p_intf, "equalizer-preset","flat" );
        vlc_object_release( p_aout );
    }
}

void ExtraPanel::OnEq2Pass( wxCommandEvent &event )
{
    aout_instance_t *p_aout= (aout_instance_t *)vlc_object_find(p_intf,
                                 VLC_OBJECT_AOUT, FIND_ANYWHERE);

    vlc_bool_t b_2p = event.IsChecked() ? VLC_TRUE : VLC_FALSE;

    if( p_aout == NULL )
    {
        config_PutInt( p_intf, "equalizer-2pass", b_2p );
    }
    else
    {
        var_SetBool( p_aout, "equalizer-2pass", b_2p );
        config_PutInt( p_intf, "equalizer-2pass", b_2p );
        if( eq_chkbox->IsChecked() )
        {
            for( int i = 0; i < p_aout->i_nb_inputs; i++ )
            {
                p_aout->pp_inputs[i]->b_restart = VLC_TRUE;
            }
        }
        vlc_object_release( p_aout );
    }
}

void ExtraPanel::OnEqSmooth( wxScrollEvent &event )
{
    /* Max smoothing : 70% */
    i_smooth = event.GetPosition() * 7;
}

void ExtraPanel::OnPreamp( wxScrollEvent &event )
{
    float f= (float)( 400 - event.GetPosition() ) / 10 - 20 ;
    char psz_val[5];

    aout_instance_t *p_aout= (aout_instance_t *)vlc_object_find(p_intf,
                                 VLC_OBJECT_AOUT, FIND_ANYWHERE);

    sprintf( psz_val, "%.1f", f );
    const wxString preamp = wxT("Preamp\n");
    preamp_text->SetLabel( preamp + wxU( psz_val ) + wxT( "dB" ) );

    if( p_aout == NULL )
    {
        config_PutFloat( p_intf, "equalizer-preamp", f );
    }
    else
    {
        var_SetFloat( p_aout, "equalizer-preamp", f );
        config_PutFloat( p_intf, "equalizer-preamp", f );
        b_my_update = VLC_TRUE;
        vlc_object_release( p_aout );
    }
}

void ExtraPanel::OnChangeEqualizer( wxScrollEvent &event )
{
    aout_instance_t *p_aout= (aout_instance_t *)vlc_object_find(p_intf,
                                 VLC_OBJECT_AOUT, FIND_ANYWHERE);
    char psz_values[102];
    memset( psz_values, 0, 102 );


    /* Smoothing */
    int i_diff = event.GetPosition() - i_values[  event.GetId() - Band0_Event ];
    i_values[ event.GetId() - Band0_Event] = event.GetPosition();

    for( int i = event.GetId() + 1 ; i <= Band9_Event ; i++ )
    {
        int i_new = band_sliders[ i-Band0_Event ]->GetValue() +
           (int)( i_diff * pow( (float)i_smooth / 100 , i- event.GetId() ) ) ;
        if( i_new < 0 ) i_new = 0;
        if( i_new > 400 ) i_new = 400;
        band_sliders[ i-Band0_Event ]->SetValue( i_new );
    }
    for( int i = Band0_Event ; i < event.GetId() ; i++ )
    {
        int i_new =   band_sliders[ i-Band0_Event ]->GetValue() +
           (int)( i_diff * pow( (float)i_smooth / 100 , event.GetId() - i  ) );
        if( i_new < 0 ) i_new = 0;
        if( i_new > 400 ) i_new = 400;
        band_sliders[ i-Band0_Event ]->SetValue( i_new );
    }

    /* Write the new bands values */
    for( int i = 0 ; i < 10 ; i++ )
    {
        char psz_val[5];
        float f_val = (float)( 400 - band_sliders[i]->GetValue() ) / 10- 20 ;
        sprintf( psz_values, "%s %f", psz_values, f_val );
        sprintf( psz_val, "%.1f", f_val );
        band_texts[i]->SetLabel( band_frequencies[i] + wxT("\n") +
                        wxU( psz_val ) + wxT("dB" ) );
    }
    if( p_aout == NULL )
    {
        config_PutPsz( p_intf, "equalizer-bands", psz_values );
    }
    else
    {
        var_SetString( p_aout, "equalizer-bands", psz_values );
        config_PutPsz( p_intf, "equalizer-bands", psz_values );
        b_my_update = VLC_TRUE;
        vlc_object_release( p_aout );
    }
}

/***********************
 * Audio Panel events
 ***********************/
void ExtraPanel::OnHeadphone( wxCommandEvent &event )
{
    aout_instance_t *p_aout= (aout_instance_t *)vlc_object_find(p_intf,
                                 VLC_OBJECT_AOUT, FIND_ANYWHERE);
    ChangeFiltersString( p_intf , p_aout, "headphone",
                         event.IsChecked() ? VLC_TRUE : VLC_FALSE );
    if( p_aout != NULL )
        vlc_object_release( p_aout );
}

void ExtraPanel::OnNormvol( wxCommandEvent &event )
{
    aout_instance_t *p_aout= (aout_instance_t *)vlc_object_find(p_intf,
                                 VLC_OBJECT_AOUT, FIND_ANYWHERE);
    ChangeFiltersString( p_intf , p_aout, "normvol",
                         event.IsChecked() ? VLC_TRUE : VLC_FALSE );
    if( p_aout != NULL )
        vlc_object_release( p_aout );
}

void ExtraPanel::OnNormvolSlider( wxScrollEvent &event )
{
    aout_instance_t *p_aout= (aout_instance_t *)vlc_object_find(p_intf,
                                 VLC_OBJECT_AOUT, FIND_ANYWHERE);
    if( p_aout != NULL )
    {
        var_SetFloat( p_aout, "norm-max-level", (float)event.GetPosition()/10 );
        vlc_object_release( p_aout );
    }
    else
    {
        config_PutFloat( p_intf, "norm-max-level",
                        (float)event.GetPosition()/10 );
    }
}
/***********************
 *  Video Panel events
 ***********************/
void ExtraPanel::OnEnableAdjust(wxCommandEvent& event)
{
    ChangeVFiltersString( p_intf,  "adjust",
                          event.IsChecked() ? VLC_TRUE : VLC_FALSE );

    if( event.IsChecked() )
    {
        restoredefaults_button->Enable();
        brightness_slider->Enable();
        saturation_slider->Enable();
        contrast_slider->Enable();
        hue_slider->Enable();
        gamma_slider->Enable();
    }
    else
    {
        restoredefaults_button->Disable();
        brightness_slider->Disable();
        saturation_slider->Disable();
        contrast_slider->Disable();
        hue_slider->Disable();
        gamma_slider->Disable();
    }
}

void ExtraPanel::OnRestoreDefaults( wxCommandEvent &event)
{
    hue_slider->SetValue(0);
    saturation_slider->SetValue(100);
    brightness_slider->SetValue(100);
    contrast_slider->SetValue(100),
    gamma_slider->SetValue(10);

    wxScrollEvent *hscroll_event = new wxScrollEvent(0, Hue_Event, 0);
    OnAdjustUpdate(*hscroll_event);

    wxScrollEvent *sscroll_event = new wxScrollEvent(0, Saturation_Event, 100);
    OnAdjustUpdate(*sscroll_event);

    wxScrollEvent *bscroll_event = new wxScrollEvent(0, Brightness_Event, 100);
    OnAdjustUpdate(*bscroll_event);

    wxScrollEvent *cscroll_event = new wxScrollEvent(0, Contrast_Event, 100);
    OnAdjustUpdate(*cscroll_event);

    wxScrollEvent *gscroll_event = new wxScrollEvent(0, Gamma_Event, 10);
    OnAdjustUpdate(*gscroll_event);

}

void ExtraPanel::OnAdjustUpdate( wxScrollEvent &event)
{
    vout_thread_t *p_vout = (vout_thread_t *)vlc_object_find(p_intf,
                                 VLC_OBJECT_VOUT, FIND_ANYWHERE);
    if( p_vout == NULL )
    {
        switch( event.GetId() )
        {
            case Hue_Event:
                config_PutInt( p_intf , "hue" , event.GetPosition() );
                break;

            case Saturation_Event:
                config_PutFloat( p_intf , "saturation" ,
                                (float)event.GetPosition()/100 );
                break;

            case Brightness_Event:
                config_PutFloat( p_intf , "brightness" ,
                                (float)event.GetPosition()/100 );
                break;

            case Contrast_Event:
                config_PutFloat( p_intf , "contrast" ,
                                (float)event.GetPosition()/100 );
                break;

            case Gamma_Event:
                config_PutFloat( p_intf , "gamma" ,
                                (float)event.GetPosition()/10 );
                break;
        }
    }
    else
    {
        vlc_value_t val;
        switch( event.GetId() )
        {
            case Hue_Event:
                val.i_int = event.GetPosition();
                var_Set( p_vout, "hue", val );
                config_PutInt( p_intf , "hue" , event.GetPosition() );
                break;

            case Saturation_Event:
                val.f_float = (float)event.GetPosition() / 100;
                var_Set( p_vout, "saturation", val );
                config_PutFloat( p_intf , "saturation" ,
                                (float)event.GetPosition()/100 );
                break;

            case Brightness_Event:
                val.f_float = (float)event.GetPosition() / 100;
                var_Set( p_vout, "brightness", val );
                config_PutFloat( p_intf , "brightness" ,
                                (float)event.GetPosition()/100 );
                break;

            case Contrast_Event:
                val.f_float = (float)event.GetPosition() / 100;
                var_Set( p_vout, "contrast", val );
                config_PutFloat( p_intf , "contrast" ,
                                (float)event.GetPosition()/100 );
                break;

            case Gamma_Event:
                val.f_float = (float)event.GetPosition() / 10;
                var_Set( p_vout, "gamma", val );
                config_PutFloat( p_intf , "gamma" ,
                                (float)event.GetPosition()/10 );
                break;
        }
        vlc_object_release(p_vout);
    }
}

/* FIXME */
void ExtraPanel::OnRatio( wxCommandEvent& event )
{
   config_PutPsz( p_intf, "aspect-ratio", ratio_combo->GetValue().mb_str() );
}


void ExtraPanel::OnSelectFilter(wxCommandEvent& event)
{
    int i_filter = event.GetId() - Filter0_Event ;
    if( vfilters[i_filter].psz_filter  )
    {
        ChangeVFiltersString( p_intf, vfilters[i_filter].psz_filter ,
                              event.IsChecked() ? VLC_TRUE : VLC_FALSE );
    }
}

void ExtraPanel::OnFiltersInfo(wxCommandEvent& event)
{
    wxMessageBox( wxU( _("Select the video effects filters to apply. "
                  "You must restart the stream for these settings to "
                  "take effect.\n"
                  "To configure the filters, go to the Preferences, "
                  "and go to Modules/Video Filters. "
                  "You can then configure each filter.\n"
                  "If you want fine control over the filters ( to choose "
                  "the order in which they are applied ), you need to enter "
                  "manually a filters string (Preferences / General / Video)."
                  ) ),
                    wxU( _("More information" ) ), wxOK | wxICON_INFORMATION,
                    this->p_parent );
}
/**********************************
 * Other functions
 **********************************/
void ExtraPanel::CheckAout()
{
    aout_instance_t *p_aout= (aout_instance_t *)vlc_object_find(p_intf,
                                 VLC_OBJECT_AOUT, FIND_ANYWHERE);
    if( p_aout != NULL )
    {
        if( p_aout != p_intf->p_sys->p_aout )
        {
            /* We want to know if someone changes the bands */
            if( var_AddCallback( p_aout, "equalizer-bands",
                                    IntfBandsCallback, this ) )
            {
                /* The variable does not exist yet, wait */
                vlc_object_release( p_aout );
                return;
            }
            if( var_AddCallback( p_aout, "equalizer-preamp",
                                    IntfPreampCallback, this )  )
            {
                vlc_object_release( p_aout );
                return;
            }
            /* Ok, we have our variables, make a first update round */
            p_intf->p_sys->p_aout = p_aout;

            f_preamp = var_GetFloat( p_aout, "equalizer-preamp" );
            psz_bands = var_GetString( p_aout, "equalizer-bands" );
            b_update = VLC_TRUE;
        }
        vlc_object_release( p_aout );
    }
}


static void ChangeVFiltersString( intf_thread_t *p_intf,
                                 char *psz_name, vlc_bool_t b_add )
{
    vout_thread_t *p_vout;
    char *psz_parser, *psz_string;

    psz_string = config_GetPsz( p_intf, "vout-filter" );

    if( !psz_string ) psz_string = strdup("");

    psz_parser = strstr( psz_string, psz_name );

    if( b_add )
    {
        if( !psz_parser )
        {
            psz_parser = psz_string;
            asprintf( &psz_string, (*psz_string) ? "%s,%s" : "%s%s",
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
                            (*(psz_parser + strlen(psz_name)) == ',' ? 1 : 0 ),
                            strlen(psz_parser + strlen(psz_name)) + 1 );

            /* Remove trailing : : */
            if( *(psz_string+strlen(psz_string ) -1 ) == ',' )
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
}


static void ChangeFiltersString( intf_thread_t *p_intf,
                                 aout_instance_t * p_aout,
                                 char *psz_name, vlc_bool_t b_add )
{
    char *psz_parser, *psz_string;

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
            asprintf( &psz_string, (*psz_string) ? "%s,%s" : "%s%s",
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
                            (*(psz_parser + strlen(psz_name)) == ',' ? 1 : 0 ),
                            strlen(psz_parser + strlen(psz_name)) + 1 );

            if( *(psz_string+strlen(psz_string ) -1 ) == ',' )
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
        for( int i = 0; i < p_aout->i_nb_inputs; i++ )
        {
            p_aout->pp_inputs[i]->b_restart = VLC_TRUE;
        }
    }
    free( psz_string );
}


static int IntfBandsCallback( vlc_object_t *p_this, char const *psz_cmd,
                          vlc_value_t oldval, vlc_value_t newval, void *param )
{
    ExtraPanel *p_panel = (ExtraPanel *)param;

    p_panel->psz_bands = strdup( newval.psz_string );
    p_panel->b_update = VLC_TRUE;

    return VLC_SUCCESS;
}

static int IntfPreampCallback( vlc_object_t *p_this, char const *psz_cmd,
                          vlc_value_t oldval, vlc_value_t newval, void *param )
{
    ExtraPanel *p_panel = (ExtraPanel *)param;

    p_panel->f_preamp = newval.f_float;
    p_panel->b_update = VLC_TRUE;

    return VLC_SUCCESS;
}

#if 0
/**********************************************************************
 * A small window to contain the extrapanel in its undocked state
 **********************************************************************/
BEGIN_EVENT_TABLE(ExtraWindow, wxFrame)
END_EVENT_TABLE()


ExtraWindow::ExtraWindow( intf_thread_t *_p_intf, wxWindow *p_parent,
                          wxPanel *_extra_panel ):
       wxFrame( p_parent, -1, wxU(_("Extended controls")), wxDefaultPosition,
                 wxDefaultSize, wxDEFAULT_FRAME_STYLE )
{
        fprintf(stderr,"Creating extrawindow\n");
    p_intf = _p_intf;
    SetIcon( *p_intf->p_sys->p_icon );

    wxBoxSizer *window_sizer = new wxBoxSizer( wxVERTICAL );
    SetSizer( window_sizer );
//    panel = new ExtraPanel(  p_intf, this );//_extra_panel;

    panel = _extra_panel;
    window_sizer->Add( panel );

    window_sizer->Layout();
    window_sizer->Fit( this );

    Show();
}

ExtraWindow::~ExtraWindow()
{
    delete panel;
}
#endif
