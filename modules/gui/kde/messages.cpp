#include "messages.h"
#include <qtextview.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qvbox.h>

KMessagesWindow::KMessagesWindow( intf_thread_t * p_intf,  msg_subscription_t *p_msg ) :
    KDialogBase( Plain, _( "Messages" ), Ok, Ok, 0, 0, false)
{
//    clearWFlags(~0);
//    setWFlags(WType_TopLevel);
    setSizeGripEnabled(true);
    this->p_intf = p_intf;
    this->p_msg = p_msg;
    QFrame *page = plainPage();
    QVBoxLayout *toplayout = new QVBoxLayout( page);
//    QScrollView *sv = new QScrollView(page);
//    sv->setResizePolicy(QScrollView::AutoOneFit);
//    sv->setFrameStyle(QScrollView::NoFrame);
//    toplayout->addWidget(sv);
//    QVBox *category_table = new QVBox(sv->viewport());
//    sv->addChild(category_table);
//    toplayout->addStretch(10);
    QVBox *category_table = new QVBox(page);
    toplayout->addWidget(category_table);
    toplayout->setResizeMode(QLayout::FreeResize);
    category_table->setSpacing(spacingHint());
    resize(300,400);
    new QLabel( _("Messages:"), category_table );
    text = new QTextView( category_table );
//    clearWFlags(WStyle_DialogBorder|WStyle_NoBorder);
//    setWFlags(WStyle_NormalBorder|WStyle_Customize);
//    connect(this, SIGNAL(okClicked()), this, SLOT(accept()));
}

KMessagesWindow::~KMessagesWindow()
{
    ;
}

void KMessagesWindow::update()
{
    int i_stop, i_start;
    /* Update the log window */
    vlc_mutex_lock( p_msg->p_lock );
    i_stop = *p_msg->pi_stop;
    vlc_mutex_unlock( p_msg->p_lock );

    if( p_msg->i_start != i_stop )
    {
//         static GdkColor white  = { 0, 0xffff, 0xffff, 0xffff };
//         static GdkColor gray   = { 0, 0xaaaa, 0xaaaa, 0xaaaa };
//         static GdkColor yellow = { 0, 0xffff, 0xffff, 0x6666 };
//         static GdkColor red    = { 0, 0xffff, 0x6666, 0x6666 };
        
        static const char * ppsz_type[4] = { ": ", " error: ", " warning: ",
                                             " debug: " };
//        static GdkColor *   pp_color[4] = { &white, &red, &yellow, &gray };

        for( i_start = p_msg->i_start;
             i_start != i_stop;
             i_start = (i_start+1) % VLC_MSG_QSIZE )
        {
            text->append( QString(p_msg->p_msg[i_start].psz_module) +
                          ppsz_type[p_msg->p_msg[i_start].i_type] +
                          p_msg->p_msg[i_start].psz_msg + "<br>");
            
//             /* Append all messages to log window */
//             gtk_text_insert( p_intf->p_sys->p_messages_text, NULL, &gray,
//              NULL, p_msg->p_msg[i_start].psz_module, -1 );

//             gtk_text_insert( p_intf->p_sys->p_messages_text, NULL, &gray,
//                 NULL, ppsz_type[p_msg->p_msg[i_start].i_type],
//                 -1 );

//             gtk_text_insert( p_intf->p_sys->p_messages_text, NULL,
//                 pp_color[p_msg->p_msg[i_start].i_type], NULL,
//                 p_msg->p_msg[i_start].psz_msg, -1 );

//             gtk_text_insert( p_intf->p_sys->p_messages_text, NULL, &gray,
//                 NULL, "\n", -1 );
        }

        vlc_mutex_lock( p_msg->p_lock );
        p_msg->i_start = i_start;
        vlc_mutex_unlock( p_msg->p_lock );
//        gtk_text_set_point( p_intf->p_sys->p_messages_text,
//                    gtk_text_get_length( p_intf->p_sys->p_messages_text ) );

    }
}
