VERSION 5.00
Object = "{F9043C88-F6F2-101A-A3C9-08002B2F49FB}#1.1#0"; "COMDLG32.OCX"
Begin VB.Form ppal 
   AutoRedraw      =   -1  'True
   BackColor       =   &H00FFFFFF&
   Caption         =   "VLC skin Curve Maker"
   ClientHeight    =   7140
   ClientLeft      =   165
   ClientTop       =   450
   ClientWidth     =   10440
   Icon            =   "Bezier.frx":0000
   LinkTopic       =   "Form1"
   ScaleHeight     =   476
   ScaleMode       =   3  'Pixel
   ScaleWidth      =   696
   StartUpPosition =   2  'CenterScreen
   Begin VB.PictureBox Pict 
      AutoSize        =   -1  'True
      BorderStyle     =   0  'None
      Height          =   975
      Left            =   2640
      ScaleHeight     =   65
      ScaleMode       =   3  'Pixel
      ScaleWidth      =   73
      TabIndex        =   4
      Top             =   1800
      Visible         =   0   'False
      Width           =   1095
   End
   Begin VB.PictureBox toolbox 
      Align           =   1  'Align Top
      BorderStyle     =   0  'None
      Height          =   900
      Left            =   0
      ScaleHeight     =   900
      ScaleWidth      =   10440
      TabIndex        =   0
      Top             =   0
      Width           =   10440
      Begin VB.HScrollBar Size 
         Height          =   255
         Left            =   4920
         Max             =   5
         Min             =   1
         TabIndex        =   3
         Top             =   480
         Value           =   1
         Width           =   2655
      End
      Begin VB.HScrollBar Color 
         Height          =   255
         Left            =   4920
         Max             =   15
         TabIndex        =   2
         Top             =   120
         Width           =   2655
      End
      Begin VB.TextBox Result 
         Height          =   615
         Left            =   120
         Locked          =   -1  'True
         MultiLine       =   -1  'True
         TabIndex        =   1
         Top             =   120
         Width           =   4575
      End
   End
   Begin MSComDlg.CommonDialog Cmd 
      Left            =   7560
      Top             =   120
      _ExtentX        =   847
      _ExtentY        =   847
      _Version        =   327680
   End
   Begin VB.Menu m_file 
      Caption         =   "&File"
      Begin VB.Menu m_load 
         Caption         =   "Load..."
      End
      Begin VB.Menu m_saveas 
         Caption         =   "Save as..."
      End
      Begin VB.Menu m_sep1 
         Caption         =   "-"
      End
      Begin VB.Menu m_quit 
         Caption         =   "Quit"
      End
   End
   Begin VB.Menu m_picture 
      Caption         =   "Picture"
      Begin VB.Menu m_loadpicture 
         Caption         =   "Load..."
      End
   End
   Begin VB.Menu m_tool 
      Caption         =   "Tool"
      Visible         =   0   'False
      Begin VB.Menu m_addpoint 
         Caption         =   "AddPoint"
      End
      Begin VB.Menu m_center 
         Caption         =   "Center"
      End
   End
   Begin VB.Menu m_point 
      Caption         =   "Point"
      Visible         =   0   'False
      Begin VB.Menu m_deletept 
         Caption         =   "Delete"
      End
   End
End
Attribute VB_Name = "ppal"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Dim xe As Single
Dim ye As Single
Dim Sel As Long

Dim MouseX As Long
Dim MouseY As Long
Dim SelectPt As Long
Dim PictureFile As String
Dim CurveFile As String

Dim OffsetX As Long
Dim OffsetY As Long
Sub form_draw()
Dim i As Long
Me.Cls


BitBlt ppal.hdc, OffsetX, OffsetY, Pict.Width, Pict.Height, imgDC, 0, 0, SRCCOPY


If MaxPt < 0 Then Exit Sub
Call bezier_draw(40, OffsetX, OffsetY)

Me.DrawWidth = 1
For i = 0 To MaxPt
    Me.Line (OffsetX + Pt(i).x - 6, OffsetY + Pt(i).y - 6)-(OffsetX + Pt(i).x + 6, OffsetY + Pt(i).y + 6), QBColor(Color.Value), B
Next i
Me.DrawWidth = Size.Value

End Sub








Sub RefreshResult()
Dim i As Long

Result.Text = "abs="""
For i = 0 To MaxPt
    If i > 0 Then Result.Text = Result.Text & ","
    Result.Text = Result.Text & Pt(i).x
Next i
Result.Text = Result.Text & """" & Chr$(13) & Chr$(10) & "ord="""
For i = 0 To MaxPt
    If i > 0 Then Result.Text = Result.Text & ","
    Result.Text = Result.Text & Pt(i).y
Next i
Result.Text = Result.Text & """"

End Sub

Private Sub Color_Change()
form_draw

End Sub

Private Sub Form_Load()
PictureFile = "none"
MaxPt = -1
OffsetX = 0
OffsetY = 0
'Pict.Width = 0
'Pict.Height = 0
Call m_center_Click

End Sub

Private Sub Form_MouseDown(Button As Integer, Shift As Integer, x As Single, y As Single)
Dim i As Long

If Button = 2 Then
    For i = 0 To MaxPt
        If Pt(i).x + OffsetX > x - 5 And Pt(i).x + OffsetX < x + 5 Then
            If Pt(i).y + OffsetY > y - 5 And Pt(i).y + OffsetY < y + 5 Then
                SelectPt = i + 1
                Me.PopupMenu m_point
                Exit Sub
            End If
        End If
    Next i
    MouseX = x
    MouseY = y
    Me.PopupMenu m_tool
ElseIf Button = 1 Then
    For i = 0 To MaxPt
        If Pt(i).x + OffsetX > x - 5 And Pt(i).x + OffsetX < x + 5 Then
            If Pt(i).y + OffsetY > y - 5 And Pt(i).y + OffsetY < y + 5 Then
                SelectPt = i + 1
                Exit Sub
            End If
        End If
    Next i
    SelectPt = 0
    Me.MousePointer = 5
    MouseX = x
    MouseY = y
End If

End Sub


Private Sub Form_MouseMove(Button As Integer, Shift As Integer, x As Single, y As Single)

If Button = 1 Then
    If SelectPt > 0 Then
        Pt(SelectPt - 1).x = x - OffsetX
        Pt(SelectPt - 1).y = y - OffsetY
        form_draw
    Else
        OffsetX = OffsetX - (x - MouseX)
        OffsetY = OffsetY - (y - MouseY)
        MouseX = x
        MouseY = y
        form_draw
    End If
ElseIf Button = 0 Then
    For i = 0 To MaxPt
        If Pt(i).x + OffsetX > x - 5 And Pt(i).x + OffsetX < x + 5 Then
            If Pt(i).y + OffsetY > y - 5 And Pt(i).y + OffsetY < y + 5 Then
                SelectPt = i + 1
                Me.MousePointer = 10
                Exit Sub
            End If
        End If
    Next i
    Me.MousePointer = 0
End If

End Sub


Private Sub Form_MouseUp(Button As Integer, Shift As Integer, x As Single, y As Single)
If Button = 1 Then
    If SelectPt > 0 Then
        SelectPt = 0
        form_draw
        Call RefreshResult
    End If
    Me.MousePointer = 0
End If

End Sub

Private Sub m_addpoint_Click()
MaxPt = MaxPt + 1
Call make_pt(MaxPt, MouseX - OffsetX, MouseY - OffsetY)
Call form_draw

Call RefreshResult

End Sub

Private Sub m_center_Click()
OffsetX = (Me.ScaleWidth - Pict.Width) / 2
OffsetY = (Me.ScaleHeight - Pict.Height - toolbox.Height) / 2
form_draw

End Sub

Private Sub m_deletept_Click()
Dim i As Long
MaxPt = MaxPt - 1
For i = SelectPt - 1 To MaxPt
    Pt(i).x = Pt(i + 1).x
    Pt(i).y = Pt(i + 1).y
Next
form_draw
Call RefreshResult

End Sub

Private Sub m_load_Click()
Dim F As FileSystemObject
Set F = New FileSystemObject
Cmd.filename = CurveFile
Cmd.CancelError = False
Cmd.DialogTitle = "Open Curve"
Cmd.Filter = "Fichier VLC curve |*.curve.vlc"
Cmd.FilterIndex = 0
Cmd.InitDir = App.Path
Cmd.ShowOpen

If Not F.FileExists(Cmd.filename) Then Exit Sub
CurveFile = Cmd.filename
Dim i As Long, l As Long
Open CurveFile For Binary As #1
    Get #1, , l
    PictureFile = Space$(l)
    Get #1, , PictureFile
    Get #1, , OffsetX
    Get #1, , OffsetY
    Get #1, , MaxPt
    For i = 0 To MaxPt
        Get #1, , Pt(i).x
        Get #1, , Pt(i).y
    Next i
Close #1
If PictureFile <> "none" Then Pict.Picture = LoadPicture(PictureFile)
Call form_draw
Call RefreshResult

End Sub

Private Sub m_loadpicture_Click()
Dim F As FileSystemObject
Set F = New FileSystemObject
Cmd.CancelError = False
Cmd.DialogTitle = "Open picture"
Cmd.Filter = "Fichier bitmap |*.bmp"
Cmd.FilterIndex = 0
Cmd.InitDir = App.Path
Cmd.ShowOpen

If Not F.FileExists(Cmd.filename) Then Exit Sub
PictureFile = Cmd.filename
Pict.Picture = LoadPicture(Cmd.filename)

Dim HBitmap As Long
HBitmap = LoadImage(0, Cmd.filename, 0, 0, 0, 16)
imgDC = CreateCompatibleDC(0)
SelectObject imgDC, HBitmap
Pict.AutoSize = True

Call m_center_Click

End Sub


Private Sub m_quit_Click()
End

End Sub

Private Sub m_saveas_Click()
Dim F As FileSystemObject
Set F = New FileSystemObject
On Error GoTo error
Cmd.CancelError = True
Cmd.DialogTitle = "Save Curve"
Cmd.Filter = "Fichier VLC curve |*.curve.vlc"
Cmd.FilterIndex = 0
Cmd.InitDir = App.Path
Cmd.ShowSave

CurveFile = Cmd.filename

Dim i As Long
Open CurveFile For Binary As #1
    Put #1, , CLng(Len(PictureFile))
    Put #1, , PictureFile
    Put #1, , OffsetX
    Put #1, , OffsetY
    Put #1, , MaxPt
    For i = 0 To MaxPt
        Put #1, , Pt(i).x
        Put #1, , Pt(i).y
    Next i
Close #1

error:

End Sub


Private Sub Size_Change()
Me.DrawWidth = Size.Value
form_draw

End Sub
