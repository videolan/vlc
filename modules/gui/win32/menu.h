/*****************************************************************************
 * menu.h: prototypes for menu functions
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: menu.h,v 1.5 2003/02/12 02:11:58 ipkiss Exp $
 *
 * Authors: Olivier Teuliere <ipkiss@via.ecp.fr>
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

#ifndef menuH
#define menuH
//----------------------------------------------------------------------------
class TMenusGen : public TObject
{
private:
    intf_thread_t *p_intf;

    /* local pointers to main window menu items */
    TMenuItem *MenuChannel;
    TMenuItem *PopupChannel;
    TMenuItem *MenuADevice;
    TMenuItem *PopupADevice;
    TMenuItem *MenuVDevice;
    TMenuItem *PopupVDevice;
    TMenuItem *MenuLanguage;
    TMenuItem *PopupLanguage;
    TMenuItem *MenuSubtitles;
    TMenuItem *PopupSubtitles;
    TMenuItem *MenuProgram;
    TMenuItem *PopupProgram;
    TMenuItem *MenuTitle;
    TMenuItem *MenuChapter;
    TMenuItem *PopupNavigation;
    TMenuItem *MenuAddInterface;

    /* Language information */
    es_descriptor_t   * p_audio_es_old;
    es_descriptor_t   * p_spu_es_old;

    /* Helpful functions */
    int Item2Index( TMenuItem *Root, TMenuItem *Item );
    TMenuItem *Index2Item( TMenuItem *Root, int i_index, bool SingleColumn );
    int __fastcall Data2Title( int data );
    int __fastcall Data2Chapter( int data );
    int __fastcall Pos2Data( int title, int chapter );
    AnsiString __fastcall TMenusGen::CleanCaption( AnsiString Caption );

    void __fastcall VarChange( vlc_object_t *, const char *, TMenuItem *,
                               TMenuItem *, TMenuItem * );
    void __fastcall LangChange( TMenuItem *, TMenuItem *, TMenuItem *, int );
    void __fastcall ProgramChange( TMenuItem *, TMenuItem * );

    void __fastcall SetupVarMenu( vlc_object_t *, const char *, TMenuItem *,
                                  TNotifyEvent );
    void __fastcall SetupModuleMenu( const char *, TMenuItem *, TNotifyEvent );
    void __fastcall ProgramMenu( TMenuItem *, pgrm_descriptor_t *,
                                 TNotifyEvent );
    void __fastcall RadioMenu( TMenuItem *, AnsiString, int, int,
                               TNotifyEvent );
    void __fastcall LanguageMenu( TMenuItem *, es_descriptor_t *, int,
                                  TNotifyEvent );
    void __fastcall NavigationMenu( TMenuItem *, TNotifyEvent );
public:
    __fastcall TMenusGen( intf_thread_t *_p_intf );

    /* menu generation */
    void __fastcall SetupMenus();

    /* callbacks for menuitems */
    void __fastcall AoutVarClick( TObject *Sender );
    void __fastcall VoutVarClick( TObject *Sender );
    void __fastcall InterfaceModuleClick( TObject *Sender );
    void __fastcall MenuLanguageClick( TObject *Sender );
    void __fastcall PopupLanguageClick( TObject *Sender );
    void __fastcall MenuSubtitleClick( TObject *Sender );
    void __fastcall PopupSubtitleClick( TObject *Sender );
    void __fastcall MenuProgramClick( TObject *Sender );
    void __fastcall PopupProgramClick( TObject *Sender );
    void __fastcall MenuTitleClick( TObject *Sender );
    void __fastcall MenuChapterClick( TObject *Sender );
    void __fastcall PopupNavigationClick( TObject *Sender );
};
//----------------------------------------------------------------------------
#endif
