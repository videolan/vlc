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
#include "../AbstractSource.hpp"
#include "../plumbing/SourceStream.hpp"

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

class adaptive::http::LibVLCHTTPSource : public adaptive::AbstractSource
{
     friend class LibVLCHTTPConnection;

     public:
        LibVLCHTTPSource(vlc_object_t *p_object, struct vlc_http_cookie_jar_t *jar)
        {
            http_mgr = vlc_http_mgr_create(p_object, jar);
            http_res = nullptr;
            totalRead = 0;
        }
        virtual ~LibVLCHTTPSource()
        {
            if(http_mgr)
                vlc_http_mgr_destroy(http_mgr);
        }
        virtual block_t *readNextBlock() override
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

        static const struct vlc_http_resource_cbs callbacks;
        size_t totalRead;
        struct vlc_http_mgr *http_mgr;
        BytesRange range;

    public:
        struct vlc_http_resource *http_res;
        int create(const char *uri,const std::string &ua,
                   const std::string &ref, const BytesRange &range)
        {
            struct restuple *tpl = new struct restuple;
            tpl->source = this;
            this->range = range;
            if (vlc_http_res_init(&tpl->resource, &this->callbacks, http_mgr, uri,
                                  ua.empty() ? nullptr : ua.c_str(),
                                  ref.empty() ? nullptr : ref.c_str()))
            {
                delete tpl;
                return -1;
            }
            http_res = &tpl->resource;
            return 0;
        }

        int abortandlogin(const char *user, const char *pass)
        {
            if(http_res == nullptr)
                return -1;

            free(http_res->username);
            http_res->username = user ? strdup(user) : nullptr;
            free(http_res->password);
            http_res->password = pass ? strdup(pass) : nullptr;

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
    if(source->http_mgr == nullptr)
        return RequestStatus::GenericError;

    reset();

    /* Set new path for this query */
    params.setPath(path);

    if(range.isValid())
        msg_Dbg(p_object, "Retrieving %s @%zu-%zu", params.getUrl().c_str(),
                           range.getStartByte(), range.getEndByte());
    else
        msg_Dbg(p_object, "Retrieving %s", params.getUrl().c_str());

    if(source->create(params.getUrl().c_str(), useragent,referer, range))
        return RequestStatus::GenericError;

    struct vlc_credential crd;
    struct vlc_url_t crd_url;
    vlc_UrlParse(&crd_url, params.getUrl().c_str());

    vlc_credential_init(&crd, &crd_url);
    int ret = vlc_credential_get(&crd, p_object, NULL, NULL, NULL, NULL);
    if (ret == 0)
    {
        vlc_http_res_set_login(source->http_res,
                               crd.psz_username, crd.psz_password);
    }
    else if (ret == -EINTR)
    {
        vlc_credential_clean(&crd);
        vlc_UrlClean(&crd_url);
        return RequestStatus::GenericError;
    }

    int status = vlc_http_res_get_status(source->http_res);
    if (status < 0)
    {
        vlc_credential_clean(&crd);
        vlc_UrlClean(&crd_url);
        return RequestStatus::GenericError;
    }

    char *psz_realm = nullptr;
    if (status == 401) /* authentication */
    {
        psz_realm = vlc_http_res_get_basic_realm(source->http_res);
        if (psz_realm)
        {
            vlc_credential_init(&crd, &crd_url);
            crd.psz_authtype = "Basic";
            crd.psz_realm = psz_realm;
            if (vlc_credential_get(&crd, p_object, NULL, NULL,
                                   _("HTTP authentication"),
                                   _("Please enter a valid login name and a "
                                   "password for realm %s."), psz_realm) == 0)
            {
                if(source->abortandlogin(crd.psz_username, crd.psz_password))
                {
                    vlc_credential_clean(&crd);
                    vlc_UrlClean(&crd_url);
                    free(psz_realm);
                    return RequestStatus::Unauthorized;
                }
                status = vlc_http_res_get_status(source->http_res);
            }
        }
    }

    if(status > 0 && status < 400 && crd.psz_realm &&
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

    if (status >= 400)
        return RequestStatus::GenericError;

    char *psz_redir = vlc_http_res_get_redirect(source->http_res);
    if(psz_redir)
    {
        ConnectionParams loc = ConnectionParams(psz_redir);
        free(psz_redir);
        if(loc.getScheme().empty())
        {
            locationparams = params;
            locationparams.setPath(loc.getPath());
        }
        else locationparams = loc;
        return RequestStatus::Redirection;
    }

    sourceStream->Reset();
    stream = sourceStream->makeStream();
    if(stream == nullptr)
        return RequestStatus::GenericError;

    contentLength = vlc_http_msg_get_size(source->http_res->response);

    const char *s = vlc_http_msg_get_header(source->http_res->response, "Content-Type");
    if(s)
        contentType = std::string(s);

    s = vlc_http_msg_get_header(source->http_res->response, "Content-Encoding");
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
    bytesRead = source->totalRead;
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

    int64_t i_size = stream_Size(p_streamurl);
    if(i_size > -1)
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
