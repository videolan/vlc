#ifndef _AtmoChannelAssignment_
#define _AtmoChannelAssignment_

#include "AtmoDefs.h"

class CAtmoChannelAssignment
{
protected:
    // name of the mapping (for menus and lists)
    char *m_psz_name;
    // count of channels starting with 0 ... X for which a mapping exists!
    int m_num_channels;
    // array were each destination channel - has an index to the source zone to use
    // or -1 to show black output on this channel
    int *m_mappings;

public:
    CAtmoChannelAssignment(void);
    CAtmoChannelAssignment(CAtmoChannelAssignment &source);
    ~CAtmoChannelAssignment(void);

public:
    // internal used to mark a not modifyable definition
    // with a default assignment!
    ATMO_BOOL system;
    char *getName() { return(m_psz_name); }
    void setName(const char *pszNewName);

    void setSize(int numChannels);
    int getSize() { return m_num_channels; }
    int *getMapArrayClone(int &count);
    int getZoneIndex(int channel);
    void setZoneIndex(int channel, int zone);
};

#endif
