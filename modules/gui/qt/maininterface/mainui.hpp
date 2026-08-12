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
#include <QSortFilterProxyModel>
#include <QWindow>
#include <QScreen>
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
#include <QAbstractItemModel>
#endif
#include <QAbstractProxyModel>
#include <QMessageBox>

class VLCVarChoiceModel;

class MainUI : public QObject
{
    Q_OBJECT

public:
    explicit MainUI(qt_intf_t *_p_intf, MainCtx* mainCtx, QWindow* interfaceWindow, QObject *parent = nullptr);
    ~MainUI();

    [[nodiscard]] bool setup(QQmlEngine* engine);

    QPointer<QQmlComponent> getComponent() const {return m_component;}
    VLC_USED
    QQuickItem* createRootItem();

private:
    void registerQMLTypes();
    static void clearQMLTypes();
    static QObject* getMainCtxInstance(QQmlEngine *, QJSEngine *);

    qt_intf_t* m_intf = nullptr;
    MainCtx* m_mainCtx = nullptr;
    QWindow*       m_interfaceWindow = nullptr;

    QPointer<QQmlComponent> m_component;
    QQuickItem* m_rootItem = nullptr;

    QPointer<QQmlEngine> m_engineBound;
};

namespace vlc {

struct QSortFilterProxyModelForeign
{
    Q_GADGET
    QML_FOREIGN(QSortFilterProxyModel)
    QML_NAMED_ELEMENT(QtSortFilterProxyModel)
};

// Qt Declarative already has this, but it is registered anonymously:
struct QWindowForeign
{
    Q_GADGET
    QML_FOREIGN(QWindow)
    QML_NAMED_ELEMENT(QtWindow)
    QML_UNCREATABLE("")
};

// Qt Declarative already has this, but it is registered anonymously:
struct QScreenForeign
{
    Q_GADGET
    QML_FOREIGN(QScreen)
    QML_NAMED_ELEMENT(QtScreen)
    QML_UNCREATABLE("")
};

#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
struct QAbstractItemModelForeign
{
    Q_GADGET
    QML_FOREIGN(QAbstractItemModel)
    QML_NAMED_ELEMENT(AbstractItemModel) // Qt Qml registers it without "Qt" prefix
    QML_UNCREATABLE("")
};
#endif

struct QAbstractProxyModelForeign
{
    Q_GADGET
    QML_FOREIGN(QAbstractProxyModel)
    QML_NAMED_ELEMENT(AbstractProxyModel) // "Qt" prefix is not used to align with `AbstractItemModel`.
    QML_UNCREATABLE("")
};

struct QMessageBoxForeign
{
    Q_GADGET
    QML_FOREIGN(QMessageBox)
    QML_NAMED_ELEMENT(QtMessageBox)
    QML_UNCREATABLE("QtMessageBox is uncreatable, use `DialogsProvider::messageDialog()` instead.")
};

}

#endif // MAINUI_HPP
