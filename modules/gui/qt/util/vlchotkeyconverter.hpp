#ifndef VLC_HOTKEY_CONVERTER_HPP
#define VLC_HOTKEY_CONVERTER_HPP

#include <QObject>
#include <QPoint>
#include <QQmlEngine>

/* VLC Key/Wheel hotkeys interactions */

Q_MOC_INCLUDE("QKeyEvent")
Q_MOC_INCLUDE("QWheelEvent")

class QKeyEvent;
class QWheelEvent;
class QInputEvent;

int qtKeyModifiersToVLC( const QInputEvent& e );
int qtEventToVLCKey( QKeyEvent *e );
int qtWheelEventToVLCKey( const QWheelEvent& e );
QString VLCKeyToString( unsigned val, bool );

/**
 * @brief The WheelToVLCConverter class aggregates wheel events and
 * emit a signal once it gathers a full scroll step, as VLC doesn't handle
 * fractionnal scroll events
 */
class WheelToVLCConverter : public QObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    using QObject::QObject;

public:
    Q_INVOKABLE Qt::Orientations getWheelOrientation(int x, int y);

signals:
    /**
     * @param vlcKey the VLC hotkey representation
     */
    void vlcWheelKey(int vlcKey);
    /**
     * @param steps: positive value indicated UP wheel events, negative DOWN wheel event
     * @param modifiers are keyboard pressed modifiers
     */
    void wheelUpDown(int steps, Qt::KeyboardModifiers modifiers);
    /**
     * @param steps: positive value indicated UP wheel events, negative DOWN wheel event
     * @param modifiers are keyboard pressed modifiers
     */
    void wheelLeftRight(int steps, Qt::KeyboardModifiers modifiers);

public slots:
    /**
     * @brief qmlWheelEvent handles wheel events as emitted by QWidget
     * @param e the wheel event
     */
    void wheelEvent(const QWheelEvent* e);

    /**
     * @brief qmlWheelEvent handles wheel events as emitted by QML
     * @param wheelEvent qml wheel event
     */
    void qmlWheelEvent(const QObject* wheelEvent);

private:
    QPoint m_scrollAmount = {};
    Qt::KeyboardModifiers m_modifiers = {};
    Qt::MouseButtons m_buttons = {};
};

#endif // VLC_HOTKEY_CONVERTER_HPP
