object MessagesDlg: TMessagesDlg
  Tag = 3
  Left = 325
  Top = 160
  Width = 440
  Height = 502
  Caption = 'Messages'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clPurple
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  OnHide = FormHide
  OnShow = FormShow
  PixelsPerInch = 96
  TextHeight = 13
  object RichEditMessages: TRichEdit
    Left = 0
    Top = 0
    Width = 432
    Height = 424
    Align = alTop
    Anchors = [akLeft, akTop, akRight, akBottom]
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clBlack
    Font.Height = -11
    Font.Name = 'MS Sans Serif'
    Font.Style = []
    ParentFont = False
    PopupMenu = PopupMenu1
    ReadOnly = True
    ScrollBars = ssBoth
    TabOrder = 0
    WantReturns = False
  end
  object ButtonOK: TButton
    Tag = 3
    Left = 144
    Top = 437
    Width = 145
    Height = 25
    Anchors = [akBottom]
    Caption = 'OK'
    Default = True
    ModalResult = 1
    TabOrder = 1
    OnClick = ButtonOKClick
  end
  object PopupMenu1: TPopupMenu
    Left = 304
    Top = 432
    object PopupCopyText: TMenuItem
      Caption = '&Copy text'
      ShortCut = 16451
      OnClick = PopupCopyTextClick
    end
  end
end
