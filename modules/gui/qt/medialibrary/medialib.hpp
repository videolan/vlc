/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#pragma once
#include <QQuickItem>

#include <memory>

#include "qt.hpp"
#include "mlthreadpool.hpp"
#include "mlqmltypes.hpp"

namespace vlc {
namespace playlist {
class Media;
}
}

struct vlc_medialibrary_t;

class MediaLib : public QObject
{
    Q_OBJECT

public:
    Q_PROPERTY(bool discoveryPending READ discoveryPending NOTIFY discoveryPendingChanged FINAL)
    Q_PROPERTY(int  parsingProgress READ parsingProgress NOTIFY parsingProgressChanged FINAL)
    Q_PROPERTY(QString discoveryEntryPoint READ discoveryEntryPoint NOTIFY discoveryEntryPointChanged FINAL)
    Q_PROPERTY(bool idle READ idle NOTIFY idleChanged FINAL)

public:
    MediaLib(qt_intf_t* _intf, vlc::playlist::PlaylistController* playlistController, QObject* _parent = nullptr );
    ~MediaLib();

    Q_INVOKABLE void addToPlaylist(const MLItemId &itemId, const QStringList &options = {});
    Q_INVOKABLE void addToPlaylist(const QString& mrl, const QStringList &options = {});
    Q_INVOKABLE void addToPlaylist(const QUrl& mrl, const QStringList &options = {});
    Q_INVOKABLE void addToPlaylist(const QVariantList& itemIdList, const QStringList &options = {});

    Q_INVOKABLE void addAndPlay(const MLItemId &itemId, const QStringList &options = {});
    Q_INVOKABLE void addAndPlay(const QString& mrl, const QStringList &options = {});
    Q_INVOKABLE void addAndPlay(const QUrl& mrl, const QStringList &options = {});
    Q_INVOKABLE void addAndPlay(const QVariantList&itemIdList, const QStringList &options = {});
    Q_INVOKABLE void insertIntoPlaylist(size_t index, const QVariantList &itemIds /*QList<MLParentId>*/, const QStringList &options = {});

    Q_INVOKABLE void reload();

    Q_INVOKABLE void mlInputItem(const QVector<MLItemId>& itemIdVector, QJSValue callback);

    inline bool idle() const { return m_idle; }
    inline int discoveryPending() const { return m_discoveryPending; }
    inline QString discoveryEntryPoint() const { return m_discoveryEntryPoint; }
    inline int parsingProgress() const { return m_parsingProgress; }

    vlc_ml_event_callback_t* registerEventListener(void (*callback)(void*, const vlc_ml_event_t*), void* data);
    void unregisterEventListener(vlc_ml_event_callback_t*);

    /**
     * this function allows to run lambdas on the ML thread,
     * this should be used to perform operation that requires to call vlc_ml_xxx functions
     * as vlc_ml_xxx should *not* be called from the UI thread
     *
     * @param Ctx is the type of context, this context is created by the runner and passed sequentially to
     *     the ML callback then to UI callback, the ML callback may modify its properties as a way to pass
     *     data to the UI callback
     *
     * @param obj: this is the instance of the calling QObject, we ensure that this object is still live when
     *   calling the ui callback, if this object gets destroyed the task is canceled
     *
     * @param mlCb, this callback is executed in a background thread (thread pool), it takes as arguments
     *     the instance of the medialibrary, and a context, the context can be modified to pass data to the
     *     UI callback
     *
     * @param uiCB, this callback is executed on the Qt thread, it takes as first argument the
     *     id of the tasks, and as second argument the context that was created for the ML callback
     *
     * @param queue, this allows to specify if the task must be executed on a specific queue, if nullptr
     * task may be run by any thread in the threadpool. this is useful if you want to ensure that tasks must
     * be executed in order.
     *
     * @warning the ml callback **SHOULD NOT** capture the obj object or point to its properties by reference,
     * as the obj object may potentially be destroyed when the functio is called
     *
     * the ui callback **MAY** capture the object obj as we will ensure that the object still exists before calling
     * the callback
     *
     * the uiCb **MAY NOT** be called if the object obj is released earlier
     *
     * anything stored int the context Ctx **MUST** auto release when the context is destroyed
     *
     * sample usage:
     *
     * @code
     * struct Ctx {
     *    bool result;
     *    Data mydata;
     * };
     * runOnMLThread<Ctx>(this,
     * //run on ML thread
     * [](vlc_medialibrary_t* ml, Ctx& ctx){
     *    ctx.result = vlc_ml_xxx(ml, &ctx.mydata):
     * },
     * //run on UI thread
     * [this](quint64 taskId, Ctx& ctx) {
     *    if (ctx.result)
     *      emit dataUpdated(ctx.mydata);
     * })
     * @endcode
     */
    template<typename Ctx>
    quint64 runOnMLThread(const QObject* obj,
                    std::function< void(vlc_medialibrary_t* ml, Ctx& ctx)> mlCb,
                    std::function< void(quint64 taskId, Ctx& ctx)> uiCB,
                    const char* queue = nullptr);

    /**
     * same as runOnMLThread<Ctx> when no context passing is required
     */
    quint64 runOnMLThread(const QObject* obj,
                    std::function< void(vlc_medialibrary_t* ml)> mlCb,
                    std::function< void()> uiCB,
                    const char* queue = nullptr);
    /**
     * same as runOnMLThread<Ctx> when no context passing is required
     */
    quint64 runOnMLThread(const QObject* obj,
                    std::function< void(vlc_medialibrary_t* ml)> mlCb,
                    std::function< void(quint64 taskId)> uiCB,
                    const char* queue = nullptr);

    /**
     * same as runOnMLThread<Ctx> when no ui callback is required
     */
   quint64 runOnMLThread(const QObject* obj,
                    std::function< void(vlc_medialibrary_t* ml)> mlCb,
                    const char* queue = nullptr);


    /**
     * @brief cancelMLTask, explicitly cancel a running task
     *
     * @param object the object passed to runOnMLThread
     * @param taskId the id of the task to cancel
     *
     * @note there is warranty whether the ML callback will be run or not.
     * The UI callback won't be run unless the task is already terminated.
     *
     * this must be called from the Qt thread.
     */
    void cancelMLTask(const QObject* object, quint64 taskId);

signals:
    void discoveryStarted();
    void discoveryCompleted();
    void parsingProgressChanged( quint32 percent );
    void discoveryEntryPointChanged( QString entryPoint );
    void discoveryPendingChanged( bool state );
    void idleChanged();

private:
    static void onMediaLibraryEvent( void* data, const vlc_ml_event_t* event );

private:
    qt_intf_t* m_intf;
    vlc::playlist::PlaylistController* m_playlistController = nullptr;

    bool m_idle = false;
    bool m_discoveryPending = false;
    int m_parsingProgress = 0;
    QString m_discoveryEntryPoint;

    /* Medialibrary */
    vlc_medialibrary_t* m_ml;
    std::unique_ptr<vlc_ml_event_callback_t, std::function<void(vlc_ml_event_callback_t*)>> m_event_cb;
    ThreadRunner* m_runner = nullptr;

    QMap<QVector<MLItemId>, QVector<QJSValue>> m_inputItemQuery;
};

template<typename Ctx>
quint64 MediaLib::runOnMLThread(const QObject* obj,
                                    std::function<void (vlc_medialibrary_t*, Ctx&)> mlFun,
                                    std::function<void (quint64 taskId, Ctx&)> uiFun,
                                    const char* queue)
{
    return m_runner->runOnThread<Ctx>(obj, [ml = m_ml, mlFun](Ctx& ctx){mlFun(ml,ctx);}, uiFun, queue);
}
