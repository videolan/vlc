/*****************************************************************************
 * kwallet.hpp: KWallet keystore module
 *****************************************************************************
 * Copyright © 2015-2016 VLC authors, VideoLAN and VideoLabs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_keystore.h>

#include <kwallet.h>
#include <QCoreApplication>
#include <QObject>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>

class VLCKWallet : public QObject
{
    Q_OBJECT

public:
    VLCKWallet(QCoreApplication *, vlc_object_t *);
    bool open(bool b_force);
    void close(void);
    int store(const char * const [KEY_MAX], const uint8_t *, size_t, const char *);
    unsigned int find(const char * const [KEY_MAX], vlc_keystore_entry **);
    unsigned int remove(const char * const [KEY_MAX]);

signals:
    void opened(bool b_force);
    void closed();
    void stored(const char * const *, const uint8_t *, size_t,
                const char *, int &);
    void found(const char * const *, vlc_keystore_entry **,
               unsigned int &);
    void removed(const char * const *, unsigned int &);


private slots:
    void mainloopOpen(bool);
    void mainloopClose();
    void mainloopStore(const char * const *, const uint8_t *, size_t,
                       const char *, int &);
    void mainloopFind(const char * const *, vlc_keystore_entry **,
                      unsigned int &);
    void mainloopRemove(const char * const *, unsigned int &);

    void kwalletOpen(bool);
    void kwalletClose();

private:
    enum State {
        STATE_INIT,
        STATE_KWALLET_OPENED,
        STATE_KWALLET_CLOSED,
    };

    void signalLocked(enum State);
    void signal(enum State);
    static void interrupted(void *);
    bool waitOpened(void);

    enum State              mState;
    vlc_object_t *          mObj;
    KWallet::Wallet *       mWallet;
    QMutex                  mMutex;
    QWaitCondition          mCond;
};
