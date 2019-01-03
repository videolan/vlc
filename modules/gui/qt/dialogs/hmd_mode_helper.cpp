
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_vout.h>
#include <vlc_input.h>

#include "hmd_mode_helper.hpp"

#include <QVBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QDesktopWidget>
#include <QPushButton>


#include "qt.hpp"
#include "hmd_mode_helper.hpp"


HMDModeHelper::HMDModeHelper(playlist_t *p_playlist)
    : QWidget( nullptr, Qt::CustomizeWindowHint | Qt::WindowTitleHint ),
      p_playlist( p_playlist )
{
    setWindowTitle( qtr( "VLC HMD mode" ) );

    layout = new QVBoxLayout();

    label1 = new QLabel();
    label1->setText( qtr( "VLC HMD mode" ) );
    QFont font = label1->font();
    font.setPointSize(48);
    label1->setFont(font);
    layout->addWidget(label1);

    label2 = new QLabel();
    label2->setText( qtr( "The headset is on." ) );
    layout->addWidget(label2);
    layout->setAlignment(label2, Qt::AlignHCenter);

    button = new QPushButton( qtr( "Return to normal mode" ), this);
    layout->addWidget(button);
    layout->setAlignment(button, Qt::AlignHCenter);

    connect(button, SIGNAL (released()), this, SLOT (quitHMDMode()));

    setLayout(layout);
    layout->setSizeConstraint( QLayout::SetFixedSize );
}


HMDModeHelper::~HMDModeHelper()
{
    delete label1;
    delete label2;
    delete button;
    delete layout;
}


void HMDModeHelper::quitHMDMode()
{
    var_SetBool( p_playlist, "hmd", false );
    input_thread_t *input = playlist_CurrentInput(p_playlist);
    if( input != NULL )
    {
        vout_thread_t *vout = input_GetVout( input );
        vlc_object_release( input );
        if( vout != NULL )
        {
            var_SetBool( vout, "hmd", false );
            vlc_object_release( vout );
        }
    }
}
