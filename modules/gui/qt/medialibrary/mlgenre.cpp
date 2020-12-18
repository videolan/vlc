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

#include <cassert>
#include <QPainter>
#include <QImage>
#include <QThreadPool>
#include <QMutex>
#include <QWaitCondition>
#include <QDir>
#include <QGradient>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsBlurEffect>
#include <algorithm>
#include "mlgenre.hpp"
#include "qt.hpp"

namespace  {

#define THUMBNAIL_WIDTH 260
#define THUMBNAIL_HEIGHT 130

QImage blurImage(const QImage& src)
{
    QGraphicsScene scene;
    QGraphicsPixmapItem item;
    item.setPixmap(QPixmap::fromImage(src));
    QGraphicsBlurEffect blurEffect;
    blurEffect.setBlurRadius(4);
    blurEffect.setBlurHints(QGraphicsBlurEffect::QualityHint);
    item.setGraphicsEffect(&blurEffect);
    scene.addItem(&item);
    QImage res(src.size(), QImage::Format_ARGB32);
    QPainter ptr(&res);
    scene.render(&ptr);
    return res;
}

class GenerateCoverTask : public QRunnable
{
public:
    GenerateCoverTask(vlc_medialibrary_t* ml, MLGenre* genre, QString filepath)
        : QRunnable()
        , m_ml(ml)
        , m_genre(genre)
        , m_filepath(filepath)
    {
    }

    void drawRegion(QPainter& target, QString source, const QRect& rect)
    {
        QImage tmpImage;
        if (tmpImage.load(source))
        {
            QRect sourceRect;
            int size = std::min(tmpImage.width(), tmpImage.height());
            if (rect.width() == rect.height())
            {
                sourceRect = QRect( (tmpImage.width() - size) / 2,
                                    (tmpImage.height() - size) / 2,
                                    size,
                                    size);
            }
            else if (rect.width() > rect.height())
            {
                sourceRect = QRect( (tmpImage.width() - size) / 2,
                                    (tmpImage.height() - size/2) / 2,
                                    size,
                                    size/2);
            }
            else
            {
                sourceRect = QRect( (tmpImage.width() - size / 2) / 2,
                                    (tmpImage.height() - size) / 2,
                                    size/2,
                                    size);
            }
            target.drawImage(rect, tmpImage, sourceRect);
        }
        else
        {
            target.setPen(Qt::black);
            target.drawRect(rect);
        }
    }

    void run() override
    {
        {
            QMutexLocker lock(&m_taskLock);
            if (m_canceled) {
                m_taskCond.wakeAll();
                return;
            }
            m_running = true;
        }

        int64_t genreId = m_genre->getId().id;
        ml_unique_ptr<vlc_ml_album_list_t> album_list;
        //TODO only retreive albums with a cover.
        vlc_ml_query_params_t queryParams;
        memset(&queryParams, 0, sizeof(vlc_ml_query_params_t));
        album_list.reset( vlc_ml_list_genre_albums(m_ml, &queryParams, genreId) );

        QStringList thumbnails;
        thumbnails.reserve(8);
        for( const vlc_ml_album_t& media: ml_range_iterate<vlc_ml_album_t>( album_list ) ) {
            if (media.thumbnails[VLC_ML_THUMBNAIL_SMALL].i_status ==
                    VLC_ML_THUMBNAIL_STATUS_AVAILABLE) {
                QUrl mediaURL( media.thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl );
                //QImage only accept local file
                if (mediaURL.isValid() && mediaURL.isLocalFile()) {
                    thumbnails.append(mediaURL.path());
                    if (thumbnails.size() == 8)
                        break;
                }
            }
        }

        if (thumbnails.empty()) {
            thumbnails.append(":/noart_album.svg");
        }

        assert(thumbnails.size() <= 8);
        std::copy(thumbnails.begin(), ( thumbnails.begin() + ( 8 - thumbnails.size() ) ), std::back_inserter(thumbnails));
        assert(thumbnails.size() == 8);

        {
            QMutexLocker lock(&m_taskLock);
            if (m_canceled) {
                m_running = false;
                m_taskCond.wakeAll();
                return;
            }
        }

        QImage image(THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT, QImage::Format_RGB32);
        image.fill(Qt::white);

        QPainter painter;
        painter.begin(&image);
        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 4; j++) {
                drawRegion(painter, thumbnails[2*j+1], QRect( ( THUMBNAIL_WIDTH / 4 ) * j, ( THUMBNAIL_HEIGHT / 2 ) * i, THUMBNAIL_WIDTH / 4, THUMBNAIL_HEIGHT / 2 ));
            }
        }
        painter.end();

        image = blurImage(image);

        QLinearGradient gradient;
        gradient.setColorAt(0, QColor(0, 0, 0, 255*.3));
        gradient.setColorAt(1, QColor(0, 0, 0, 255*.7));
        painter.begin(&image);
        painter.setOpacity(.7);
        painter.fillRect(image.rect(), gradient);
        painter.end();

        if (image.save(m_filepath, "jpg"))
            /* Set the cover from the main thread */
            QMetaObject::invokeMethod(m_genre, [genre = m_genre, cover = QUrl::fromLocalFile(m_filepath).toString()]
                {
                    genre->setCover(std::move(cover));
                });

        {
            QMutexLocker lock(&m_taskLock);
            m_running = false;
            m_taskCond.wakeAll();
        }
    }

    void cancel()
    {
        QMutexLocker lock(&m_taskLock);
        m_canceled = true;
        if (!m_running)
            return;
        m_taskCond.wait(&m_taskLock);
    }

private:
    bool m_canceled = false;
    bool m_running = false;
    QMutex m_taskLock;
    QWaitCondition m_taskCond;

    vlc_medialibrary_t* m_ml = nullptr;
    MLGenre* m_genre = nullptr;
    QString m_filepath;
};

}


MLGenre::MLGenre(vlc_medialibrary_t* ml, const vlc_ml_genre_t *_data, QObject *_parent )
    : QObject(_parent)
    , MLItem    ( MLItemId( _data->i_id, VLC_ML_PARENT_GENRE ) )
    , m_ml      ( ml )
    , m_name    ( QString::fromUtf8( _data->psz_name ) )
    , m_nbTracks ( (unsigned int)_data->i_nb_tracks )

{
    assert(_data);
    connect(this, &MLGenre::askGenerateCover, this, &MLGenre::generateThumbnail);
}

MLGenre::~MLGenre()
{
    if (m_coverTask) {
        if (!QThreadPool::globalInstance()->tryTake(m_coverTask)) {
            //task is done or running
            static_cast<GenerateCoverTask*>(m_coverTask)->cancel();
        }
        delete m_coverTask;
    }
}

QString MLGenre::getName() const
{
    return m_name;
}

unsigned int MLGenre::getNbTracks() const
{
    return m_nbTracks;
}

QString MLGenre::getCover() const
{
    if (!m_cover.isEmpty())
        return  m_cover;
    if (!m_coverTask) {
        emit askGenerateCover( QPrivateSignal() );
    }
    return m_cover;
}

void MLGenre::setCover(QString cover)
{
    m_cover = cover;
    //TODO store in media library
}

void MLGenre::generateThumbnail()
{
    if (!m_coverTask && m_cover.isNull()) {

        QDir dir(config_GetUserDir(VLC_CACHE_DIR));
        dir.mkdir("art");
        dir.cd("art");
        dir.mkdir("qt-genre-covers");
        dir.cd("qt-genre-covers");

        QString filename = QString("genre_thumbnail_%1.jpg").arg(getId().id);
        QString absoluteFilePath =  dir.absoluteFilePath(filename);
        if (dir.exists(filename))
        {
            setCover(QUrl::fromLocalFile(absoluteFilePath).toString());
        }
        else
        {
            GenerateCoverTask* coverTask = new GenerateCoverTask(m_ml, this, absoluteFilePath);
            coverTask->setAutoDelete(false);
            m_coverTask = coverTask;
            QThreadPool::globalInstance()->start(coverTask);
        }
    }
}

