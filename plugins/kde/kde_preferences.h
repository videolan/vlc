#ifndef _KDE_PREFERENCES_H_
#define _KDE_PREFERENCES_H_
#include <kdialogbase.h>

#include "QConfigItem.h"
class KPreferences : KDialogBase
{
    Q_OBJECT
 public:
    KPreferences( const char *psz_module_name, QWidget *parent,
                  const QString &caption=QString::null);
    ~KPreferences();
    bool isConfigureable(QString module);

 public slots:
    void slotApply();
    void slotOk();
    void slotUser1();
};
#endif
