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
#ifndef VAR_COMMON_P_HPP
#define VAR_COMMON_P_HPP

#include <vlc_cxx_helpers.hpp>
#include <vlc_vout.h>
#include <vlc_aout.h>
#include <vlc_interface.h>

struct VLCObjectHolder
{
public:
    virtual ~VLCObjectHolder() {}

    virtual vlc_object_t* get() const
    {
        assert(false);
        return nullptr;
    }

    virtual void reset(vout_thread_t* , bool  = true )
    {
        assert(false);
    }
    virtual void reset(audio_output_t*, bool  = true )
    {
        assert(false);
    }
    virtual void reset(vlc_object_t*, bool  = true )
    {
        assert(false);
    }
    virtual void reset(intf_thread_t*, bool  = true )
    {
        assert(false);
    }

    virtual void clear() {}

    operator bool() const {
        return get() != nullptr;
    }
};

template<typename T>
struct VLCObjectHolderImpl : public VLCObjectHolder
{
};

template<>
struct VLCObjectHolderImpl<vout_thread_t> : public VLCObjectHolder
{
public:
    VLCObjectHolderImpl(vout_thread_t* vout)
        : m_object(vout)
    { }

    virtual vlc_object_t* get() const override
    {
        if (!m_object)
            return nullptr;
        return VLC_OBJECT(m_object.get());
    }

    void reset(vout_thread_t* vout, bool hold) override
    {
        m_object.reset(vout, hold);
    }

    void clear() override {
        m_object.reset(NULL, false);
    }

private:
    static void obj_hold( vout_thread_t* obj ) { vout_Hold(obj); }
    static void obj_release( vout_thread_t* obj ) { vout_Release(obj); }
    typedef vlc_shared_data_ptr_type(vout_thread_t, obj_hold, obj_release) SharedPtr;
    SharedPtr m_object;
};

template<>
struct VLCObjectHolderImpl<intf_thread_t> : public VLCObjectHolder
{
public:
    VLCObjectHolderImpl(intf_thread_t* p_intf)
        : m_object(p_intf)
    { }

    virtual vlc_object_t* get() const override
    {
        if (!m_object)
            return nullptr;
        return VLC_OBJECT(m_object);
    }

    void reset(intf_thread_t* p_intf, bool) override
    {
        m_object = p_intf;
    }

    void clear() override {
        m_object = NULL;
    }

private:
    intf_thread_t* m_object;
};

template<>
struct VLCObjectHolderImpl<audio_output_t> : public VLCObjectHolder
{
public:

    VLCObjectHolderImpl(audio_output_t* aout)
        : m_object(aout)
    { }

    virtual vlc_object_t* get() const override
    {
        if (!m_object)
            return nullptr;
        return VLC_OBJECT(m_object.get());
    }

    void reset(audio_output_t* aout, bool hold) override
    {
        m_object.reset(aout, hold);
    }

    void clear() override {
        m_object.reset(NULL, false);
    }

private:
    static void obj_hold( audio_output_t* obj ) { aout_Hold(obj); }
    static void obj_release( audio_output_t* obj ) { aout_Release(obj); }
    typedef vlc_shared_data_ptr_type(audio_output_t, obj_hold, obj_release) SharedPtr;
    SharedPtr m_object;
};

template<>
struct VLCObjectHolderImpl<vlc_object_t> : public VLCObjectHolder
{
public:

    VLCObjectHolderImpl(vlc_object_t* obj)
        : m_object(obj)
    { }

    virtual vlc_object_t* get() const override {
        return m_object;
    }

    void reset(vlc_object_t* obj, bool) override {
        m_object = obj;
    }

    void clear() override {
        m_object = NULL;
    }

private:
    vlc_object_t *m_object;
};

#endif // VAR_COMMON_P_HPP
