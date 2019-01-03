#ifndef HMDMODEHELPER_HPP
#define HMDMODEHELPER_HPP

#include "qt.hpp"

#include <QWidget>

struct playlist_t;
class QVBoxLayout;
class QLabel;
class QPushButton;


class HMDModeHelper : public QWidget
{
    Q_OBJECT

public:
    HMDModeHelper(playlist_t *p_playlist);
    ~HMDModeHelper();

private slots:
    void quitHMDMode();

private:
    playlist_t *p_playlist;

    QVBoxLayout *layout;
    QLabel *label1, *label2;
    QPushButton *button;
};

#endif // HMDMODEHELPER_HPP
