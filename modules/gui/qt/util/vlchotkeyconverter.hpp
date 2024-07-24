#ifndef VLC_HOTKEY_CONVERTER_HPP
#define VLC_HOTKEY_CONVERTER_HPP

#include <QObject>

/* VLC Key/Wheel hotkeys interactions */

class QKeyEvent;
class QWheelEvent;
class QInputEvent;

int qtKeyModifiersToVLC( const QInputEvent& e );
int qtEventToVLCKey( QKeyEvent *e );
int qtWheelEventToVLCKey( const QWheelEvent& e );
QString VLCKeyToString( unsigned val, bool );

#endif // VLC_HOTKEY_CONVERTER_HPP
