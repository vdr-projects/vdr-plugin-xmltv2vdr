/*
 * source.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __source_h
#define __source_h

#include <vdr/tools.h>

#include "maps.h"
#include "import.h"
#include "parse.h"

#define EPGSOURCES "/var/lib/epgsources" // NEVER (!) CHANGE THIS

#define EITSOURCE "EIT"

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

class cImport;

class cEPGSource : public cListObject
{
private:
    const char *name;
    const char *confdir;
    const char *epgfile;
    const char *pin;
    int loglen;
    cParse *parse;
    cImport *import;
    bool ready2parse;
    bool usepipe;
    bool needpin;
    bool running;
    bool upstartdone;
    bool disabled;
    int daysinadvance;
    int exec_upstart;
    int exec_weekday;
    int exec_time;
    int daysmax;
    time_t lastexec;
    int lastretcode;
    void add2Log(const char prefix, const char *line);
    bool ReadConfig();
    int ReadOutput(char *&result, size_t &l);
    cEPGChannels channels;
public:
    cEPGSource(const char *Name,const char *ConfDir,const char *EPGFile,
               cEPGMappings *Maps, cTEXTMappings *Texts);
    ~cEPGSource();
    int Execute(cEPGExecutor &myExecutor);
    int Import(cEPGExecutor &myExecutor);
    bool RunItNow();
    time_t NextRunTime();
    void Store(void);
    void ChangeChannelSelection(int *Selection);
    char *Log;
    bool Disabled()
    {
        return disabled;
    }
    void Disable()
    {
        disabled=true;
    }
    void Enable()
    {
        disabled=false;
    }
    cEPGChannels *ChannelList()
    {
        return &channels;
    }
    int LastRetCode()
    {
        return lastretcode;
    }
    int ExecTime()
    {
        return exec_time;
    }
    int ExecWeekDay()
    {
        return exec_weekday;
    }
    int ExecUpStart()
    {
        return exec_upstart;
    }
    int DaysMax()
    {
        return daysmax;
    }
    int DaysInAdvance()
    {
        return daysinadvance;
    }
    bool NeedPin()
    {
        return needpin;
    }
    const char *EPGFile()
    {
        return epgfile;
    }
    const char *Name()
    {
        return name;
    }
    const char *Pin()
    {
        return pin;
    }
    void ChangeExec(int UpStart, int Time, int WeekDay)
    {
        exec_upstart=UpStart;
        exec_time=Time;
        exec_weekday=WeekDay;
    }
    void ChangeDaysInAdvance(int NewDaysInAdvance)
    {
        daysinadvance=NewDaysInAdvance;
    }
    void ChangePin(const char *NewPin)
    {
        if (pin) free((void *) pin);
        pin=strdup(NewPin);
    }
    time_t LastExecution()
    {
        return lastexec;
    }
    void Dlog(const char *format, ...);
    void Elog(const char *format, ...);
    void Ilog(const char *format, ...);
    bool Active()
    {
        return running;
    }
};

class cEPGSources : public cList<cEPGSource>
{
public:
    void ReadIn(const char *ConfDir, const char *EpgFile, cEPGMappings *EPGMappings,
                cTEXTMappings *TextMappings, const char *SourceOrder, bool Reload=false);
    bool RunItNow();
    time_t NextRunTime();
    bool Exists(const char *Name);
    cEPGSource *GetSource(const char *Name);
    int GetSourceIdx(const char *Name);
    void Remove();
};

class cPluginXmltv2vdr;

class cEPGExecutor : public cThread
{
private:
    cEPGSources *sources;
    cPluginXmltv2vdr *baseplugin;
public:
    cEPGExecutor(cPluginXmltv2vdr *Plugin, cEPGSources *Sources);
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

#endif
