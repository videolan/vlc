#ifndef _PICTURE_VOUT_H
#define _PICTURE_VOUT_H 1

#define PICTURE_VOUT_E_AVAILABLE 0
#define PICTURE_VOUT_E_OCCUPIED 1
struct picture_vout_e_t {
    picture_t *p_picture;
    int i_status;
};
struct picture_vout_t
{
    vlc_mutex_t lock;
    int i_picture_num;
    struct picture_vout_e_t *p_pic;
};

#endif
