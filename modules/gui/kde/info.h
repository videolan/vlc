#include <kdialogbase.h>
#include "common.h"

class KInfoWindow : public KDialogBase
{
    Q_OBJECT
public:
    KInfoWindow( intf_thread_t*,  input_thread_t * );
    ~KInfoWindow();

};
