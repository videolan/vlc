#ifndef _KDE_PREFERENCES_H_
#define _KDE_PREFERENCES_H_
#include "kde_common.h"
#include <kdialogbase.h>

#include "QConfigItem.h"
class KPreferences : KDialogBase
{
    Q_OBJECT
 public:
    KPreferences(intf_thread_t *p_intf, const char *psz_module_name,
                 QWidget *parent, const QString &caption=QString::null);
    ~KPreferences();
    bool isConfigureable(QString module);

 public slots:
    void slotApply();
    void slotOk();
    void slotUser1();

 private:
    intf_thread_t *p_intf;
};
#endif
