/*****************************************************************************
 * kwallet.cpp: KWallet keystore module
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

#include "kwallet.hpp"

#include <vlc_plugin.h>
#include <vlc_interrupt.h>
#include <vlc_strings.h>

#include <QMap>
#include <QUrl>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/qdbusservicewatcher.h>

#define MAP_KEY_SECRET QString("secret")

#define qfu(i) QString::fromUtf8(i)
#define qtu(i) ((i).toUtf8().constData())

static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_shortname(N_("KWallet keystore"))
    set_description(N_("secrets are stored via KWallet"))
    set_category(CAT_ADVANCED)
    set_subcategory(SUBCAT_ADVANCED_MISC)
    set_capability("keystore", 100)
    set_callbacks(Open, Close)
    cannot_unload_broken_library()
vlc_module_end ()

/* List of kwallet services names */
static const QStringList kwalletServices = QStringList()
    << QString::fromLatin1("org.kde.kwalletd5")
    << QString::fromLatin1("org.kde.kwalletd");

static const char *const ppsz_keys[] = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "realm",
    "authtype",
};
static_assert(sizeof(ppsz_keys)/sizeof(*ppsz_keys) == KEY_MAX, "key mismatch");

static int
str2key(const char *psz_key)
{
    for (unsigned int i = 0; i < KEY_MAX; ++i)
    {
        if (ppsz_keys[i] && strcmp(ppsz_keys[i], psz_key) == 0)
            return i;
    }
    return -1;
}

static int
str2int(const char *psz_port)
{
    if (psz_port)
    {
        bool ok;
        int i_port = QString::fromLocal8Bit(psz_port).toInt(&ok);
        if (!ok)
            return -1;
        return i_port;
    }
    else
        return -1;
}

/**
 * Create a key and a map from values
 */
static QString
values2Key(const char * const ppsz_values[KEY_MAX])
{
    const char *psz_protocol = ppsz_values[KEY_PROTOCOL];
    const char *psz_user = ppsz_values[KEY_USER];
    const char *psz_server = ppsz_values[KEY_SERVER];
    const char *psz_path = ppsz_values[KEY_PATH];
    const char *psz_port = ppsz_values[KEY_PORT];
    int i_port = str2int(psz_port);

    if (!psz_protocol || !psz_server)
        return QString();

    QUrl url;
    url.setScheme(qfu(psz_protocol));
    url.setHost(qfu(psz_server));
    if (psz_user != NULL)
        url.setUserName(qfu(psz_user));
    if (psz_path != NULL)
        url.setPath(qfu(psz_path));

    if (i_port != -1)
        url.setPort(i_port);

    if (!url.isValid())
        return QString();
    return url.toString();
}

/**
 * In case of error, caller must free ppsz_values[*]
 */
static int
key2Values(const QString &key, char * ppsz_values[KEY_MAX])
{
    QUrl url(key);
    if (!url.isValid())
        return VLC_EGENERIC;

    if (url.scheme().isEmpty() || url.host().isEmpty())
        return VLC_EGENERIC;

    ppsz_values[KEY_PROTOCOL] = strdup(qtu(url.scheme()));
    ppsz_values[KEY_SERVER] = strdup(qtu(url.host()));
    if (!ppsz_values[KEY_PROTOCOL] || !ppsz_values[KEY_SERVER])
        return VLC_EGENERIC;

    if (!url.userName().isEmpty())
    {
        ppsz_values[KEY_USER] = strdup(qtu(url.userName()));
        if (!ppsz_values[KEY_USER])
            return VLC_EGENERIC;
    }
    if (!url.path().isEmpty())
    {
        ppsz_values[KEY_PATH] = strdup(qtu(url.path()));
        if (!ppsz_values[KEY_PATH])
            return VLC_EGENERIC;
    }

    int i_port = url.port();
    if (i_port != -1)
    {
        ppsz_values[KEY_PORT] = strdup(QString::number(i_port).toLocal8Bit().constData());
        if (!ppsz_values[KEY_PORT])
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static bool
matchQString(const char *psz_str, const QString &str)
{
    return psz_str == NULL || (!str.isEmpty() && strcmp(psz_str, qtu(str)) == 0);
}

static bool
matchKey(const QString &key, const char * const ppsz_values[KEY_MAX])
{
    QUrl url(key);
    if (!url.isValid())
        return false;

    if (url.scheme().isEmpty() || url.host().isEmpty())
        return false;

    const char *psz_protocol = ppsz_values[KEY_PROTOCOL];
    const char *psz_user = ppsz_values[KEY_USER];
    const char *psz_server = ppsz_values[KEY_SERVER];
    const char *psz_path = ppsz_values[KEY_PATH];
    const char *psz_port = ppsz_values[KEY_PORT];
    int i_port = str2int(psz_port);

    return (matchQString(psz_protocol, url.scheme())
        && matchQString(psz_user, url.userName())
        && matchQString(psz_server, url.host())
        && matchQString(psz_path, url.path())
        && (i_port == -1 || i_port == url.port()));
}

static QMap<QString, QString>
values2Map(const char * const ppsz_values[KEY_MAX])
{
    QMap<QString, QString> map;
    for (unsigned int i = 0; i < KEY_MAX; ++i)
    {
        const char *psz_key = ppsz_keys[i];
        if (psz_key && ppsz_values[i])
            map.insert(qfu(psz_key), qfu(ppsz_values[i]));
    }
    return map;
}

/**
 * Return true if all pairs of matchMap are in map (and not the contrary).
 */
static bool
matchMaps(const QMap<QString, QString> &matchMap,
          const QMap<QString, QString> &map)
{
    if (matchMap.isEmpty())
        return true;

    QMapIterator<QString, QString> it(matchMap);
    while (it.hasNext())
    {
        it.next();
        if (map.value(it.key()) != it.value())
            return false;
    }
    return true;
}

/**
 * Fill a keystore entry from a map and a key.
 * In case of error, caller must free p_entry
 */
static int
mapAndKey2Entry(const QMap<QString, QString> &map, const QString &key,
                vlc_keystore_entry *p_entry)
{
    QMapIterator<QString, QString> it(map);
    while (it.hasNext())
    {
        it.next();
        if (it.key() != MAP_KEY_SECRET)
        {
            /* Copy map pair to ppsz_values */
            const char *psz_key = qtu(it.key());
            int i_key = str2key(psz_key);
            if (i_key == -1 || i_key >= KEY_MAX)
                return VLC_EGENERIC;

            char *psz_value = strdup(qtu(it.value()));
            if (!psz_value)
                return VLC_EGENERIC;
            p_entry->ppsz_values[i_key] = psz_value;
        }
        else
        {
            /* Copy secret from the map */
            p_entry->i_secret_len = 
                vlc_b64_decode_binary(&p_entry->p_secret,
                                      it.value().toLocal8Bit().constData());
        }
    }
    if (!p_entry->i_secret_len)
        return VLC_EGENERIC;
    return key2Values(key, p_entry->ppsz_values);
}

VLCKWallet::VLCKWallet(QCoreApplication *p_qApp, vlc_object_t *p_obj)
    : mState(STATE_INIT)
    , mObj(p_obj)
    , mWallet(NULL)
{
    /* KWallet and Dbus methods need to be run from the mainloop */
    moveToThread(p_qApp->thread());
}

void
VLCKWallet::signalLocked(enum State state)
{
    mState = state;
    mCond.wakeOne();
}

void
VLCKWallet::signal(enum State state)
{
    QMutexLocker locker(&mMutex);
    signalLocked(state);
}

void
VLCKWallet::interrupted(void *data)
{
    VLCKWallet *self = (VLCKWallet *) data;

    self->signal(STATE_KWALLET_CLOSED);
}

/**
 * Interruptible wait for KWallet opening (or for an error)
 */
bool
VLCKWallet::waitOpened()
{
    QMutexLocker locker(&mMutex);

    vlc_interrupt_register(interrupted, this);

    while (mState == STATE_INIT)
        mCond.wait(&mMutex);

    vlc_interrupt_unregister();

    return mState == STATE_KWALLET_OPENED;
}

/**
 * Slot called when KWallet is opened
 */
void
VLCKWallet::kwalletOpen(bool opened)
{
    QMutexLocker locker(&mMutex);

    if (mState != STATE_INIT)
        return;

    if (opened)
    {
        /* Create VLC folder if it doesn't exist */
        if (!mWallet->hasFolder(VLC_KEYSTORE_NAME))
        {
            if (!mWallet->createFolder(VLC_KEYSTORE_NAME))
            {
                msg_Err(mObj, "could not create '%s' folder'", VLC_KEYSTORE_NAME);
                signalLocked(STATE_KWALLET_CLOSED);
                return;
            }
        }
        /* set VLC folder */
        if (!mWallet->setFolder(VLC_KEYSTORE_NAME))
        {
            signalLocked(STATE_KWALLET_CLOSED);
            return;
        }
        signalLocked(STATE_KWALLET_OPENED);
    }
    else
        signalLocked(STATE_KWALLET_CLOSED);
}

/**
 * Slot called when KWallet is closed
 */
void
VLCKWallet::kwalletClose()
{
    msg_Err(mObj, "VLCKWallet::kwalletClose\n");
    signal(STATE_KWALLET_CLOSED);
}

/**
 * Open from a VLC thread
 * Returns true if case of success.
 */
bool
VLCKWallet::open(bool b_force)
{
    /* Open from the mainloop, KWallet and Dbus methods need to be run from the
     * mainloop */
    if (!connect(this, SIGNAL(opened(bool)), this, SLOT(mainloopOpen(bool)),
                 Qt::QueuedConnection))
        return false;
    emit opened(b_force);

    bool b_opened = waitOpened();
    disconnect(this, SIGNAL(opened(bool)), this, SLOT(mainloopOpen(bool)));

    if (!b_opened)
        return false;

    if (!connect(this, SIGNAL(closed(void)), this, SLOT(mainloopClose(void)),
                 Qt::QueuedConnection))
        return false;

    /* Theses slots will be executed on the mainloop and will be blocking */

    if (!connect(this, SIGNAL(stored(const char * const *,
                 const uint8_t *, size_t, const char *, int &)),
                 this, SLOT(mainloopStore(const char * const *,
                 const uint8_t *, size_t, const char *, int &)),
                 Qt::BlockingQueuedConnection))
        return false;

    if (!connect(this, SIGNAL(found(const char * const *,
                 vlc_keystore_entry **, unsigned int &)),
                 this, SLOT(mainloopFind(const char * const *,
                 vlc_keystore_entry **, unsigned int &)),
                 Qt::BlockingQueuedConnection))
        return false;

    if (!connect(this, SIGNAL(removed(const char * const *, unsigned int &)),
                 this, SLOT(mainloopRemove(const char * const *, unsigned int &)),
                 Qt::BlockingQueuedConnection))
        return false;

    return true;
}

/**
 * Open from the main loop
 */
void
VLCKWallet::mainloopOpen(bool b_force)
{
    QMutexLocker locker(&mMutex);

    if (mState != STATE_INIT)
        return;

    if (!b_force)
    {
        /* First, check if kwallet service is running. Indeed,
         * KWallet::Wallet::openWallet() will spawn a service if it's not
         * running, even on non KDE environments */
        bool b_registered = false;
        QDBusConnectionInterface *intf = QDBusConnection::sessionBus().interface();

        for (int i = 0; i < kwalletServices.size(); ++i)
        {
            if (intf->isServiceRegistered(kwalletServices.at(i)))
            {
                b_registered = true;
                break;
            }
        }
        if (!b_registered)
        {
            signalLocked(STATE_KWALLET_CLOSED);
            return;
        }
    }

    mWallet = KWallet::Wallet::openWallet(KWallet::Wallet::NetworkWallet(),
                                          0, KWallet::Wallet::Asynchronous);
    if (!mWallet)
    {
        msg_Err(mObj, "openWallet failed");
        signalLocked(STATE_KWALLET_CLOSED);
        return;
    }
    mWallet->setParent(this);

    /* Connect KWallet signals */
    if (!connect(mWallet, SIGNAL(walletOpened(bool)),
                 this, SLOT(kwalletOpen(bool))))
    {
        msg_Err(mObj, "could not connect to walletOpened");
        signalLocked(STATE_KWALLET_CLOSED);
        return;
    }
    if (!connect(mWallet, SIGNAL(walletClosed()), this, SLOT(kwalletClose())))
    {
        msg_Err(mObj, "could not connect to walletClosed");
        signalLocked(STATE_KWALLET_CLOSED);
        return;
    }
}

/**
 * Close from a VLC thread
 */
void
VLCKWallet::close()
{
    signal(STATE_KWALLET_CLOSED);
    emit closed();
}

/**
 * Close from the mainloop
 */
void
VLCKWallet::mainloopClose(void)
{
    /* delete VLCKWallet, and its child (mWallet) */
    delete this;
}

/**
 * Store from a VLC thread
 */
int
VLCKWallet::store(const char * const ppsz_values[KEY_MAX],
                  const uint8_t *p_secret, size_t i_secret_len,
                  const char *psz_label)
{
    int i_ret = VLC_EGENERIC;
    emit stored(ppsz_values, p_secret, i_secret_len, psz_label, i_ret);
    return i_ret;
}

/**
 * Store from the mainloop
 */
void
VLCKWallet::mainloopStore(const char * const *ppsz_values,
                          const uint8_t *p_secret, size_t i_secret_len,
                          const char *psz_label, int &i_ret)
{
    (void) psz_label;

    QMutexLocker locker(&mMutex);
    if (mState != STATE_KWALLET_OPENED)
        return;

    /* Get key and map from values */
    QString key = values2Key(ppsz_values);
    if (key.isEmpty())
        return;

    QMap<QString, QString> map = values2Map(ppsz_values);

    /* Encode secret, since KWallet can't store binary */
    char *psz_b64_secret = vlc_b64_encode_binary(p_secret, i_secret_len);
    if (!psz_b64_secret)
        return;
    /* Write the secret into the map */
    map.insert(MAP_KEY_SECRET, QString(psz_b64_secret));
    free(psz_b64_secret);

    /* Write the map at the specified key */
    if (mWallet->writeMap(key, map) != 0)
        return;

    i_ret = VLC_SUCCESS;
}

/**
 * Find from a VLC thread
 */
unsigned int
VLCKWallet::find(const char * const ppsz_values[KEY_MAX],
                 vlc_keystore_entry **pp_entries)
{
    unsigned int i_entry_count = 0;
    emit found(ppsz_values, pp_entries, i_entry_count);
    return i_entry_count;
}

/**
 * Find from the mainloop
 */
void
VLCKWallet::mainloopFind(const char * const *ppsz_values,
                         vlc_keystore_entry **pp_entries,
                         unsigned int &i_entry_count)
{
    QMutexLocker locker(&mMutex);
    if (mState != STATE_KWALLET_OPENED)
        return;

    /* Get map from values */
    QMap<QString, QString> matchMap = values2Map(ppsz_values);

    /* Fetch all maps */
    QMap<QString, QMap<QString, QString>> mapMap;
    if (mWallet->readMapList(QString("*"), mapMap) != 0)
        return;

    vlc_keystore_entry *p_entries = (vlc_keystore_entry *)
        calloc(mapMap.size(), sizeof(vlc_keystore_entry));

    QMapIterator<QString, QMap<QString, QString>> it(mapMap);
    while (it.hasNext())
    {
        it.next();

        if (!matchKey(it.key(), ppsz_values) || !matchMaps(matchMap, it.value()))
            continue;

        /* Matching key/value */
        vlc_keystore_entry *p_entry = &p_entries[i_entry_count++];

        /* Fill the entry from the map and the key */
        if (mapAndKey2Entry(it.value(), it.key(), p_entry))
        {
            vlc_keystore_release_entries(p_entries, i_entry_count);
            i_entry_count = 0;
            return;
        }
    }
    *pp_entries = p_entries;
}

/**
 * Remove from a VLC thread
 */
unsigned int
VLCKWallet::remove(const char * const ppsz_values[KEY_MAX])
{
    unsigned int i_entry_count = 0;
    emit removed(ppsz_values, i_entry_count);
    return i_entry_count;
}

/**
 * Remove from the mainloop
 */
void
VLCKWallet::mainloopRemove(const char * const *ppsz_values,
                           unsigned int &i_entry_count)
{
    QMutexLocker locker(&mMutex);
    if (mState != STATE_KWALLET_OPENED)
        return;

    QMap<QString, QString> matchMap = values2Map(ppsz_values);

    /* Fetch all maps */
    QMap<QString, QMap<QString, QString>> mapMap;
    if (mWallet->readMapList("*", mapMap) != 0)
        return;

    QMapIterator<QString, QMap<QString, QString>> it(mapMap);
    while (it.hasNext())
    {
        it.next();

        if (!matchKey(it.key(), ppsz_values) || !matchMaps(matchMap, it.value()))
            continue;

        /* Matching key/value */
        if (mWallet->removeEntry(it.key()) == 0)
            i_entry_count++;
    }
}

static int
Store(vlc_keystore *p_keystore, const char * const ppsz_values[KEY_MAX],
      const uint8_t *p_secret, size_t i_secret_len, const char *psz_label)
{
    VLCKWallet *p_wallet = (VLCKWallet *) p_keystore->p_sys;

    return p_wallet->store(ppsz_values, p_secret, i_secret_len, psz_label);
}

static unsigned int
Find(vlc_keystore *p_keystore, const char * const ppsz_values[KEY_MAX],
     vlc_keystore_entry **pp_entries)
{
    VLCKWallet *p_wallet = (VLCKWallet *) p_keystore->p_sys;

    return p_wallet->find(ppsz_values, pp_entries);
}

static unsigned int
Remove(vlc_keystore *p_keystore, const char * const ppsz_values[KEY_MAX])
{
    VLCKWallet *p_wallet = (VLCKWallet *) p_keystore->p_sys;

    return p_wallet->remove(ppsz_values);
}

static int
Open(vlc_object_t *p_this)
{
    /* KWallet and DBus methods need to be run from the QApplication main loop.
     * There can be only one QApplication and it's currently used by the qt
     * interface.
     * TODO: Spawn the Qt thread singleton from here or from the qt interface
     * module */
    QCoreApplication *p_qApp = QCoreApplication::instance();
    if (p_qApp == NULL)
        return VLC_EGENERIC;

    VLCKWallet *p_wallet = new VLCKWallet(p_qApp, p_this);
    if (!p_wallet)
        return VLC_EGENERIC;

    if (!p_wallet->open(p_this->obj.force))
    {
        p_wallet->close();
        return VLC_EGENERIC;
    }

    vlc_keystore *p_keystore = (vlc_keystore *)p_this;
    p_keystore->p_sys = (vlc_keystore_sys *) p_wallet;
    p_keystore->pf_store = Store;
    p_keystore->pf_find = Find;
    p_keystore->pf_remove = Remove;

    return VLC_SUCCESS;
}

static void
Close(vlc_object_t *p_this)
{
    vlc_keystore *p_keystore = (vlc_keystore *)p_this;
    VLCKWallet *p_wallet = (VLCKWallet *) p_keystore->p_sys;
    p_wallet->close();
}
