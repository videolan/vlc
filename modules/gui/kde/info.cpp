#include "info.h"
#include "common.h"
#include <qtextview.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qvbox.h>

KInfoWindow::KInfoWindow( intf_thread_t * p_intf,  input_thread_t *p_input ) :
    KDialogBase( Tabbed, _( "Messages" ), Ok, Ok, 0, 0, false)
{
//    clearWFlags(~0);
//    setWFlags(WType_TopLevel);
    setSizeGripEnabled(true);
    vlc_mutex_lock( &p_input->stream.stream_lock );
    input_info_category_t *p_category = p_input->stream.p_info;
    while ( p_category )
    {
        QFrame *page = addPage( QString(p_category->psz_name) );
        QVBoxLayout *toplayout = new QVBoxLayout( page);
        QVBox *category_table = new QVBox(page);
        toplayout->addWidget(category_table);
        toplayout->setResizeMode(QLayout::FreeResize);
        toplayout->addStretch(10);
        category_table->setSpacing(spacingHint());
        input_info_t *p_info = p_category->p_info;
        while ( p_info )
        {
            QHBox *hb = new QHBox( category_table );
            new QLabel( QString(p_info->psz_name) + ":", hb );
            new QLabel( p_info->psz_value, hb );
            p_info = p_info->p_next;
        }
        p_category = p_category->p_next;
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock );
    resize(300,400);
    show();
}

KInfoWindow::~KInfoWindow()
{
    ;
}
