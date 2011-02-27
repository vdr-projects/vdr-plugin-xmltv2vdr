/*
 * setup.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __setup_h
#define __setup_h

#include <vdr/menuitems.h>
#include "xmltv2vdr.h"

class cMyMenuEditBitItem : public cMenuEditBoolItem
{
protected:
    uint *value;
    uint mask;
    int bit;
    virtual void Set();
public:
    cMyMenuEditBitItem(const char *Name, uint *Value, uint Mask, const char *FalseString=NULL, const char *TrueString=NULL);
};

class cMenuSetupXmltv2vdr : public cMenuSetupPage
{
protected:
    virtual void Store(void);
private:
    cStringList channels;
    cPluginXmltv2vdr *baseplugin;
    int mappingBegin,mappingEnd;
    int sourcesBegin,sourcesEnd;
    int mappingEntry;
    cOsdItem *newtitle (const char *s);
    eOSState edit(void);
    void generatesumchannellist();
    int exectime;
    int wakeup;
    int upstart;
public:
    void Output(void);
    cMenuSetupXmltv2vdr(cPluginXmltv2vdr *Plugin);
    virtual eOSState ProcessKey(eKeys Key);
    cStringList *ChannelList()
    {
        return &channels;
    }
};

class cMenuSetupXmltv2vdrTextMap : public cMenuSetupPage
{
protected:
    virtual void Store(void);
private:
    cPluginXmltv2vdr *baseplugin;
    char country[255];
    char date[255];
    char originaltitle[255];
    char director[255];
    char actor[255];
    char writer[255];
    char adapter[255];
    char producer[255];
    char composer[255];
    char editor[255];
    char presenter[255];
    char commentator[255];
    char guest[255];
    char review[255];
    cOsdItem *newtitle (const char *s);
public:
    cMenuSetupXmltv2vdrTextMap(cPluginXmltv2vdr *Plugin);
};

class cMenuSetupXmltv2vdrChannelSource : public cMenuSetupPage
{
protected:
    virtual void Store(void);
private:
    cMenuSetupXmltv2vdr *menu;
    cPluginXmltv2vdr *baseplugin;
    cEPGSource *epgsrc;
    int *sel;
    int days;
    cOsdItem *newtitle (const char *s);
public:
    cMenuSetupXmltv2vdrChannelSource(cPluginXmltv2vdr *Plugin, cMenuSetupXmltv2vdr *Menu, int Index);
    ~cMenuSetupXmltv2vdrChannelSource();
};

class cMenuSetupXmltv2vdrChannelMap : public cMenuSetupPage
{
protected:
    virtual void Store(void);
private:
    cPluginXmltv2vdr *baseplugin;
    cMenuSetupXmltv2vdr *menu;
    cEPGMapping *map;
    bool hasmaps;
    uint flags;
    int days;
    int daysmax;
    void output(void);
    cString title;
    const char *channel;
    int getdaysmax();
    cOsdItem *newtitle (const char *s);
    cOsdItem *option(const char *s, bool yesno);
    void epgmappingreplace(cEPGMapping *newmapping);
    int c1,c2,c3,c4,cm;
public:
    cMenuSetupXmltv2vdrChannelMap(cPluginXmltv2vdr *Plugin, cMenuSetupXmltv2vdr *Menu, int Index);
    ~cMenuSetupXmltv2vdrChannelMap();
    void AddChannel2Map(int ChannelNumber);
    bool EPGMappingExists(tChannelID ChannelID);
    virtual eOSState ProcessKey(eKeys Key);
};

class cMenuSetupXmltv2vdrChannelsVDR : public cOsdMenu
{
private:
    cPluginXmltv2vdr *baseplugin;
    cMenuSetupXmltv2vdrChannelMap *map;
    bool epgmappingexists(tChannelID channelid, const char *channel2ignore);
public:
    cMenuSetupXmltv2vdrChannelsVDR(cPluginXmltv2vdr *Plugin, cMenuSetupXmltv2vdrChannelMap *Map,
                                   const char *Channel, cString Title);
    virtual eOSState ProcessKey(eKeys Key);
};

#endif
