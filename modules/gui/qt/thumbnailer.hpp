/*
 * Thumbnailer: QML-facing helper and image provider for timeline previews
 */

#ifndef VLC_QT_THUMBNAILER_HPP
#define VLC_QT_THUMBNAILER_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QObject>
#include <QImage>
#include <QSize>
#include <QUrl>
#include <QQuickAsyncImageProvider>
#include <QPointer>
#include <QAtomicInt>

#include <vlc_common.h>
#include <vlc_preparser.h>
#include <vlc_picture.h>
#include <vlc_input_item.h>

#include "qt.hpp" // qt_intf_t

// Async image response handling a single libvlc thumbnail request
class ThumbnailImageResponse;

// Image provider used via Image { source: "image://thumbnail?..." }
class ThumbnailImageProvider : public QQuickAsyncImageProvider
{
public:
	explicit ThumbnailImageProvider(qt_intf_t* intf);
	~ThumbnailImageProvider() override;

	QQuickImageResponse* requestImageResponse(const QString &id, const QSize &requestedSize) override;

	// Exposed for tests only
	vlc_preparser_t* preparser() const { return m_preparser; }
	vlc_object_t* ownerObject() const { return &m_intf->obj; }

	// Lifetime helpers
	void decActive() { m_activeResponses.deref(); }

private:
	qt_intf_t* m_intf = nullptr;
	vlc_preparser_t* m_preparser = nullptr;
	QAtomicInt m_activeResponses{0};
	bool m_shuttingDown = false;
	bool m_enabled = true;
};

// QML helper to build provider URLs and expose basic settings
class Thumbnailer : public QObject
{
	Q_OBJECT
	Q_PROPERTY(int defaultWidth READ defaultWidth WRITE setDefaultWidth NOTIFY defaultsChanged FINAL)
	Q_PROPERTY(int defaultHeight READ defaultHeight WRITE setDefaultHeight NOTIFY defaultsChanged FINAL)
	Q_PROPERTY(bool crop READ crop WRITE setCrop NOTIFY defaultsChanged FINAL)
	Q_PROPERTY(bool precise READ precise WRITE setPrecise NOTIFY defaultsChanged FINAL)

public:
	explicit Thumbnailer(QObject* parent = nullptr);

	int defaultWidth() const { return m_width; }
	void setDefaultWidth(int w) { if (m_width == w) return; m_width = w; emit defaultsChanged(); }

	int defaultHeight() const { return m_height; }
	void setDefaultHeight(int h) { if (m_height == h) return; m_height = h; emit defaultsChanged(); }

	bool crop() const { return m_crop; }
	void setCrop(bool c) { if (m_crop == c) return; m_crop = c; emit defaultsChanged(); }

	bool precise() const { return m_precise; }
	void setPrecise(bool p) { if (m_precise == p) return; m_precise = p; emit defaultsChanged(); }

	// Build a provider URL with given media url and position (0..1)
	Q_INVOKABLE QUrl buildSource(const QUrl& mediaUrl, double position, int width = -1, int height = -1,
								 bool crop = true, bool precise = false) const;

signals:
	void defaultsChanged();

private:
	int m_width = 256;
	int m_height = 144;
	bool m_crop = true;
	bool m_precise = false; // fast by default
};

#endif // VLC_QT_THUMBNAILER_HPP

