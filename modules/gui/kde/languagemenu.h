#include <kaction.h>
#include "common.h"
class KLanguageMenuAction : public KRadioAction
{
    Q_OBJECT
public:
    KLanguageMenuAction(intf_thread_t*, const QString&, es_descriptor_t *, QObject *);
    ~KLanguageMenuAction();
signals:
    void toggled( bool, es_descriptor_t *);
public slots:
    void setChecked( bool );
private:
    es_descriptor_t *p_es;
    intf_thread_t    *p_intf;
};
