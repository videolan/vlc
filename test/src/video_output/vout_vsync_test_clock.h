#include <vlc_common.h>

typedef struct vlc_clock_t vlc_clock_t;

static inline vlc_tick_t
vlc_clock_ConvertToSystem(vlc_clock_t *clock, vlc_tick_t system_now,
                          vlc_tick_t ts, double rate)
{
    return ts;
}