Attribute VB_Name = "Bezier"
Declare Function CreateCompatibleDC Lib "gdi32" (ByVal hdc As Long) As Long
Declare Function LoadImage Lib "user32" Alias "LoadImageA" (ByVal hInst As Long, ByVal lpsz As String, ByVal un1 As Long, ByVal n1 As Long, ByVal n2 As Long, ByVal un2 As Long) As Long
Declare Function SelectObject Lib "gdi32" (ByVal hdc As Long, ByVal hObject As Long) As Long

Declare Function BitBlt Lib "gdi32" (ByVal hDestDC As Long, ByVal x As Long, ByVal y As Long, ByVal nWidth As Long, ByVal nHeight As Long, ByVal hSrcDC As Long, ByVal xSrc As Long, ByVal ySrc As Long, ByVal dwRop As Long) As Long
Public Const SRCCOPY = &HCC0020

Global imgDC As Long

Type pts
    x As Single
    y As Single
End Type
Global ft(30) As Single
Global Pt(30) As pts
Global MaxPt As Long

Sub bezier_draw(nb As Long, OffX As Long, OffY As Long)
Dim i As Long, pas As Single, t As Single, oldx As Single, oldy As Single, x As Single, y As Single

pas = 1 / nb
Call ini_factorielles
oldx = Pt(0).x
oldy = Pt(0).y

For t = pas To 1 Step pas
    x = bezier_ptx(t)
    y = bezier_pty(t)
    ppal.Line (OffX + oldx, OffY + oldy)-(OffX + x, OffY + y), QBColor(ppal.Color.Value)
    oldx = x
    oldy = y
Next t
For i = 0 To MaxPt
    ppal.PSet (OffX + Pt(i).x, OffY + Pt(i).y), QBColor(ppal.Color.Value)
Next i

End Sub

Function bezier_pty(t As Single) As Single
Dim k As Long, i As Long
k = 0
For i = 0 To MaxPt
    bezier_pty = bezier_pty + Pt(i).y * melange(k, MaxPt, t)
    k = k + 1
Next i

End Function
Function bezier_ptx(t As Single) As Single
Dim k As Long, i As Long
k = 0
For i = 0 To MaxPt
    bezier_ptx = bezier_ptx + Pt(i).x * melange(k, MaxPt, t)
    k = k + 1
Next i

End Function

Sub ini_factorielles()
ft(0) = 1
For i& = 1 To 30
    ft(i&) = ft(i& - 1) * i&
Next i&

End Sub

Sub make_pt(i As Long, x As Long, y As Long)
Pt(i).x = x
Pt(i).y = y

End Sub

Function melange(i As Long, n As Long, t As Single) As Single
melange = CSng(ft(n) / ft(i) / ft(n - i)) * t ^ i * (1 - t) ^ (n - i)

End Function

