/*
 * xmltv2vdr.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef _XMLTV2VDR_H
#define _XMLTV2VDR_H

#define EPGSOURCES "/var/lib/epgsources"

#include <vdr/plugin.h>
#include "maps.h"
#include "parse.h"

static const char *VERSION        = "0.0.1";
static const char *DESCRIPTION    = trNOOP ( "Imports xmltv epg into vdr" );

class cEPGChannel : public cListObject
{
private:
    bool inuse;
    const char *name;
public:
    cEPGChannel(const char *Name, bool InUse=false);
    ~cEPGChannel();
    virtual int Compare(const cListObject &ListObject) const;
    bool InUse()
    {
        return inuse;
    }
    void SetUsage(bool InUse)
    {
        inuse=InUse;
    }
    const char *Name()
    {
        return name;
    }
};

class cEPGChannels : public cList<cEPGChannel> {};

class cEPGExecute : public cThread
{
private:
    const char *name;
    cParse *parse;
    bool ready2parse;
    bool pipe;
    int daysinadvance;
    int daysmax;
    bool ReadConfig();
    cEPGChannels channels;
public:
    cEPGExecute(const char *Name,cEPGMappings *Maps,cTEXTMappings *Texts);
    ~cEPGExecute();
    cEPGChannels *GetChannelList()
    {
        return &channels;
    }
    int GetDaysMax()
    {
        return daysmax;
    }
    int GetDaysInAdvance()
    {
        return daysinadvance;
    }
    void SetDaysInAdvance(int NewDaysInAdvance)
    {
        daysinadvance=NewDaysInAdvance;
    }
    void SetChannelSelection(int *Selection);
    virtual void Action();
    void Stop()
    {
        Cancel(3);
    }
};

class cEPGSource : public cListObject
{
private:
    const char *name;
    cEPGExecute exec;
public:
    cEPGSource(const char *Name,cEPGMappings *Maps,cTEXTMappings *Texts);
    ~cEPGSource();
    bool Execute();
    void Store(void);
    cEPGChannels *ChannelList()
    {
        return exec.GetChannelList();
    }
    int DaysMax()
    {
        return exec.GetDaysMax();
    }
    int DaysInAdvance()
    {
        return exec.GetDaysInAdvance();
    }
    const char *Name()
    {
        return name;
    }
    void ChangeDaysInAdvance(int NewDaysInAdvance)
    {
        exec.SetDaysInAdvance(NewDaysInAdvance);
    }
    void ChangeChannelSelection(int *Selection)
    {
        exec.SetChannelSelection(Selection);
    }
    bool Active()
    {
        return exec.Active();
    }
};

class cEPGSources : public cList<cEPGSource> {};

class cPluginXmltv2vdr : public cPlugin
{
private:
    cEPGMappings epgmappings;
    cEPGSources epgsources;
    cTEXTMappings textmappings;
    void removeepgsources();
    void removeepgmappings();
    void removetextmappings();
    bool executeepgsources();
    bool epgsourcesactive();
    bool epgsourceexists(const char *name);
    int exectime;
    time_t exectime_t,last_exectime_t;
public:
    int ExecTime()
    {
        return exectime;
    }
    void SetExecTime(int ExecTime);
    int wakeup;
    void ReadInEPGSources(bool Reload=false);
    int EPGSourceCount()
    {
        return epgsources.Count();
    }
    cEPGSource *EPGSource(int Index)
    {
        return epgsources.Get(Index);
    }
    int EPGMappingCount()
    {
        return epgmappings.Count();
    }
    cEPGMapping *EPGMapping(int Index)
    {
        return epgmappings.Get(Index);
    }
    cEPGMapping *EPGMapping(const char *ChannelName);
    void EPGMappingAdd(cEPGMapping *Map)
    {
        epgmappings.Add(Map);
    }
    cTEXTMapping *TEXTMapping(const char *Name);
    void TEXTMappingAdd(cTEXTMapping *TextMap)
    {
        textmappings.Add(TextMap);
    }
    cPluginXmltv2vdr ( void );
    virtual ~cPluginXmltv2vdr();
    virtual const char *Version ( void )
    {
        return VERSION;
    }
    virtual const char *Description ( void )
    {
        return DESCRIPTION;
    }
    virtual const char *CommandLineHelp ( void );
    virtual bool ProcessArgs ( int argc, char *argv[] );
    virtual bool Initialize ( void );
    virtual bool Start ( void );
    virtual void Stop ( void );
    virtual void Housekeeping ( void );
    virtual void MainThreadHook ( void );
    virtual cString Active ( void );
    virtual time_t WakeupTime ( void );
    virtual const char *MainMenuEntry ( void );
    virtual cOsdObject *MainMenuAction ( void );
    virtual cMenuSetupPage *SetupMenu ( void );
    virtual bool SetupParse ( const char *Name, const char *Value );
    virtual bool Service ( const char *Id, void *Data = NULL );
    virtual const char **SVDRPHelpPages ( void );
    virtual cString SVDRPCommand ( const char *Command, const char *Option, int &ReplyCode );
};

#endif
