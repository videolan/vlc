/*****************************************************************************
 * mainframe.h: Prototype for main window
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
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


#ifndef mainframeH
#define mainframeH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ComCtrls.hpp>
#include <Dialogs.hpp>
#include <ImgList.hpp>
#include <Menus.hpp>
#include <ToolWin.hpp>
#include <AppEvnts.hpp>
#include <ExtCtrls.hpp>
#include "CSPIN.h"
#include <ActnList.hpp>

#include <oleidl.h>                                   /* for drag and drop */

/*****************************************************************************
 * This message is sent to the controls registered as drop targets
 *****************************************************************************/
#define WM_OLEDROP WM_USER + 1

//---------------------------------------------------------------------------
class TMainFrameDlg : public TForm
{
__published:	// IDE-managed Components
    TToolBar *ToolBar;
    TToolButton *ToolButtonFile;
    TToolButton *ToolButtonDisc;
    TToolButton *ToolButtonNet;
    TToolButton *ToolButtonSep1;
    TToolButton *ToolButtonStop;
    TToolButton *ToolButtonPlay;
    TToolButton *ToolButtonPause;
    TToolButton *ToolButtonSep2;
    TToolButton *ToolButtonSlow;
    TToolButton *ToolButtonFast;
    TToolButton *ToolButtonSep3;
    TToolButton *ToolButtonPlaylist;
    TToolButton *ToolButtonPrev;
    TToolButton *ToolButtonNext;
    TMainMenu *MainMenu1;
    TMenuItem *MenuFile;
    TMenuItem *MenuOpenFile;
    TMenuItem *MenuOpenDisc;
    TMenuItem *MenuNetworkStream;
    TMenuItem *MenuExit;
    TMenuItem *MenuView;
    TMenuItem *MenuHideinterface;
    TMenuItem *MenuTitle;
    TMenuItem *MenuChapter;
    TMenuItem *MenuAngle;
    TMenuItem *N3;
    TMenuItem *MenuPlaylist;
    TMenuItem *MenuMessages;
    TMenuItem *MenuControls;
    TMenuItem *MenuLanguage;
    TMenuItem *MenuSubtitles;
    TMenuItem *MenuPreferences;
    TMenuItem *MenuHelp;
    TMenuItem *MenuAbout;
    TOpenDialog *OpenDialog1;
    TImageList *ImageListToolbar;
    TPopupMenu *PopupMenuMain;
    TMenuItem *PopupPlay;
    TMenuItem *PopupPause;
    TMenuItem *PopupStop;
    TMenuItem *PopupBack;
    TMenuItem *PopupSlow;
    TMenuItem *PopupFast;
    TMenuItem *PopupToggleInterface;
    TMenuItem *PopupNext;
    TMenuItem *PopupPrev;
    TMenuItem *PopupJump;
    TMenuItem *PopupNavigation;
    TMenuItem *PopupProgram;
    TMenuItem *PopupLanguage;
    TMenuItem *PopupSubtitles;
    TMenuItem *PopupFile;
    TMenuItem *PopupPlaylist;
    TMenuItem *PopupPreferences;
    TMenuItem *N7;
    TMenuItem *PopupExit;
    TStatusBar *StatusBar;
    TGroupBox *GroupBoxFile;
    TLabel *LabelFileName;
    TGroupBox *GroupBoxNetwork;
    TLabel *LabelChannel;
    TLabel *LabelServer;
    TGroupBox *GroupBoxDisc;
    TMenuItem *N8;
    TMenuItem *MenuProgram;
    TLabel *LabelDisc;
    TLabel *LabelTitle;
    TButton *ButtonTitlePrev;
    TButton *ButtonTitleNext;
    TButton *ButtonChapterPrev;
    TButton *ButtonChapterNext;
    TLabel *LabelChapter;
    TLabel *LabelTitleCurrent;
    TLabel *LabelChapterCurrent;
    TButton *ButtonGo;
    TGroupBox *GroupBoxSlider;
    TTrackBar *TrackBar;
    TTimer *TimerManage;
    TMenuItem *PopupOpenFile;
    TMenuItem *PopupOpenDisc;
    TMenuItem *PopupNetworkStream;
    TMenuItem *PopupClose;
    TMenuItem *N9;
    TCSpinEdit *SpinEditChannel;
    TActionList *ActionList1;
    TAction *OpenFileAction;
    TAction *OpenDiscAction;
    TAction *NetworkStreamAction;
    TAction *PlayAction;
    TAction *PauseAction;
    TAction *StopAction;
    TAction *EjectAction;
    TAction *SlowAction;
    TAction *FastAction;
    TAction *PreviousAction;
    TAction *NextAction;
    TAction *BackAction;
    TAction *PlaylistAction;
    TAction *ExitAction;
    TAction *FullscreenAction;
    TAction *PreferencesAction;
    TAction *MessagesAction;
    TAction *AboutAction;
    TAction *PrevTitleAction;
    TAction *NextTitleAction;
    TAction *PrevChapterAction;
    TAction *NextChapterAction;
    TAction *VolumeUpAction;
    TAction *VolumeDownAction;
    TAction *MuteAction;
    TAction *StreamOutputAction;
    TAction *AddSubtitlesAction;
    TMenuItem *Control1;
    TMenuItem *N5;
    TMenuItem *N10;
    TMenuItem *N11;
    TMenuItem *MenuChannel;
    TMenuItem *PopupChannel;
    TMenuItem *MenuVolumeUp;
    TMenuItem *MenuVolumeDown;
    TMenuItem *MenuMute;
    TMenuItem *MenuADevice;
    TMenuItem *PopupAudioSettings;
    TMenuItem *PopupADevice;
    TMenuItem *VolumeDown1;
    TMenuItem *VolumeUp1;
    TMenuItem *Mute1;
    TMenuItem *MenuVDevice;
    TMenuItem *N1;
    TMenuItem *PopupVDevice;
    TMenuItem *MenuStreamOuput;
    TMenuItem *MenuAddSubtitles;
    TMenuItem *N4;
    TMenuItem *MenuAddInterface;
    TAction *OnTopAction;
    TMenuItem *MenuFullscreen;
    TMenuItem *MenuOnTop;
    TMenuItem *PopupFullscreen;
    TMenuItem *Videosettings1;
    TMenuItem *PopupOnTop;
    TMenuItem *N2;
    TMenuItem *MenuEjectDisc;
    void __fastcall TimerManageTimer( TObject *Sender );
    void __fastcall TrackBarChange( TObject *Sender );
    void __fastcall FormClose( TObject *Sender, TCloseAction &Action );
    void __fastcall MenuHideinterfaceClick( TObject *Sender );
    void __fastcall PopupCloseClick( TObject *Sender );
    void __fastcall PopupToggleInterfaceClick( TObject *Sender );
    void __fastcall PopupJumpClick( TObject *Sender );
    void __fastcall ButtonGoClick( TObject *Sender );
    void __fastcall OpenFileActionExecute( TObject *Sender );
    void __fastcall OpenDiscActionExecute( TObject *Sender );
    void __fastcall NetworkStreamActionExecute( TObject *Sender );
    void __fastcall StreamOutputActionExecute( TObject *Sender );
    void __fastcall ExitActionExecute( TObject *Sender );
    void __fastcall PlaylistActionExecute( TObject *Sender );
    void __fastcall MessagesActionExecute( TObject *Sender );
    void __fastcall AboutActionExecute( TObject *Sender );
    void __fastcall BackActionExecute( TObject *Sender );
    void __fastcall PlayActionExecute( TObject *Sender );
    void __fastcall PauseActionExecute( TObject *Sender );
    void __fastcall StopActionExecute( TObject *Sender );
    void __fastcall SlowActionExecute( TObject *Sender );
    void __fastcall FastActionExecute( TObject *Sender );
    void __fastcall PreviousActionExecute( TObject *Sender );
    void __fastcall NextActionExecute( TObject *Sender );
    void __fastcall EjectActionExecute( TObject *Sender );
    void __fastcall VolumeUpActionExecute( TObject *Sender );
    void __fastcall VolumeDownActionExecute( TObject *Sender );
    void __fastcall MuteActionExecute( TObject *Sender );
    void __fastcall FullscreenActionExecute( TObject *Sender );
    void __fastcall PreferencesActionExecute( TObject *Sender );
    void __fastcall PrevTitleActionExecute( TObject *Sender );
    void __fastcall NextTitleActionExecute( TObject *Sender );
    void __fastcall PrevChapterActionExecute( TObject *Sender );
    void __fastcall NextChapterActionExecute( TObject *Sender );
    void __fastcall AddSubtitlesActionsExecute( TObject *Sender );
    void __fastcall OnTopActionExecute( TObject *Sender );
private:	// User declarations
    intf_thread_t *p_intf;
    /* drag and drop handling */
    LPDROPTARGET lpDropTarget;
    BEGIN_MESSAGE_MAP
        MESSAGE_HANDLER( WM_OLEDROP, TMessage, OnDrop )
    END_MESSAGE_MAP( TForm )
public:		// User declarations
    TStringList *StringListPref;                   /* stores config dialogs */
    __fastcall TMainFrameDlg( TComponent* Owner, intf_thread_t *_p_intf );
    virtual __fastcall ~TMainFrameDlg();
    void __fastcall DisplayHint( TObject *Sender );
    void __fastcall ModeManage();
    void __fastcall CreatePreferences( AnsiString Name );
    void __fastcall MenuProgramClick( TObject *Sender );
    void __fastcall MenuAudioClick( TObject *Sender );
    void __fastcall MenuSubtitleClick( TObject *Sender );
    void __fastcall MenuTitleClick( TObject *Sender );
    void __fastcall MenuChapterClick( TObject *Sender );
    void __fastcall PopupProgramClick( TObject *Sender );
    void __fastcall PopupAudioClick( TObject *Sender );
    void __fastcall PopupSubtitleClick( TObject *Sender );
    void __fastcall PopupNavigationClick( TObject *Sender );
    void __fastcall OnDrop( TMessage &Msg );
};
//---------------------------------------------------------------------------
#endif
