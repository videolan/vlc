#ifndef MAINUI_HPP
#define MAINUI_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"

#include <QObject>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>

class VLCVarChoiceModel;

class MainUI : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool playlistDocked READ isPlaylistDocked WRITE setPlaylistDocked NOTIFY playlistDockedChanged)
    Q_PROPERTY(bool playlistVisible READ isPlaylistVisible WRITE setPlaylistVisible NOTIFY playlistVisibleChanged)
    Q_PROPERTY(bool showRemainingTime READ isShowRemainingTime WRITE setShowRemainingTime NOTIFY showRemainingTimeChanged)

    Q_PROPERTY(VLCVarChoiceModel* extraInterfaces READ getExtraInterfaces CONSTANT)

public:
    explicit MainUI(intf_thread_t *_p_intf, MainInterface* mainInterface, QObject *parent = nullptr);
    ~MainUI();

    bool setup(QQmlEngine* engine);

    inline QQmlComponent* getComponent() const {return m_component;}
    VLC_USED
    QQuickItem* createRootItem();

public slots:
    inline bool isPlaylistDocked() const { return m_playlistDocked; }
    inline bool isPlaylistVisible() const { return m_playlistVisible; }
    inline bool isShowRemainingTime() const  { return m_showRemainingTime; }
    inline VLCVarChoiceModel* getExtraInterfaces() const { return m_extraInterfaces; };

    void setPlaylistDocked( bool );
    void setPlaylistVisible( bool );
    void setShowRemainingTime( bool );


signals:
    void playlistDockedChanged(bool);
    void playlistVisibleChanged(bool);
    void showRemainingTimeChanged(bool);

private slots:
    void onQmlWarning(const QList<QQmlError>& errors);

private:
    void registerQMLTypes();

    intf_thread_t* m_intf = nullptr;
    MainInterface* m_mainInterface = nullptr;

    QQmlEngine* m_engine = nullptr;
    QQmlComponent* m_component = nullptr;
    QQuickItem* m_rootItem = nullptr;

    QSettings* m_settings = nullptr;

    bool m_hasMedialibrary;

    bool m_playlistDocked = false;
    bool m_playlistVisible = false;
    bool m_showRemainingTime = false;
    VLCVarChoiceModel* m_extraInterfaces = nullptr;

};

#endif // MAINUI_HPP
