#ifndef _KCONFIGITEM_H_
#define _KCONFIGITEM_H_
#include <qobject.h>
#include <qstring.h>
/*
  A class to handle the information for one configuration item. 
*/

class QConfigItem : public QObject
{
    Q_OBJECT
 public:
    QConfigItem(QObject *parent, QString name, int iType, int i_val);
    QConfigItem(QObject *parent, QString name, int iType, float f_val);
    QConfigItem(QObject *parent, QString name, int iType, QString s_val);
    ~QConfigItem();

    int getType();
    float fValue();
    int iValue();
    QString sValue();

 public slots:
    void setValue(int val);
    void setValue(float val);
    void setValue(double val);
    void setValue(const QString &val);
    
 private:
    int iVal, type;
    float fVal;
    QString sVal;
};
#endif
