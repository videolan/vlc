#include "QConfigItem.h"
#include <vlc/vlc.h>
QConfigItem::QConfigItem(QObject *parent, QString name, int iType, int i_val) :
    QObject(parent, name)
{
    type = iType;
    iVal = i_val;
}

QConfigItem::QConfigItem(QObject *parent, QString name, int iType, float f_val) :
    QObject(parent, name)
{
    type = iType;
    fVal = f_val;
}

QConfigItem::QConfigItem(QObject *parent, QString name, int iType, QString s_val) :
    QObject(parent, name)
{
    type = iType;
    sVal = s_val;
}

QConfigItem::~QConfigItem()
{
    ;
}

int QConfigItem::getType()
{
    return type;
}

int QConfigItem::iValue()
{
    return iVal;
}

float QConfigItem::fValue()
{
    return fVal;
}

QString QConfigItem::sValue()
{
    return sVal;
}

void QConfigItem::setValue(int val)
{
    iVal = val;
}

void QConfigItem::setValue(float val)
{
    fVal = val;
}

void QConfigItem::setValue(double val)
{
    fVal = (float)val;
}

void QConfigItem::setValue(const QString &val)
{
    sVal = val;
}
