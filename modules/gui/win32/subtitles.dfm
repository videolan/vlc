object SubtitlesDlg: TSubtitlesDlg
  Left = 397
  Top = 333
  Width = 432
  Height = 134
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
    Left = 8
    Top = 8
    Width = 310
    Height = 91
    Anchors = [akLeft, akTop, akRight, akBottom]
    Caption = 'Select a subtitles file'
    TabOrder = 0
    object LabelDelay: TLabel
      Left = 48
      Top = 64
      Width = 30
      Height = 13
      Caption = 'Delay:'
    end
    object LabelFPS: TLabel
      Left = 173
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
      TabOrder = 0
      Text = '0.0'
    end
    object EditFPS: TEdit
      Left = 205
      Top = 60
      Width = 57
      Height = 21
      Hint = 'Set the number of Frames Per Second'
      Anchors = [akTop, akRight]
      ParentShowHint = False
      ShowHint = True
      TabOrder = 1
      Text = '0.0'
    end
    object EditFile: TEdit
      Left = 16
      Top = 24
      Width = 190
      Height = 21
      Anchors = [akLeft, akTop, akRight]
      TabOrder = 2
    end
    object ButtonBrowse: TButton
      Left = 219
      Top = 22
      Width = 75
      Height = 25
      Anchors = [akTop, akRight]
      Caption = 'Browse...'
      TabOrder = 3
      OnClick = ButtonBrowseClick
    end
  end
  object BitBtnOK: TBitBtn
    Left = 333
    Top = 22
    Width = 81
    Height = 25
    Anchors = [akRight]
    TabOrder = 1
    OnClick = BitBtnOKClick
    Kind = bkOK
  end
  object BitBtnCancel: TBitBtn
    Left = 333
    Top = 60
    Width = 81
    Height = 25
    Anchors = [akRight]
    TabOrder = 2
    Kind = bkCancel
  end
  object OpenDialog1: TOpenDialog
    Filter = 
      'Subtitles Files (*.txt, *.sub, *.srt, *.ssa)|*.txt;*.sub;*.srt;*' +
      '.ssa|All Files (*.*)|*.*'
    Left = 16
    Top = 64
  end
end
