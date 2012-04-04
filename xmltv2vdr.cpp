/*
 * xmltv2vdr.cpp: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <vdr/plugin.h>
#include <vdr/videodir.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>

#include "setup.h"
#include "xmltv2vdr.h"

// -------------------------------------------------------------

cEPGHandler::cEPGHandler(cPluginXmltv2vdr *Plugin, const char *EpgFile, cEPGSources *Sources,
                         cEPGMappings *Maps, cTEXTMappings *Texts)
{
    baseplugin=Plugin;
    epall=false;
    epgfile=EpgFile;
    maps=Maps;
    sources=Sources;
    import = new cImport(NULL,Maps,Texts);
}

cEPGHandler::~cEPGHandler()
{
    if (import) delete import;
}

bool cEPGHandler::IgnoreChannel(const cChannel* Channel)
{
    if (!maps) return false;
    if (!Channel) return false;
    return maps->IgnoreChannel(Channel);
}

bool cEPGHandler::SetContents(cEvent* UNUSED(Event), uchar* UNUSED(Contents))
{
    return false;
}

bool cEPGHandler::SetDescription(cEvent* Event, const char* Description)
{
    if (!Event) return false;
    if (!maps) return false;
    if (!import) return false;
    if (!baseplugin) return false;

    bool special_epall_timer_handling=false;
    if (!maps->ProcessChannel(Event->ChannelID()))
    {
        if (!epall) return false;
        if (!Event->HasTimer()) return false;
        if (!Event->ShortText()) return false;
        special_epall_timer_handling=true;
    }

    if (!baseplugin->IsIdle())
    {
        if (import->WasChanged(Event)) return true;
        return false;
    }

    int Flags=0;
    const char *ChannelID;

    if (special_epall_timer_handling)
    {
        cChannel *chan=Channels.GetByChannelID(Event->ChannelID());
        if (!chan) return false;
        Flags=USE_SEASON;
        ChannelID=chan->Name();
    }
    else
    {
        cEPGMapping *map=maps->GetMap(Event->ChannelID());
        if (!map) return false;
        Flags=map->Flags();
        ChannelID=map->ChannelName();
    }

    cXMLTVEvent *xevent=import->SearchXMLTVEvent(epgfile,ChannelID,Event);
    if (!xevent)
    {
        if (!epall) return false;
        xevent=import->AddXMLTVEvent(epgfile,ChannelID,Event,Description);
        if (!xevent) return false;
    }

    bool update=false;
    if (!xevent->EITEventID()) update=true;
    if (!xevent->EITDescription() && Description) update=true;
    if (xevent->EITDescription() && Description &&
            strcasecmp(xevent->EITDescription(),Description)) update=true;

    if (update)
    {
        import->UpdateXMLTVEvent(epgfile,NULL,xevent->Source(),
                                 xevent->EventID(),Event->EventID(),Description);
    }

    bool ret=import->PutEvent(sources->GetSource(xevent->Source()),NULL,
                              (cSchedule *) Event->Schedule(),
                              Event,xevent,Flags,IMPORT_DESCRIPTION);
    delete xevent;
    if (!ret)
    {
        dsyslog("xmltv2vdr: failed to put event description!");
    }
    return ret;
}

bool cEPGHandler::SetParentalRating(cEvent* UNUSED(Event), int UNUSED(ParentalRating))
{
    return false;
}

bool cEPGHandler::SetShortText(cEvent* Event, const char* UNUSED(ShortText))
{
    if (!Event) return false;
    if (!maps) return false;
    if (!import) return false;

    if (!maps->ProcessChannel(Event->ChannelID())) return false;

    if (!baseplugin->IsIdle())
    {
        if (import->WasChanged(Event)) return true;
        return false;
    }

    cEPGMapping *map=maps->GetMap(Event->ChannelID());
    if (!map) return false;

    cXMLTVEvent *xevent=import->SearchXMLTVEvent(epgfile,map->ChannelName(),Event);
    if (!xevent) return false;

    if (!xevent->EITEventID()) import->UpdateXMLTVEvent(epgfile,NULL,xevent->Source(),
                xevent->EventID(),Event->EventID());

    bool ret=import->PutEvent(sources->GetSource(xevent->Source()),NULL,
                              (cSchedule *) Event->Schedule(),Event,xevent,
                              map->Flags(),IMPORT_SHORTTEXT);
    delete xevent;
    if (!ret)
    {
        dsyslog("xmltv2vdr: failed to put event shorttext!");
    }
    return ret;
}

// -------------------------------------------------------------

cEPGTimer::cEPGTimer(const char *EpgFile, cEPGSources *Sources, cEPGMappings *Maps,
                     cTEXTMappings *Texts) : cThread("xmltv2vdr timer")
{
    epgfile=EpgFile;
    sources=Sources;
    maps=Maps;
    import = new cImport(NULL,Maps,Texts);
}

void cEPGTimer::Action()
{
    struct stat statbuf;
    if (stat(epgfile,&statbuf)==-1) return; // no database? -> exit immediately
    if (!statbuf.st_size) return; // no database? -> exit immediately

    cSchedulesLock *schedulesLock = new cSchedulesLock(true,2000); // wait up to 2 secs for lock!
    const cSchedules *schedules = cSchedules::Schedules(*schedulesLock);
    if (!schedules)
    {
        delete schedulesLock;
        return;
    }

    for (cTimer *Timer = Timers.First(); Timer; Timer = Timers.Next(Timer))
    {
        const cEvent *event=Timer->Event();
        if (!event) continue;
        if (!event->ShortText()) continue; // no short text -> no episode
        if (maps->ProcessChannel(event->ChannelID())) continue; // already processed by xmltv2vdr

        cChannel *chan=Channels.GetByChannelID(event->ChannelID());
        if (!chan) continue;
        const char *ChannelID=chan->Name();

        cXMLTVEvent *xevent=import->SearchXMLTVEvent(epgfile,ChannelID,event);
        if (!xevent)
        {
            xevent=import->AddXMLTVEvent(epgfile,ChannelID,event,event->Description());
            if (!xevent) continue;
        }

        cSchedule* schedule = (cSchedule *) schedules->GetSchedule(chan,false);
        if (schedule)
        {
            import->PutEvent(sources->GetSource(EITSOURCE),NULL,schedule,
                             (cEvent *) event,xevent,USE_SEASON,IMPORT_DESCRIPTION);
        }
        delete xevent;
    }
    delete schedulesLock;
    cSchedules::Cleanup(true);
}

// -------------------------------------------------------------

cPluginXmltv2vdr::cPluginXmltv2vdr(void) : epgexecutor(this, &epgsources)
{
    // Initialize any member variables here.
    // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
    // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
    confdir=NULL;
    epgfile=NULL;
    srcorder=NULL;
    epghandler=NULL;
    epgtimer=NULL;
    last_housetime_t=0;
    last_maintime_t=0;
    last_epcheck_t=0;
    wakeup=0;
    SetEPAll(false);
    TEXTMappingAdd(new cTEXTMapping("country",tr("country")));
    TEXTMappingAdd(new cTEXTMapping("year",tr("year")));
    TEXTMappingAdd(new cTEXTMapping("originaltitle",tr("originaltitle")));
    TEXTMappingAdd(new cTEXTMapping("category",tr("category")));
    TEXTMappingAdd(new cTEXTMapping("actor",tr("actor")));
    TEXTMappingAdd(new cTEXTMapping("adapter",tr("adapter")));
    TEXTMappingAdd(new cTEXTMapping("commentator",tr("commentator")));
    TEXTMappingAdd(new cTEXTMapping("composer",tr("composer")));
    TEXTMappingAdd(new cTEXTMapping("director",tr("director")));
    TEXTMappingAdd(new cTEXTMapping("editor",tr("editor")));
    TEXTMappingAdd(new cTEXTMapping("guest",tr("guest")));
    TEXTMappingAdd(new cTEXTMapping("presenter",tr("presenter")));
    TEXTMappingAdd(new cTEXTMapping("producer",tr("producer")));
    TEXTMappingAdd(new cTEXTMapping("writer",tr("writer")));
    TEXTMappingAdd(new cTEXTMapping("video",tr("video")));
    TEXTMappingAdd(new cTEXTMapping("blacknwhite",tr("blacknwhite")));
    TEXTMappingAdd(new cTEXTMapping("audio",tr("audio")));
    TEXTMappingAdd(new cTEXTMapping("dolby",tr("dolby")));
    TEXTMappingAdd(new cTEXTMapping("dolbydigital",tr("dolbydigital")));
    TEXTMappingAdd(new cTEXTMapping("bilingual",tr("bilingual")));
    TEXTMappingAdd(new cTEXTMapping("review",tr("review")));
    TEXTMappingAdd(new cTEXTMapping("starrating",tr("starrating")));
    TEXTMappingAdd(new cTEXTMapping("season",tr("season")));
    TEXTMappingAdd(new cTEXTMapping("episode",tr("episode")));
}

cPluginXmltv2vdr::~cPluginXmltv2vdr()
{
    // Clean up after yourself!
#if VDRVERSNUM < 10726 && (!EPGHANDLER)
    delete epghandler;
#endif
}

bool cPluginXmltv2vdr::IsIdle()
{
    if (!epgexecutor.Active() && (!epgtimer->Active())) return true;
    return false;
}

void cPluginXmltv2vdr::Wait4TimerThread()
{
    if (!epgtimer->Active()) return;
    dsyslog("xmltv2vdr: wait for timer thread");
    pthread_join(epgtimer->ThreadId(),NULL);
}

bool cPluginXmltv2vdr::EPGSourceMove(int From, int To)
{
    if (From==To) return false;
    if (!IsIdle()) return false;
    sqlite3 *db=NULL;
    char *sql=NULL;
    if (sqlite3_open_v2(epgfile,&db,SQLITE_OPEN_READWRITE,NULL)==SQLITE_OK)
    {
        if (asprintf(&sql,"BEGIN TRANSACTION;" \
                     "UPDATE epg SET srcidx=98 WHERE srcidx=%i;" \
                     "UPDATE epg SET srcidx=%i WHERE srcidx=%i;" \
                     "UPDATE epg SET srcidx=%i WHERE srcidx=98;" \
                     "COMMIT;", To, From, To, From)==-1)
        {
            sqlite3_close(db);
            return false;
        }
        if (sqlite3_exec(db,sql,NULL,NULL,NULL)!=SQLITE_OK)
        {
            free(sql);
            sqlite3_close(db);
            return false;
        }
    }
    free(sql);
    sqlite3_close(db);
    epgsources.Move(From,To);
    return true;
}


const char *cPluginXmltv2vdr::CommandLineHelp(void)
{
    // Return a string that describes all known command line options.
    return "  -E FILE,   --epgfile=FILE write the EPG data into the given FILE(default is\n"
           "                            'epg.db' in the video directory)\n";
}

bool cPluginXmltv2vdr::ProcessArgs(int argc, char *argv[])
{
    // Command line argument processing
    static struct option long_options[] =
    {
        { "epgfile",      required_argument, NULL, 'E'
        },
        { 0,0,0,0 }
    };

    int c;
    while ((c = getopt_long(argc, argv, "E:", long_options, NULL)) != -1)
    {
        switch (c)
        {
        case 'E':
            if (epgfile) free(epgfile);
            epgfile=strdup(optarg);
            isyslog("xmltv2vdr: using file '%s' for epgdata",optarg);
            break;
        default:
            return false;
        }
    }
    return true;
}

bool cPluginXmltv2vdr::Initialize(void)
{
    // Initialize any background activities the plugin shall perform.
    return true;
}

bool cPluginXmltv2vdr::Start(void)
{
    // Start any background activities the plugin shall perform.
    if (confdir) free(confdir);
    confdir=strdup(ConfigDirectory(PLUGIN_NAME_I18N)); // creates internally the confdir!
    if (!epgfile)
    {
        if (asprintf(&epgfile,"%s/epg.db",VideoDirectory)==-1)return false;
    }
    cParse::InitLibXML();

    ReadInEPGSources();
    epghandler = new cEPGHandler(this,epgfile,&epgsources,&epgmappings,&textmappings);
    epgtimer = new cEPGTimer(epgfile,&epgsources,&epgmappings,&textmappings);

    if (sqlite3_threadsafe()==0) esyslog("xmltv2vdr: ERROR sqlite3 not threadsafe!");
    return true;
}

void cPluginXmltv2vdr::Stop(void)
{
    // Stop any background activities the plugin is performing.
    cSchedules::Cleanup(true);
    epgtimer->Stop();
    delete epgtimer;
    epgexecutor.Stop();
    epgsources.Remove();
    epgmappings.Remove();
    textmappings.Remove();
    cParse::CleanupLibXML();
    if (confdir)
    {
        free(confdir);
        confdir=NULL;
    }
    if (epgfile)
    {
        free(epgfile);
        epgfile=NULL;
    }
    if (srcorder)
    {
        free(srcorder);
        srcorder=NULL;
    }
}

void cPluginXmltv2vdr::Housekeeping(void)
{
    // Perform any cleanup or other regular tasks.
    time_t now=time(NULL);
    if (now>(last_housetime_t+3600))
    {
        if (IsIdle())
        {
            struct stat statbuf;
            if (stat(epgfile,&statbuf)!=-1)
            {
                if (statbuf.st_size)
                {
                    sqlite3 *db=NULL;
                    if (sqlite3_open_v2(epgfile,&db,SQLITE_OPEN_READWRITE,NULL)==SQLITE_OK)
                    {
                        char *sql;
                        if (asprintf(&sql,"delete from epg where ((starttime+duration) < %li)",now)!=-1)
                        {
                            char *errmsg;
                            if (sqlite3_exec(db,sql,NULL,NULL,&errmsg)!=SQLITE_OK)
                            {
                                esyslog("xmltv2vdr: %s",errmsg);
                                sqlite3_free(errmsg);
                            }
                            else
                            {
                                isyslog("xmltv2vdr: removed %i old entries from db",sqlite3_changes(db));
                                sqlite3_exec(db,"VACCUM;",NULL,NULL,NULL);
                            }
                            free(sql);
                        }
                    }
                    sqlite3_close(db);
                }
            }
        }
        last_housetime_t=now;
    }
}

void cPluginXmltv2vdr::MainThreadHook(void)
{
    // Perform actions in the context of the main program thread.
    // WARNING: Use with great care - see PLUGINS.html!
    time_t now=time(NULL);
    if (now>(last_maintime_t+60))
    {
        if (!epgexecutor.Active())
        {
            if (epgsources.RunItNow()) epgexecutor.Start();
        }
        last_maintime_t=now;
    }
    if (epall)
    {
        if (now>(last_epcheck_t+600))
        {
            if (IsIdle())
            {
                epgtimer->Start();
            }
            last_epcheck_t=now;
        }
    }
}

cString cPluginXmltv2vdr::Active(void)
{
    // Return a message string if shutdown should be postponed
    if (epgexecutor.Active())
    {
        return tr("xmltv2vdr plugin still working");
    }
    return NULL;
}

time_t cPluginXmltv2vdr::WakeupTime(void)
{
    // Return custom wakeup time for shutdown script
    if (!wakeup) return (time_t) 0;

    time_t nt=epgsources.NextRunTime();
    if (nt) nt-=(time_t) 180;
    return nt;
}

const char *cPluginXmltv2vdr::MainMenuEntry(void)
{
    // Return a main menu entry
    return NULL;
}

cOsdObject *cPluginXmltv2vdr::MainMenuAction(void)
{
    // Perform the action when selected from the main VDR menu.
    return NULL;
}

cMenuSetupPage *cPluginXmltv2vdr::SetupMenu(void)
{
    // Return a setup menu in case the plugin supports one.
    return new cMenuSetupXmltv2vdr(this);
}

bool cPluginXmltv2vdr::SetupParse(const char *Name, const char *Value)
{
    // Parse your own setup parameters and store their values.
    if (!strncasecmp(Name,"channel",7))
    {
        if (strlen(Name)<10) return false;
        epgmappings.Add(new cEPGMapping(&Name[8],Value));
    }
    else if (!strncasecmp(Name,"textmap",7))
    {
        if (strlen(Name)<10) return false;
        cTEXTMapping *textmap=TEXTMapping(&Name[8]);
        if (textmap)
        {
            textmap->ChangeValue(Value);
        }
        else
        {
            textmappings.Add(new cTEXTMapping(&Name[8],Value));
        }
    }
    else if (!strcasecmp(Name,"options.epall"))
    {
        SetEPAll((bool) atoi(Value));
    }
    else if (!strcasecmp(Name,"options.wakeup"))
    {
        wakeup=(bool) atoi(Value);
    }
    else if (!strcasecmp(Name,"source.order"))
    {
        srcorder=strdup(Value);
    }
    else return false;
    return true;
}

bool cPluginXmltv2vdr::Service(const char *UNUSED(Id), void *UNUSED(Data))
{
    // Handle custom service requests from other plugins
    return false;
}

const char **cPluginXmltv2vdr::SVDRPHelpPages(void)
{
    // Returns help text
    static const char *HelpPages[]=
    {
        "UPDT\n"
        "    Start epg update",
        NULL
    };
    return HelpPages;
}

cString cPluginXmltv2vdr::SVDRPCommand(const char *Command, const char *UNUSED(Option), int &ReplyCode)
{
    // Process SVDRP commands

    cString output;
    if (!strcasecmp(Command,"UPDT"))
    {
        if (!epgsources.Count())
        {
            ReplyCode=550;
            output="No epg sources installed\n";
        }
        else
        {
            if (epgexecutor.Start())
            {
                ReplyCode=250;
                output="Update started\n";
            }
            else
            {
                ReplyCode=550;
                output="Update already running\n";
            }
        }
    }
    else
    {
        return NULL;
    }
    return output;
}

VDRPLUGINCREATOR(cPluginXmltv2vdr) // Don't touch this!
