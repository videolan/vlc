object SubtitlesDlg: TSubtitlesDlg
  Tag = 3
  Left = 520
  Top = 185
  Width = 338
  Height = 173
  Caption = 'Add subtitles'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  PixelsPerInch = 96
  TextHeight = 13
  object GroupBoxSubtitles: TGroupBox
    Tag = 3
    Left = 8
    Top = 8
    Width = 314
    Height = 96
    Anchors = [akLeft, akTop, akRight]
    Caption = 'Select a subtitles file'
    TabOrder = 0
    object LabelDelay: TLabel
      Tag = 3
      Left = 48
      Top = 64
      Width = 30
      Height = 13
      Caption = 'Delay:'
    end
    object LabelFPS: TLabel
      Tag = 3
      Left = 177
      Top = 64
      Width = 23
      Height = 13
      Anchors = [akTop, akRight]
      Caption = 'FPS:'
    end
    object EditDelay: TEdit
      Left = 88
      Top = 60
      Width = 57
      Height = 21
      Hint = 'Set the delay (in seconds)'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 2
      Text = '0.0'
    end
    object EditFPS: TEdit
      Left = 209
      Top = 60
      Width = 57
      Height = 21
      Hint = 'Set the number of Frames Per Second'
      Anchors = [akTop, akRight]
      ParentShowHint = False
      ShowHint = True
      TabOrder = 3
      Text = '0.0'
    end
    object EditFile: TEdit
      Left = 16
      Top = 24
      Width = 194
      Height = 21
      Anchors = [akLeft, akTop, akRight]
      TabOrder = 0
    end
    object ButtonBrowse: TButton
      Tag = 3
      Left = 223
      Top = 22
      Width = 75
      Height = 25
      Anchors = [akTop, akRight]
      Caption = 'Browse...'
      TabOrder = 1
      OnClick = ButtonBrowseClick
    end
  end
  object ButtonOK: TButton
    Tag = 3
    Left = 39
    Top = 112
    Width = 98
    Height = 25
    Caption = 'OK'
    Default = True
    ModalResult = 1
    TabOrder = 1
    OnClick = ButtonOKClick
  end
  object ButtonCancel: TButton
    Tag = 3
    Left = 192
    Top = 112
    Width = 98
    Height = 25
    Cancel = True
    Caption = 'Cancel'
    ModalResult = 2
    TabOrder = 2
    OnClick = ButtonOKClick
  end
  object OpenDialog1: TOpenDialog
    Filter = 
      'Subtitles Files (*.txt, *.sub, *.srt, *.ssa)|*.txt;*.sub;*.srt;*' +
      '.ssa|All Files (*.*)|*.*'
    Left = 16
    Top = 64
  end
end
