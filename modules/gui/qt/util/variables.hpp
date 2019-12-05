/*****************************************************************************
 * variables.hpp : VLC variable class
 ****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef QVLC_VARIABLES_H_
#define QVLC_VARIABLES_H_ 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_variables.h>
#include <QObject>

#include "qt.hpp"
#include "varcommon_p.hpp"

/*
 * type traits to convert VLC_VAR to C++types
 */
template<typename T>
struct VLCVarTypeTraits {
};

template<>
struct VLCVarTypeTraits<QString>
{
    static const int var_type = VLC_VAR_STRING;
    inline static QString fromValue(const vlc_value_t& value) {
        return value.psz_string;
    }
    inline static vlc_value_t toValue(const QString& value) {
        vlc_value_t ret;
        ret.psz_string = strdup(qtu(value));
        return ret;
    }
    inline static void releaseValue(vlc_value_t value) {
        free(value.psz_string);
    }
};

template<>
struct VLCVarTypeTraits<bool>
{
    static const int var_type = VLC_VAR_BOOL;
    inline static bool fromValue(const vlc_value_t& value) {
        return value.b_bool;
    }
    inline static vlc_value_t toValue(bool value) {
        vlc_value_t ret;
        ret.b_bool = value;
        return ret;
    }
    inline static void releaseValue(vlc_value_t) {}
};

template<>
struct VLCVarTypeTraits<int64_t>
{
    static const int var_type = VLC_VAR_INTEGER;
    inline static int64_t fromValue(const vlc_value_t& value) {
        return value.i_int;
    }
    inline static vlc_value_t toValue(int64_t value) {
        vlc_value_t ret;
        ret.i_int = value;
        return ret;
    }
    inline static void releaseValue(vlc_value_t) {}
};

template<>
struct VLCVarTypeTraits<float>
{
    static const int var_type = VLC_VAR_FLOAT;
    inline static float fromValue(const vlc_value_t& value) {
        return value.f_float;
    }
    inline static vlc_value_t toValue(float value) {
        vlc_value_t ret;
        ret.f_float = value;
        return ret;
    }
    inline static void releaseValue(vlc_value_t) {}
};

//Generic observer
template<typename Derived, typename BaseType>
class QVLCVariable : public QObject
{
    typedef QVLCVariable<Derived, BaseType> SelfType;

    struct QVLCVariableCRef  {
        SelfType* self;
    };
    static_assert( std::is_pod<QVLCVariableCRef>::value,  "QVLCVariableCRef must be POD");

public:
    template<typename T>
    QVLCVariable(T* object, QString property, QObject* parent)
        : QObject(parent)
        , m_object(new VLCObjectHolderImpl<T>(nullptr))
        , m_property(property)
    {
        cref.self = this;
        /* This parameter is later used by derivative classes, and only helps
         * the type inference here. */
        VLC_UNUSED(object);
    }

    virtual ~QVLCVariable()
    {
        assert(m_object->get() == nullptr);
    }

    ///change the object beeing observed
    template<typename T>
    void resetObject( T* object )
    {
        clearObject();
        if (object)
        {
            m_object->reset( object, true );
            int type = var_Type(object, qtu(m_property));
            if (type == 0) //variable not found
            {
                msg_Warn(m_object->get(), "variable %s not found in object", qtu(m_property));
                m_object->clear();
                return;
            }
            assert((type & VLC_VAR_CLASS) == VLCVarTypeTraits<BaseType>::var_type);
            vlc_value_t currentvalue;
            if (var_Get(m_object->get(), qtu(m_property), &currentvalue) == VLC_SUCCESS)
            {
                Derived* derived = static_cast<Derived*>(this);
                m_value = VLCVarTypeTraits<BaseType>::fromValue(currentvalue);
                emit derived->valueChanged( m_value );
            }

            var_Create(m_object->get(), qtu(m_property), VLCVarTypeTraits<BaseType>::var_type);
            var_AddCallback(m_object->get(), qtu(m_property), value_modified, &cref);
        }
    }

    void clearObject()
    {
        if (m_object->get())
        {
            var_DelCallback( m_object->get(), qtu(m_property), value_modified, &cref );
            var_Destroy(m_object->get(), qtu(m_property));
            m_object->clear();
        }
    }

    BaseType getValue() const
    {
        if (!m_object->get())
            return BaseType{};
        return m_value;
    }

protected:
    //called by setValue in child classes
    void setValueInternal(BaseType value)
    {
        if (! m_object->get())
            return;
        vlc_value_t vlcvalue = VLCVarTypeTraits<BaseType>::toValue( value );
        var_Set(m_object->get(), qtu(m_property), vlcvalue);
        VLCVarTypeTraits<BaseType>::releaseValue(vlcvalue);
    }

    //executed on UI thread
    virtual void onValueChangedInternal(vlc_object_t* object, BaseType value)
    {
        if (m_object->get() != object)
            return;
        if (m_value != value) {
            m_value = value;
            Derived* derived = static_cast<Derived*>(this);
            emit derived->valueChanged( m_value );
        }
    }


private:
    //executed on variable thread, this forwards the callback to the UI thread
    static int value_modified( vlc_object_t * object, char const *, vlc_value_t, vlc_value_t newValue, void * data)
    {
        QVLCVariableCRef* cref = static_cast<QVLCVariableCRef*>(data);
        Derived* derived = static_cast<Derived*>(cref->self);
        emit derived->onValueChangedInternal( object, VLCVarTypeTraits<BaseType>::fromValue( newValue ) );
        return VLC_SUCCESS;
    }

    std::unique_ptr<VLCObjectHolder> m_object;
    QString m_property;
    BaseType m_value;
    QVLCVariableCRef cref;
};

//specialisation

class QVLCBool : public QVLCVariable<QVLCBool, bool>
{
    Q_OBJECT
public:
    Q_PROPERTY(bool value READ getValue WRITE setValue NOTIFY valueChanged)

    template<typename T>
    QVLCBool(T* object, QString property, QObject* parent = nullptr)
        : QVLCVariable<QVLCBool, bool>(object, property, parent)
    {
        resetObject<T>(object);
        connect(this, &QVLCBool::valueChangedInternal, this, &QVLCBool::onValueChangedInternal, Qt::QueuedConnection);
    }

    ~QVLCBool() {
        clearObject();
    }

public slots:
    void setValue(bool value);

signals:
    void valueChanged( bool );
    void valueChangedInternal(vlc_object_t *, bool );
};

class QVLCString : public QVLCVariable<QVLCString, QString>
{
    Q_OBJECT
public:
    Q_PROPERTY(QString value READ getValue WRITE setValue NOTIFY valueChanged)

    template<typename T>
    QVLCString(T* object, QString property, QObject* parent = nullptr)
        : QVLCVariable<QVLCString, QString>(object, property, parent)
    {
        resetObject<T>(object);
        connect(this, &QVLCString::valueChangedInternal, this, &QVLCString::onValueChangedInternal, Qt::QueuedConnection);
    }

    ~QVLCString() {
        clearObject();
    }

public slots:
    void setValue(QString value);

signals:
    void valueChanged( QString );
    void valueChangedInternal( vlc_object_t *, QString );
};

class QVLCFloat : public QVLCVariable<QVLCFloat, float>
{
    Q_OBJECT
public:
    Q_PROPERTY(float value READ getValue WRITE setValue NOTIFY valueChanged)

    template<typename T>
    QVLCFloat(T* object, QString property, QObject* parent = nullptr)
        : QVLCVariable<QVLCFloat, float>(object, property, parent)
    {
        resetObject<T>(object);
        connect(this, &QVLCFloat::valueChangedInternal, this, &QVLCFloat::onValueChangedInternal, Qt::QueuedConnection);
    }

    ~QVLCFloat() {
        clearObject();
    }

public slots:
    void setValue(float value);

signals:
    void valueChanged( float );
    void valueChangedInternal( vlc_object_t *, float );
};


class QVLCInteger : public QVLCVariable<QVLCInteger, int64_t>
{
    Q_OBJECT
public:
    Q_PROPERTY(int64_t value READ getValue WRITE setValue NOTIFY valueChanged)

    template<typename T>
    QVLCInteger(T* object, QString property, QObject* parent = nullptr)
        : QVLCVariable<QVLCInteger, int64_t>(object, property, parent)
    {
        resetObject<T>(object);
        connect(this, &QVLCInteger::valueChangedInternal, this, &QVLCInteger::onValueChangedInternal, Qt::QueuedConnection);
    }

    ~QVLCInteger() {
        clearObject();
    }

public slots:
    void setValue(int64_t value);

signals:
    void valueChanged( int64_t );
    void valueChangedInternal( vlc_object_t *,  int64_t );
};

#endif
