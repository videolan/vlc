/*
 * Thumbnailer implementation
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "thumbnailer.hpp"

#include <QQuickTextureFactory>
#include <QQuickWindow>
#include <QUrlQuery>
#include <QCoreApplication>
#include <QDateTime>
#include <QThread>
#include <QAtomicInt>
#include <algorithm>
#include <cstring>

#include <vlc_interface.h>
#include <vlc_objects.h>
#include <vlc_block.h>

// ---------------------- ThumbnailImageResponse ----------------------

class ThumbnailImageResponse : public QQuickImageResponse
{
public:
	ThumbnailImageResponse(ThumbnailImageProvider* provider,
						   const QUrl& mediaUrl,
						   double position,
						   const QSize& requestedSize,
						   bool crop,
						   bool precise)
	: m_providerWeak(provider)
	{
	// Snapshot core pointers; avoid dereferencing provider later
	m_preparser = provider ? provider->preparser() : nullptr;
	m_owner = provider ? provider->ownerObject() : nullptr;
	if (m_owner)
		msg_Dbg(m_owner, "ThumbnailImageResponse: start url=%s pos=%.3f size=%dx%d",
				qtu(mediaUrl.toString()), position, requestedSize.width(), requestedSize.height());
		// Run thumbnail request in a worker thread without QtConcurrent
		m_thread = QThread::create([this, mediaUrl, position, requestedSize, crop, precise]() {
			QImage img = this->generate(mediaUrl, position, requestedSize, crop, precise);
			QMetaObject::invokeMethod(this, [this, img = std::move(img)]() mutable {
				m_image = std::move(img);
				Q_EMIT finished();
			}, Qt::QueuedConnection);
		});
		if (m_thread) {
			QObject::connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
			m_thread->start();
		}
	}

	~ThumbnailImageResponse() override
	{
		if (m_thread && m_thread->isRunning()) {
			// The thread runs a single function and should finish quickly; wait briefly.
			m_thread->wait();
		}
		// m_thread is set to deleteLater on finished
		if (!m_providerWeak.isNull())
			m_providerWeak.data()->decActive();
		if (m_owner)
			msg_Dbg(m_owner, "ThumbnailImageResponse: destroyed");
	}

	QQuickTextureFactory *textureFactory() const override
	{
		return QQuickTextureFactory::textureFactoryForImage(m_image);
	}

	QString errorString() const override { return m_error; }

private:
	QImage generate(const QUrl& mediaUrl, double position, const QSize& requestedSize, bool crop, bool precise)
	{
		if (!m_preparser || !m_owner) {
			m_error = QStringLiteral("No thumbnailer");
			if (m_owner)
				msg_Warn(m_owner, "ThumbnailImageResponse: no preparser/owner");
			return {};
		}

		// Build input item
		QByteArray mrl = mediaUrl.toString(QUrl::FullyEncoded).toUtf8();
		input_item_t* item = input_item_New(mrl.constData(), NULL);
		if (!item) {
			m_error = QStringLiteral("Failed to create input item");
			return {};
		}

		// Prepare thumbnailer args
	struct vlc_thumbnailer_arg arg = {};
	arg.seek.type = vlc_thumbnailer_arg::seek::VLC_THUMBNAILER_SEEK_POS;
	arg.seek.pos = position;
	arg.seek.speed = precise ? vlc_thumbnailer_arg::seek::VLC_THUMBNAILER_SEEK_PRECISE
				  : vlc_thumbnailer_arg::seek::VLC_THUMBNAILER_SEEK_FAST;
		arg.hw_dec = false;

		const unsigned w = requestedSize.width() > 0 ? unsigned(requestedSize.width()) : 0u;
		const unsigned h = requestedSize.height() > 0 ? unsigned(requestedSize.height()) : 0u;

		// Simple synchronous waiter using a local semaphore
		class Sync {
		public:
			QAtomicInt done{0};
			int status = VLC_SUCCESS;
			picture_t* pic = nullptr;
			QAtomicInt detached{0}; // if set, callback will delete this
			~Sync() {
				if (pic) picture_Release(pic);
			}
		} sync;

		auto onEnded = [](input_item_t* it, int status, picture_t* thumbnail, void* data) {
			Q_UNUSED(it);
			Sync* s = static_cast<Sync*>(data);
			s->status = status;
			s->pic = thumbnail ? picture_Hold(thumbnail) : nullptr;
			s->done.storeRelaxed(1);
			if (s->detached.loadRelaxed())
				delete s;
		};

		const struct vlc_thumbnailer_cbs cbs = {
			.on_ended = onEnded,
		};

		// Configure a per-request preparser timeout: keep small
	vlc_preparser_SetTimeout(m_preparser, VLC_TICK_FROM_MS(5000));

		// allocate context on heap so it's safe even if we return early
		Sync* ctx = new Sync();

		const vlc_preparser_req_id id = vlc_preparser_GenerateThumbnail(
			m_preparser, item, &arg, &cbs, ctx);
		if (id == VLC_PREPARSER_REQ_ID_INVALID) {
			delete ctx;
			input_item_Release(item);
			m_error = QStringLiteral("Thumbnail request failed");
			if (m_owner)
				msg_Warn(m_owner, "ThumbnailImageResponse: GenerateThumbnail failed");
			return {};
		}

		// Wait for completion or timeout
		const qint64 start = QDateTime::currentMSecsSinceEpoch();
		const qint64 timeout = 5000;
		while (!ctx->done.loadRelaxed()) {
			QThread::msleep(5);
			if (QDateTime::currentMSecsSinceEpoch() - start > timeout)
				break; // let internal timeout trigger callback
		}

	QImage out;
	if (ctx->done.loadRelaxed() && ctx->status == VLC_SUCCESS && ctx->pic) {
			// Export picture to PNG in-memory, then load QImage from data
			block_t* block = nullptr;
			int ret = picture_Export(m_owner, &block, NULL,
									 ctx->pic, VLC_CODEC_PNG, w, h, crop);
			if (ret == VLC_SUCCESS && block && block->p_buffer && block->i_buffer > 0) {
				out.loadFromData(block->p_buffer, int(block->i_buffer), "PNG");
			} else {
				m_error = QStringLiteral("Export failed");
			}
			if (block) block_Release(block);
			// we've consumed the picture; prevent destructor double-release
			picture_Release(ctx->pic);
			ctx->pic = nullptr;
			delete ctx;
			ctx = nullptr;
			if (m_owner)
				msg_Dbg(m_owner, "ThumbnailImageResponse: success (%dx%d)", out.width(), out.height());
		} else if (!ctx->done.loadRelaxed()) {
			m_error = QStringLiteral("No thumbnail generated (timeout)");
			// Detach and let the callback clean up when it eventually fires
			ctx->detached.storeRelaxed(1);
			if (m_owner)
				msg_Warn(m_owner, "ThumbnailImageResponse: timeout waiting for thumbnail");
		} else {
			// done but failed or no picture: callback already fired; free ctx here
			m_error = QStringLiteral("No thumbnail generated");
			int st = ctx->status; // cache before delete
			delete ctx;
			ctx = nullptr;
			if (m_owner)
				msg_Warn(m_owner, "ThumbnailImageResponse: done without picture (status=%d)", st);
		}

		input_item_Release(item);
		return out;
	}

private:
	QPointer<ThumbnailImageProvider> m_providerWeak;
	vlc_preparser_t* m_preparser = nullptr;
	vlc_object_t* m_owner = nullptr;
	QThread* m_thread = nullptr;
	mutable QString m_error;
	QImage m_image;
};

// ---------------------- ThumbnailImageProvider ----------------------

ThumbnailImageProvider::ThumbnailImageProvider(qt_intf_t* intf)
	: QQuickAsyncImageProvider()
	, m_intf(intf)
{
	if (m_intf)
		msg_Dbg(m_intf, "ThumbnailImageProvider: init");
	if (qEnvironmentVariableIntValue("VLC_DISABLE_THUMBNAILER") == 1) {
		msg_Warn(m_intf, "Thumbnailer disabled by VLC_DISABLE_THUMBNAILER=1");
		return;
	}
	// Create a dedicated preparser for thumbnailing (lazy)
	libvlc_int_t* libvlc = vlc_object_instance(&intf->obj);
	const struct vlc_preparser_cfg cfg = {
		.types = VLC_PREPARSER_TYPE_THUMBNAIL,
		.max_parser_threads = 0,
		.max_thumbnailer_threads = 1,
		.timeout = 0,
	};
	m_preparser = vlc_preparser_New(VLC_OBJECT(libvlc), &cfg);
	if (!m_preparser)
		msg_Err(m_intf, "Failed to create thumbnail preparser");
	else
		msg_Dbg(m_intf, "ThumbnailImageProvider: preparser created");
}

ThumbnailImageProvider::~ThumbnailImageProvider()
{
	m_shuttingDown = true;
	if (m_intf)
		msg_Dbg(m_intf, "ThumbnailImageProvider: shutting down, active responses=%d", m_activeResponses.loadRelaxed());
	// Wait briefly for outstanding responses to finish
	for (int i = 0; i < 500 && m_activeResponses.loadRelaxed() > 0; ++i)
		QThread::msleep(2);
	if (m_preparser)
		vlc_preparser_Delete(m_preparser);
	m_preparser = nullptr;
	if (m_intf)
		msg_Dbg(m_intf, "ThumbnailImageProvider: destroyed");
}

QQuickImageResponse* ThumbnailImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
	if (m_shuttingDown || !m_preparser) {
		m_activeResponses.ref();
		return new ThumbnailImageResponse(this, QUrl(), 0.0, requestedSize, false, false);
	}
	// id example: "mrl=<url>&pos=0.42&crop=1&precise=0"
	if (m_intf)
		msg_Dbg(m_intf, "ThumbnailImageProvider: request id='%s' size=%dx%d", qtu(id), requestedSize.width(), requestedSize.height());
	QUrlQuery q(id);
	const QUrl mediaUrl = QUrl(q.queryItemValue(QStringLiteral("mrl")));
	const double pos = q.queryItemValue(QStringLiteral("pos")).toDouble();
	const QString cropStr = q.queryItemValue(QStringLiteral("crop"));
	const bool crop = cropStr.isEmpty() ? true : (cropStr != QStringLiteral("0"));
	const QString preciseStr = q.queryItemValue(QStringLiteral("precise"));
	const bool precise = preciseStr == QStringLiteral("1");
	QSize sz = requestedSize;
	const int w = q.queryItemValue(QStringLiteral("w")).toInt();
	const int h = q.queryItemValue(QStringLiteral("h")).toInt();
	if (w > 0 || h > 0)
		sz = QSize(w, h);

	m_activeResponses.ref();
	if (m_intf)
		msg_Dbg(m_intf, "ThumbnailImageProvider: dispatch thumbnail pos=%.3f size=%dx%d crop=%d precise=%d",
				pos, sz.width(), sz.height(), int(crop), int(precise));
	auto* r = new ThumbnailImageResponse(this, mediaUrl, pos, sz, crop, precise);
	return r;
}

// ---------------------- Thumbnailer (QML helper) ----------------------

Thumbnailer::Thumbnailer(QObject* parent)
	: QObject(parent)
{
}

QUrl Thumbnailer::buildSource(const QUrl& mediaUrl, double position, int width, int height,
							  bool crop, bool precise) const
{
	const int w = width > 0 ? width : m_width;
	const int h = height > 0 ? height : m_height;
	const bool c = crop;
	const bool p = precise;
	QUrl u;
	u.setScheme(QStringLiteral("image"));
	u.setHost(QStringLiteral("thumbnail"));
	QUrlQuery q;
	q.addQueryItem(QStringLiteral("mrl"), mediaUrl.toString(QUrl::FullyEncoded));
	q.addQueryItem(QStringLiteral("pos"), QString::number(position, 'f', 6));
	q.addQueryItem(QStringLiteral("w"), QString::number(w));
	q.addQueryItem(QStringLiteral("h"), QString::number(h));
	q.addQueryItem(QStringLiteral("crop"), c ? QStringLiteral("1") : QStringLiteral("0"));
	q.addQueryItem(QStringLiteral("precise"), p ? QStringLiteral("1") : QStringLiteral("0"));
	u.setQuery(q);
	return u;
}

