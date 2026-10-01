!define nsProcess::FindProcess `!insertmacro nsProcess::FindProcess`

!macro nsProcess::FindProcess _FILE _ERR
	nsProcess::_FindProcess /NOUNLOAD `${_FILE}`
	Pop ${_ERR}
!macroend


!define nsProcess::CloseProcess `!insertmacro nsProcess::CloseProcess`

!macro nsProcess::CloseProcess _FILE _ERR
	nsProcess::_CloseProcess /NOUNLOAD `${_FILE}`
	Pop ${_ERR}
!macroend
