#include <kdialogbase.h>
#include <qtextview.h>
#include "common.h"

class KMessagesWindow : public KDialogBase
{
    Q_OBJECT
public:
    KMessagesWindow( intf_thread_t*,  msg_subscription_t * );
    ~KMessagesWindow();

public slots:
    void update();
private:
    intf_thread_t* p_intf;
    QTextView* text;
    msg_subscription_t *p_msg;
    
};
