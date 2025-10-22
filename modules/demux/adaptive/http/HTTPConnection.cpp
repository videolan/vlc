/*
 * HTTPConnection.cpp
 *****************************************************************************
 * Copyright (C) 2014-2015 - VideoLAN and VLC Authors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "HTTPConnection.hpp"
#include "ConnectionParams.hpp"
#include "AuthStorage.hpp"
#include "../BlockStreamInterface.hpp"
#include "../plumbing/SourceStream.hpp"

#include <optional>

#include <vlc_stream.h>
#include <vlc_keystore.h>

extern "C"
{
    #include "access/http/resource.h"
    #include "access/http/connmgr.h"
    #include "access/http/conn.h"
    #include "access/http/message.h"
}

using namespace adaptive::http;

AbstractConnection::AbstractConnection(vlc_object_t *p_object_)
{
    p_object = p_object_;
    available = true;
    bytesRead = 0;
    contentLength = 0;
}

AbstractConnection::~AbstractConnection()
{

}

bool AbstractConnection::prepare(const ConnectionParams &params_)
{
    if (!available)
        return false;
    params = params_;
    locationparams = ConnectionParams();
    available = false;
    return true;
}

size_t AbstractConnection::getContentLength() const
{
    return contentLength;
}

size_t AbstractConnection::getBytesRead() const
{
    return bytesRead;
}

const std::string & AbstractConnection::getContentType() const
{
    return contentType;
}

const ConnectionParams & AbstractConnection::getRedirection() const
{
    return locationparams;
}

class adaptive::http::LibVLCHTTPSource : public adaptive::BlockStreamInterface
{
     public:
        LibVLCHTTPSource(vlc_object_t *p_object_, struct vlc_http_cookie_jar_t *jar)
        {
            p_object = p_object_;
            http_mgr = vlc_http_mgr_create(p_object, jar);
            http_res = nullptr;
            totalRead = 0;
        }
        virtual ~LibVLCHTTPSource()
        {
            if(http_mgr)
                vlc_http_mgr_destroy(http_mgr);
        }
        block_t *readNextBlock() override
        {
            if(http_res == nullptr)
                return nullptr;
            block_t *b = vlc_http_res_read(http_res);
            if(b == vlc_http_error)
                return nullptr;
            if(b)
                totalRead += b->i_buffer;
            return b;
        }
        void reset()
        {
            if(http_res)
            {
                vlc_http_res_destroy(http_res);
                http_res = nullptr;
                totalRead = 0;
            }
        }

    private:
        struct restuple
        {
            struct vlc_http_resource resource;
            LibVLCHTTPSource *source;
        };

        int formatRequest(const struct vlc_http_resource *,
                          struct vlc_http_msg *req)
        {
            vlc_http_msg_add_header(req, "Accept-Encoding", "deflate, gzip");
            vlc_http_msg_add_header(req, "Cache-Control", "no-cache");
            if(range.isValid())
            {
                if(range.getEndByte() > 0)
                {
                    if (vlc_http_msg_add_header(req, "Range", "bytes=%zu-%zu",
                                                range.getStartByte(), range.getEndByte()))
                        return -1;
                }
                else
                {
                    if (vlc_http_msg_add_header(req, "Range", "bytes=%zu-",
                                                range.getStartByte()))
                        return -1;
                }
            }
            return 0;
        }

        int validateResponse(const struct vlc_http_resource *, const struct vlc_http_msg *resp)
        {
            if (vlc_http_msg_get_status(resp) == 206)
            {
                const char *str = vlc_http_msg_get_header(resp, "Content-Range");
                if (str == NULL)
                    /* A multipart/byteranges response. This is not what we asked for
                     * and we do not support it. */
                    return -1;

                uintmax_t start, end;
                if (sscanf(str, "bytes %" SCNuMAX "-%" SCNuMAX, &start, &end) != 2
                 || start != range.getStartByte() || start > end ||
                 (range.getEndByte() > range.getStartByte() && range.getEndByte() != end) )
                    /* A single range response is what we asked for, but not at that
                     * start offset. */
                    return -1;
            }
            return 0;
        }

        static int formatrequest_handler(const struct vlc_http_resource *res,
                                         struct vlc_http_msg *req, void *opaque)
        {
            return (*static_cast<LibVLCHTTPSource **>(opaque))->formatRequest(res, req);
        }

        static int validateresponse_handler(const struct vlc_http_resource *res,
                                            const struct vlc_http_msg *resp, void *opaque)
        {
            return (*static_cast<LibVLCHTTPSource **>(opaque))->validateResponse(res, resp);
        }

        vlc_object_t *p_object;
        static const struct vlc_http_resource_cbs callbacks;
        size_t totalRead;
        struct vlc_http_mgr *http_mgr;
        BytesRange range;
        struct vlc_http_resource *http_res;
        std::optional<std::string> username;
        std::optional<std::string> password;
        ConnectionParams lastparams;

    public:
        void setCredentials(const char *psz_username, const char *psz_password)
        {
            username = psz_username;
            password = psz_password;
        }

        bool isInitialized() const
        {
            return http_mgr != nullptr;
        }
        size_t getTotalRead() const
        {
            return totalRead;
        }

        const char * getResponseHeader(const char *key) const
        {
            return vlc_http_msg_get_header(http_res->response, key);
        }

        const ConnectionParams & getFinalLocation() const
        {
            return lastparams;
        }

        size_t getSize() const
        {
            return vlc_http_msg_get_size(http_res->response);
        }

        int create(const ConnectionParams &params,const std::string &ua,
                   const std::string &ref, const BytesRange &range)
        {
            auto *tpl = static_cast<struct restuple *>(
                std::malloc(sizeof(struct restuple)));
            if (unlikely(tpl == nullptr))
                return -1;

            tpl->source = this;
            this->range = range;
            this->lastparams = params;
            if (vlc_http_res_init(&tpl->resource, &this->callbacks, http_mgr,
                                  params.getUrl().c_str(),
                                  ua.empty() ? nullptr : ua.c_str(),
                                  ref.empty() ? nullptr : ref.c_str()))
            {
                std::free(tpl);
                return -1;
            }
            http_res = &tpl->resource;
            return 0;
        }

        RequestStatus connect()
        {
            if (http_res == nullptr)
                return RequestStatus::GenericError;

            if (username.has_value() || password.has_value())
                vlc_http_res_set_login(http_res,
                                       username.has_value() ? username->c_str() : nullptr,
                                       password.has_value() ? password->c_str() : nullptr);

            int status = vlc_http_res_get_status(http_res);
            if (status < 0)
                return RequestStatus::GenericError;

            if (status == 401) /* authentication */
            {
                char *psz_realm = vlc_http_res_get_basic_realm(http_res);
                if (psz_realm)
                {
                    struct vlc_credential crd;
                    struct vlc_url_t crd_url;
                    vlc_credential_init(&crd, &crd_url);
                    vlc_UrlParse(&crd_url, lastparams.getUrl().c_str());

                    crd.psz_authtype = "Basic";
                    crd.psz_realm = psz_realm;
                    if (vlc_credential_get(&crd, p_object, NULL, NULL,
                                           _("HTTP authentication"),
                                           _("Please enter a valid login name and a "
                                             "password for realm %s."), psz_realm) == 0)
                    {
                        setCredentials(crd.psz_username, crd.psz_password);
                        if(!abortandlogin())
                            status = vlc_http_res_get_status(http_res);
                    }

                    if (status > 0 && status < 400 && crd.psz_realm &&
                        crd.i_get_order > decltype(crd.i_get_order)::GET_FROM_MEMORY_KEYSTORE)
                    {
                        /* Force caching into memory keystore */
                        crd.b_from_keystore = false;
                        crd.b_store = false;
                        vlc_credential_store(&crd, p_object);
                    }

                    vlc_credential_clean(&crd);
                    vlc_UrlClean(&crd_url);
                    free(psz_realm);
                }
            }

            if (status == 401)
                return RequestStatus::Unauthorized;

            if (status >= 400)
                return RequestStatus::GenericError;

            char *psz_redir = vlc_http_res_get_redirect(http_res);
            if (psz_redir)
            {
                ConnectionParams loc = ConnectionParams(psz_redir);
                free(psz_redir);
                if(loc.getScheme().empty())
                    lastparams.setPath(loc.getPath());
                else
                    lastparams = loc;
                return RequestStatus::Redirection;
            }

            return RequestStatus::Success;
        }

        int abortandlogin()
        {
            if(http_res == nullptr)
                return -1;

            free(http_res->username);
            http_res->username = username.has_value() ? strdup(username->c_str()) : nullptr;
            free(http_res->password);
            http_res->password = password.has_value() ? strdup(password->c_str()) : nullptr;

            struct vlc_http_msg *resp = vlc_http_res_open(http_res, &http_res[1]);
            if (resp == nullptr)
                return -1;

            if (http_res->response != nullptr)
                vlc_http_msg_destroy(http_res->response);

            http_res->response = resp;
            return 0;
        }
};

const struct vlc_http_resource_cbs LibVLCHTTPSource::callbacks =
{
    LibVLCHTTPSource::formatrequest_handler,
    LibVLCHTTPSource::validateresponse_handler,
};

LibVLCHTTPConnection::LibVLCHTTPConnection(vlc_object_t *p_object_, AuthStorage *auth)
    : AbstractConnection( p_object_ )
{
    source = new adaptive::http::LibVLCHTTPSource(p_object_, auth->getJar());
    sourceStream = new ChunksSourceStream(p_object, source);
    stream = nullptr;
    char *psz_useragent = var_InheritString(p_object_, "http-user-agent");
    if(psz_useragent)
    {
        useragent = std::string(psz_useragent);
        free(psz_useragent);
    }
    char *psz_referer = var_InheritString(p_object_, "http-referrer");
    if(psz_referer)
    {
        referer = std::string(psz_referer);
        free(psz_referer);
    }
}

LibVLCHTTPConnection::~LibVLCHTTPConnection()
{
    reset();
    delete sourceStream;
    delete source;
}

void LibVLCHTTPConnection::reset()
{
    source->reset();
    sourceStream->Reset();
    if(stream)
    {
        vlc_stream_Delete(stream);
        stream = nullptr;
    }
    bytesRange = BytesRange();
    contentType = std::string();
    bytesRead = 0;
    contentLength = 0;
}

bool LibVLCHTTPConnection::canReuse(const ConnectionParams &params_) const
{
    if(!available)
        return false;
    return (params.getHostname() == params_.getHostname() &&
            params.getScheme() == params_.getScheme() &&
            params.getPort() == params_.getPort());
}

RequestStatus LibVLCHTTPConnection::request(const std::string &path,
                                            const BytesRange &range)
{
    if(!source->isInitialized())
        return RequestStatus::GenericError;

    reset();

    /* Set new path for this query */
    params.setPath(path);

    if(range.isValid())
        msg_Dbg(p_object, "Retrieving %s @%zu-%zu", params.getUrl().c_str(),
                           range.getStartByte(), range.getEndByte());
    else
        msg_Dbg(p_object, "Retrieving %s", params.getUrl().c_str());

    if(source->create(params, useragent,referer, range))
        return RequestStatus::GenericError;

    /* Set credentials from URL. Deprecated warning will follow */
    struct vlc_credential crd;
    struct vlc_url_t crd_url;
    vlc_UrlParse(&crd_url, params.getUrl().c_str());
    vlc_credential_init(&crd, &crd_url);
    int ret = vlc_credential_get(&crd, p_object, NULL, NULL, NULL, NULL);
    if (ret == 0)
        source->setCredentials(crd.psz_username, crd.psz_password);
    vlc_credential_clean(&crd);
    vlc_UrlClean(&crd_url);
    if (ret == -EINTR)
        return RequestStatus::GenericError;

    RequestStatus status = source->connect();
    if (status != RequestStatus::Success)
    {
        if (status == RequestStatus::Redirection)
            locationparams = source->getFinalLocation();
        return status;
    }

    sourceStream->Reset();
    stream = sourceStream->makeStream();
    if(stream == nullptr)
        return RequestStatus::GenericError;

    contentLength = source->getSize();

    const char *s = source->getResponseHeader("Content-Type");
    if(s)
        contentType = std::string(s);

    s = source->getResponseHeader("Content-Encoding");
    if(s && stream && (strstr(s, "deflate") || strstr(s, "gzip")))
    {
        stream_t *decomp = vlc_stream_FilterNew(stream, "inflate");
        if(decomp)
        {
            stream = decomp;
            contentLength = 0;
        }
    }

    return RequestStatus::Success;
}

ssize_t LibVLCHTTPConnection::read(void *p_buffer, size_t len)
{
    ssize_t read = vlc_stream_Read(stream, p_buffer, len);
    bytesRead = source->getTotalRead();
    return read;
}

void LibVLCHTTPConnection::setUsed( bool b )
{
    available = !b;
    if(available)
       reset();
}

StreamUrlConnection::StreamUrlConnection(vlc_object_t *p_object)
    : AbstractConnection(p_object)
{
    p_streamurl = nullptr;
    bytesRead = 0;
    contentLength = 0;
}

StreamUrlConnection::~StreamUrlConnection()
{
    reset();
}

void StreamUrlConnection::reset()
{
    if(p_streamurl)
        vlc_stream_Delete(p_streamurl);
    p_streamurl = nullptr;
    bytesRead = 0;
    contentLength = 0;
    contentType = std::string();
    bytesRange = BytesRange();
}

bool StreamUrlConnection::canReuse(const ConnectionParams &params_) const
{
    if( !available || !params_.usesAccess() )
        return false;
    return (params.getHostname() == params_.getHostname() &&
            params.getScheme() == params_.getScheme() &&
            params.getPort() == params_.getPort());
}

RequestStatus StreamUrlConnection::request(const std::string &path,
                                           const BytesRange &range)
{
    reset();

    /* Set new path for this query */
    params.setPath(path);

    msg_Dbg(p_object, "Retrieving %s @%zu", params.getUrl().c_str(),
                      range.isValid() ? range.getStartByte() : 0);

    p_streamurl = vlc_stream_NewURL(p_object, params.getUrl().c_str());
    if(!p_streamurl)
        return RequestStatus::GenericError;

    char *psz_type = stream_ContentType(p_streamurl);
    if(psz_type)
    {
        contentType = std::string(psz_type);
        free(psz_type);
    }

    stream_t *p_chain = vlc_stream_FilterNew( p_streamurl, "inflate" );
    if( p_chain )
        p_streamurl = p_chain;

    if(range.isValid() && range.getEndByte() > 0)
    {
        if(vlc_stream_Seek(p_streamurl, range.getStartByte()) != VLC_SUCCESS)
        {
            vlc_stream_Delete(p_streamurl);
            return RequestStatus::GenericError;
        }
        bytesRange = range;
        contentLength = range.getEndByte() - range.getStartByte() + 1;
    }

    uint64_t i_size;
    if(vlc_stream_GetSize(p_streamurl, &i_size) == VLC_SUCCESS)
    {
        if(!range.isValid() || contentLength > (size_t) i_size)
            contentLength = (size_t) i_size;
    }
    return RequestStatus::Success;
}

ssize_t StreamUrlConnection::read(void *p_buffer, size_t len)
{
    if( !p_streamurl )
        return VLC_EGENERIC;

    if(len == 0)
        return VLC_SUCCESS;

    const size_t toRead = (contentLength) ? contentLength - bytesRead : len;
    if (toRead == 0)
        return VLC_SUCCESS;

    if(len > toRead)
        len = toRead;

    ssize_t ret = vlc_stream_Read(p_streamurl, p_buffer, len);
    if(ret >= 0)
        bytesRead += ret;

    if(ret < 0 || (size_t)ret < len || /* set EOF */
       contentLength == bytesRead )
    {
        reset();
        return ret;
    }

    return ret;
}

void StreamUrlConnection::setUsed( bool b )
{
    available = !b;
    if(available && contentLength == bytesRead)
       reset();
}

LibVLCHTTPConnectionFactory::LibVLCHTTPConnectionFactory( AuthStorage *auth )
    : AbstractConnectionFactory()
{
    authStorage = auth;
}

AbstractConnection * LibVLCHTTPConnectionFactory::createConnection(vlc_object_t *p_object,
                                                                  const ConnectionParams &params)
{
    if((params.getScheme() != "http" && params.getScheme() != "https") ||
       params.getHostname().empty())
        return nullptr;
    return new LibVLCHTTPConnection(p_object, authStorage);
}

StreamUrlConnectionFactory::StreamUrlConnectionFactory()
    : AbstractConnectionFactory()
{

}

AbstractConnection * StreamUrlConnectionFactory::createConnection(vlc_object_t *p_object,
                                                                  const ConnectionParams &)
{
    return new (std::nothrow) StreamUrlConnection(p_object);
}
