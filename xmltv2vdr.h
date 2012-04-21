/*
 * xmltv2vdr.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef _XMLTV2VDR_H
#define _XMLTV2VDR_H

#include <sqlite3.h>
#include <vdr/plugin.h>
#include "maps.h"
#include "parse.h"
#include "import.h"
#include "source.h"

#if __GNUC__ > 3
#define UNUSED(v) UNUSED_ ## v __attribute__((unused))
#else
#define UNUSED(x) x
#endif

static const char *VERSION        = "0.1.1pre";
static const char *DESCRIPTION    = trNOOP("Imports xmltv epg into vdr");

int ioprio_set(int which, int who, int ioprio);

#if VDRVERSNUM < 10726 && !EPGHANDLER
class cEpgHandler : public cListObject
{
public:
    cEpgHandler(void) {}
    virtual ~cEpgHandler() {}
    virtual bool IgnoreChannel(const cChannel *UNUSED(Channel))
    {
        return false;
    }
    virtual bool SetShortText(cEvent *UNUSED(Event),const char *UNUSED(ShortText))
    {
        return false;
    }
    virtual bool SetDescription(cEvent *UNUSED(Event),const char *UNUSED(Description))
    {
        return false;
    }
    virtual bool HandleEvent(cEvent *UNUSED(Event))
    {
        return false;
    }
    virtual bool SortSchedule(cSchedule *UNUSED(Schedule))
    {
        return false;
    }
};
#endif

class cEPGSources;
class cImport;
class cPluginXmltv2vdr;

class cEPGHandler : public cEpgHandler
{
private:
    cEPGMappings *maps;
    cEPGSources *sources;
    cImport *import;
    const char *epgfile;
    bool epall;
    sqlite3 *db;
    void closedb(void);
    bool check4proc(cEvent *event, bool &spth);
public:
    cEPGHandler(const char *EpgFile, cEPGSources *Sources,
                cEPGMappings *Maps, cTEXTMappings *Texts);
    void SetEPAll(bool Value)
    {
        epall=Value;
    }
    bool Active()
    {
        return (db!=NULL);
    }
    virtual ~cEPGHandler();
    virtual bool IgnoreChannel(const cChannel *Channel);
    virtual bool SetShortText(cEvent *Event,const char *ShortText);
    virtual bool SetDescription(cEvent *Event,const char *Description);
    virtual bool HandleEvent(cEvent *Event);
    virtual bool SortSchedule(cSchedule *Schedule);
};

class cEPGTimer : public cThread
{
private:
    const char *epgfile;
    cEPGSources *sources;
    cEPGMappings *maps;
    cImport *import;
public:
    cEPGTimer(const char *EpgFile, cEPGSources *Sources, cEPGMappings *Maps,cTEXTMappings *Texts);
    bool StillRunning()
    {
        return Running();
    }
    void Stop()
    {
        Cancel(3);
    }
    virtual void Action();
};

class cHouseKeeping : public cThread
{
private:
    const char *epgfile;
public:
    cHouseKeeping(const char *EPGFile);
    virtual void Action();
};

class cPluginXmltv2vdr : public cPlugin
{
private:
    cEPGHandler *epghandler;
    cEPGTimer *epgtimer;
    cHouseKeeping *housekeeping;
    cEPGExecutor epgexecutor;
    cEPGMappings epgmappings;
    cEPGSources epgsources;
    cTEXTMappings textmappings;
    sqlite3_mutex *dbmutex;
    time_t last_housetime_t;
    time_t last_maintime_t;
    time_t last_epcheck_t;
    char *confdir;
    char *epgfile;
    char *srcorder;
    bool epall;
    bool wakeup;
    bool insetup;
public:
    void SetSetupState(bool Value)
    {
        insetup=Value;
    }
    void SetEPAll(bool Value)
    {
        epall=Value;
        if (epghandler) epghandler->SetEPAll(Value);
    }
    bool EPAll()
    {
        return epall;
    }
    void SetWakeUp(bool Value)
    {
        wakeup=Value;
    }
    bool WakeUp()
    {
        return wakeup;
    }
    void ReadInEPGSources(bool Reload=false)
    {
        epgsources.ReadIn(confdir,epgfile,&epgmappings,&textmappings,srcorder,Reload);
    }
    bool EPGSourceMove(int From, int To);
    int EPGSourceCount()
    {
        if (!epgsources.Count()) return 0;
        return epgsources.Count()-1;
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
    cEPGMapping *EPGMapping(const char *ChannelName)
    {
        return epgmappings.GetMap(ChannelName);
    }
    void EPGMappingAdd(cEPGMapping *Map)
    {
        epgmappings.Add(Map);
    }
    cTEXTMapping *TEXTMapping(const char *Name)
    {
        return textmappings.GetMap(Name);
    }
    void TEXTMappingAdd(cTEXTMapping *TextMap)
    {
        textmappings.Add(TextMap);
    }
    cPluginXmltv2vdr(void);
    virtual ~cPluginXmltv2vdr();
    virtual const char *Version(void)
    {
        return VERSION;
    }
    virtual const char *Description(void)
    {
        return tr(DESCRIPTION);
    }
    virtual const char *CommandLineHelp(void);
    virtual bool ProcessArgs(int argc, char *argv[]);
    virtual bool Initialize(void);
    virtual bool Start(void);
    virtual void Stop(void);
    virtual void Housekeeping(void);
    virtual void MainThreadHook(void);
    virtual cString Active(void);
    virtual time_t WakeupTime(void);
    virtual const char *MainMenuEntry(void);
    virtual cOsdObject *MainMenuAction(void);
    virtual cMenuSetupPage *SetupMenu(void);
    virtual bool SetupParse(const char *Name, const char *Value);
    virtual bool Service(const char *Id, void *Data = NULL);
    virtual const char **SVDRPHelpPages(void);
    virtual cString SVDRPCommand(const char *Command, const char *Option, int &ReplyCode);
};

#endif
