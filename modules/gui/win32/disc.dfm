object DiscDlg: TDiscDlg
  Left = 390
  Top = 320
  BorderIcons = [biSystemMenu, biMinimize]
  BorderStyle = bsDialog
  Caption = 'Open Disc'
  ClientHeight = 138
  ClientWidth = 338
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  OnHide = FormHide
  OnShow = FormShow
  PixelsPerInch = 96
  TextHeight = 13
  object LabelDevice: TLabel
    Tag = 3
    Left = 8
    Top = 113
    Width = 66
    Height = 13
    Caption = 'Device &name:'
  end
  object Bevel1: TBevel
    Left = 240
    Top = 0
    Width = 9
    Height = 137
    Shape = bsLeftLine
  end
  object GroupBoxPosition: TGroupBox
    Tag = 3
    Left = 96
    Top = 8
    Width = 137
    Height = 97
    Caption = 'Starting position'
    TabOrder = 1
    object LabelTitle: TLabel
      Tag = 3
      Left = 8
      Top = 44
      Width = 23
      Height = 13
      Caption = '&Title:'
    end
    object LabelChapter: TLabel
      Tag = 3
      Left = 8
      Top = 72
      Width = 40
      Height = 13
      Caption = '&Chapter:'
    end
    object SpinEditTitle: TCSpinEdit
      Left = 56
      Top = 39
      Width = 73
      Height = 22
      TabStop = True
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'MS Sans Serif'
      Font.Style = []
      MaxValue = 65535
      MinValue = 1
      ParentColor = False
      ParentFont = False
      TabOrder = 1
      Value = 1
    end
    object SpinEditChapter: TCSpinEdit
      Left = 56
      Top = 67
      Width = 73
      Height = 22
      TabStop = True
      MaxValue = 65535
      MinValue = 1
      ParentColor = False
      TabOrder = 2
      Value = 1
    end
    object CheckBoxMenus: TCheckBox
      Left = 8
      Top = 16
      Width = 97
      Height = 17
      Caption = '&Menus'
      TabOrder = 0
      OnClick = CheckBoxMenusClick
    end
  end
  object RadioGroupType: TRadioGroup
    Tag = 3
    Left = 8
    Top = 8
    Width = 81
    Height = 97
    Caption = 'Disc type'
    ItemIndex = 0
    Items.Strings = (
      '&DVD'
      '&VCD')
    TabOrder = 0
    OnClick = RadioGroupTypeClick
  end
  object ButtonOk: TButton
    Tag = 3
    Left = 248
    Top = 8
    Width = 81
    Height = 25
    Caption = 'OK'
    Default = True
    ModalResult = 1
    TabOrder = 3
    OnClick = ButtonOkClick
  end
  object ButtonCancel: TButton
    Tag = 3
    Left = 248
    Top = 40
    Width = 81
    Height = 25
    Cancel = True
    Caption = 'Cancel'
    ModalResult = 2
    TabOrder = 4
    OnClick = ButtonCancelClick
  end
  object EditDevice: TEdit
    Tag = 5
    Left = 96
    Top = 109
    Width = 137
    Height = 21
    TabOrder = 2
    Text = 'F:\'
  end
end
