/*
 * maps.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef _MAPS_H
#define _MAPS_H

#include <vdr/channels.h>

// Usage field definition

// Bit  0- 5  DAYS in advance +1 -> Range 1-32
// Bit  6-23  USE_ flags
// Bit 24-30  OPT_ flags
// Bit 31     always zero

#define USE_NOTHING     0

#define USE_SHORTTEXT   0x1
#define USE_LONGTEXT    0x2
#define USE_COUNTRYDATE 0x4
#define USE_ORIGTITLE   0x8
#define USE_CATEGORY    0x10
#define USE_CREDITS     0x20
#define USE_RATING      0x40
#define USE_REVIEW      0x80
#define USE_VIDEO       0x100
#define USE_AUDIO       0x200

#define OPT_MERGELTEXT  0x10000000
#define OPT_VPS         0x20000000
#define OPT_APPEND      0x40000000

class cTEXTMapping : public cListObject
{
private:
    const char *name;
    const char *value;
public:
    cTEXTMapping(const char *Name, const char *Value);
    ~cTEXTMapping();
    void ChangeValue(const char *Value);
    const char *Name(void)
    {
        return name;
    }
    const char *Value(void)
    {
        return value;
    }
};

class cTEXTMappings : public cList<cTEXTMapping> {};

class cEPGMapping : public cListObject
{
private:
    static int compare(const void *a, const void *b);
    const char *channelname;
    tChannelID *channelids;
    int numchannelids;
    int flags;
    int days;
    void addchannels(const char *channels);
public:
    cEPGMapping(const char *ChannelName, const char *Flags_and_Mappings);
    ~cEPGMapping();
    cEPGMapping(cEPGMapping&copy);
    void ChangeFlags(int NewFlags)
    {
        flags=NewFlags;
    }
    void ChangeDays(int NewDays)
    {
        days=NewDays;
    }
    void ReplaceChannels(int NumChannelIDs, tChannelID *ChannelIDs);
    void AddChannel(int ChannelNumber);
    void RemoveChannel(int ChannelNumber);
    int Flags()
    {
        return flags;
    }
    int Days()
    {
        return days;
    }
    const char *ChannelName()
    {
        return channelname;
    }
    int NumChannelIDs()
    {
        return numchannelids;
    }
    tChannelID *ChannelIDs()
    {
        return channelids;
    }
};

class cEPGMappings : public cList<cEPGMapping> {};

#endif
