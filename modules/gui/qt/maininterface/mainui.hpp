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

public:
    explicit MainUI(intf_thread_t *_p_intf, MainInterface* mainInterface, QWindow* interfaceWindow, QObject *parent = nullptr);
    ~MainUI();

    bool setup(QQmlEngine* engine);

    inline QQmlComponent* getComponent() const {return m_component;}
    VLC_USED
    QQuickItem* createRootItem();

private slots:
    void onQmlWarning(const QList<QQmlError>& errors);

private:
    void registerQMLTypes();

    intf_thread_t* m_intf = nullptr;
    MainInterface* m_mainInterface = nullptr;
    QWindow*       m_interfaceWindow = nullptr;

    QQmlComponent* m_component = nullptr;
    QQuickItem* m_rootItem = nullptr;
};

#endif // MAINUI_HPP
