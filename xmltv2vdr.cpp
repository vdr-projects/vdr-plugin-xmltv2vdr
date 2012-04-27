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
#include <stdarg.h>
#include <locale.h>
#include <langinfo.h>
#include <sqlite3.h>
#include <sys/types.h>
#include <pwd.h>

#include "setup.h"
#include "xmltv2vdr.h"
#include "debug.h"

int ioprio_set(int which, int who, int ioprio)
{
#if defined(__i386__)
#define __NR_ioprio_set  289
#elif defined(__ppc__)
#define __NR_ioprio_set  273
#elif defined(__x86_64__)
#define __NR_ioprio_set  251
#elif defined(__ia64__)
#define __NR_ioprio_set  1274
#else
#define __NR_ioprio_set  0
#endif
    if (__NR_ioprio_set)
    {
        return syscall(__NR_ioprio_set, which, who, ioprio);
    }
    else
    {
        return 0; // just do nothing
    }
}

char *logfile=NULL;

void logger(cEPGSource *source, char logtype, const char* format, ...)
{
    va_list ap;
    char fmt[255];
    if (source && logtype!='T')
    {
        if (logtype=='E')
        {
            if (snprintf(fmt,sizeof(fmt),"xmltv2vdr: '%s' ERROR %s",source->Name(),format)==-1) return;
        }
        else
        {
            if (snprintf(fmt,sizeof(fmt),"xmltv2vdr: '%s' %s",source->Name(),format)==-1) return;
        }
    }
    else
    {
        if (logtype=='E')
        {
            snprintf(fmt,sizeof(fmt),"xmltv2vdr: ERROR %s",format);
        }
        else
        {
            snprintf(fmt,sizeof(fmt),"xmltv2vdr: %s",format);
        }
    }

    va_start(ap, format);
    char *ptr;
    if (vasprintf(&ptr,fmt,ap)==-1) return;
    va_end(ap);

    struct tm tm;
    if (logfile || source)
    {
        time_t now=time(NULL);
        localtime_r(&now,&tm);
    }

    char *crlf=strchr(ptr,'\n');
    if (crlf) *crlf=0;
    crlf=strchr(ptr,'\r');
    if (crlf) *crlf=0;

    if (source && logtype!='T')
    {
        source->Add2Log(&tm,logtype,ptr);
    }

    if (logfile)
    {
        char dt[30];
        strftime(dt,sizeof(dt)-1,"%b %d %H:%M:%S",&tm);

        FILE *l=fopen(logfile,"a+");
        if (l)
        {
            fprintf(l,"%s [%i] %s\n",dt,cThread::ThreadId(),ptr);
            fclose(l);
        }
    }
    switch (logtype)
    {
    case 'E':
        if (SysLogLevel>0) syslog_with_tid(LOG_ERR,"%s",ptr);
        break;
    case 'I':
        if (SysLogLevel>1) syslog_with_tid(LOG_ERR,"%s",ptr);
        break;
    case 'D':
        if (SysLogLevel>2) syslog_with_tid(LOG_ERR,"%s",ptr);
        break;
    default:
        break;
    }

    free(ptr);
}

// -------------------------------------------------------------

cEPGHandler::cEPGHandler(const char *EpgFile, const char *EPDir, const char *CodeSet, cEPGSources *Sources,
                         cEPGMappings *Maps, cTEXTMappings *Texts) : import(EpgFile,EPDir,CodeSet,Maps,Texts)
{
    epall=false;
    maps=Maps;
    sources=Sources;
    db=NULL;
    now=time(NULL);
}

bool cEPGHandler::IgnoreChannel(const cChannel* Channel)
{
    now=time(NULL);
    if (!maps) return false;
    if (!Channel) return false;
    return maps->IgnoreChannel(Channel);
}

bool cEPGHandler::check4proc(cEvent *event, bool &spth, cEPGMapping **map)
{
    if (map) *map=NULL;
    spth=false;
    if (!event) return false;
    if (now>(event->StartTime()+event->Duration())) return false; // event in the past?
    if (!maps) return false;
    if (!import.DBExists()) return false;

    cEPGMapping *t_map=maps->GetMap(event->ChannelID());
    if (!t_map)
    {
        if (!epall) return false;
        if (!event->HasTimer()) return false;
        if (!event->ShortText()) return false;
        spth=true;
    }
    *map=t_map;
    return true;
}

bool cEPGHandler::SetShortText(cEvent* Event, const char* ShortText)
{
    bool seth;
    if (!check4proc(Event,seth,NULL)) return false;

    if (import.WasChanged(Event))
    {
        // ok we already changed this event!
#ifdef VDRDEBUG
        tsyslog("{%i} %salready seen stext '%s'",Event->EventID(),seth ? "*" : "",
                Event->Title());
#endif
        return true;
    }
    // prevent setting empty shorttext
    if (!ShortText) return true;
    // prevent setting empty shorttext
    if (!strlen(ShortText)) return true;
    // prevent setting shorttext equal to title
    if (Event->Title() && !strcasecmp(Event->Title(),ShortText)) return true;
    tsyslog("{%i} %ssetting stext (%s) of '%s'",Event->EventID(),seth ? "*" : "",
            ShortText,Event->Title());
    return false;
}

bool cEPGHandler::SetDescription(cEvent* Event, const char* Description)
{
    bool seth;
    if (!check4proc(Event,seth,NULL)) return false;

    if (import.WasChanged(Event))
    {
        // ok we already changed this event!
        if (!Description) return true; // prevent setting nothing to description
        int len=strlen(Description);
        if (strncasecmp(Event->Description(),Description,len))
        {
            // eit description changed -> set it
            tsyslog("{%i} %schanging descr of '%s'",Event->EventID(),seth ? "*" : "",
                    Event->Title());
            return false;
        }
#ifdef VDRDEBUG
        tsyslog("{%i} %salready seen descr '%s'",Event->EventID(),seth ? "*" : "",
                Event->Title());
#endif
        return true;
    }
    tsyslog("{%i} %ssetting descr of '%s'",Event->EventID(),seth ? "*" : "",
            Event->Title());
    return false;
}


bool cEPGHandler::HandleEvent(cEvent* Event)
{
    bool special_epall_timer_handling;
    cEPGMapping *map;
    if (!check4proc(Event,special_epall_timer_handling,&map)) return false;

    int Flags=0;
    const char *ChannelID=strdup(*Event->ChannelID().ToString());
    if (!ChannelID) return false;

    if (special_epall_timer_handling)
    {
        Flags=USE_SEASON;
    }
    else
    {
        // map is always set if seth is not set
        Flags=map->Flags();
    }

    if (ioprio_set(1,getpid(),7 | 3 << 13)==-1)
    {
        tsyslog("failed to set ioprio to 3,7");
    }

    cEPGSource *source=NULL;
    cXMLTVEvent *xevent=import.SearchXMLTVEvent(&db,ChannelID,Event);
    if (!xevent)
    {
        if (!epall)
        {
            free((void*)ChannelID);
            return false;
        }
        source=sources->GetSource(EITSOURCE);
        if (!source) tsyslog("no source for %s",EITSOURCE);
        xevent=import.AddXMLTVEvent(source,db,ChannelID,Event,Event->Description());
        if (!xevent)
        {
            free((void*)ChannelID);
            return false;
        }
    }
    else
    {
        source=sources->GetSource(xevent->Source());
    }
    free((void*)ChannelID);
    if (!source)
    {
        tsyslog("no source for %s",xevent->Source());
        delete xevent;
        return false;
    }

    import.PutEvent(source,db,(cSchedule *) Event->Schedule(),Event,xevent,Flags);
    delete xevent;
    return false; // let other handlers change this event
}

bool cEPGHandler::SortSchedule(cSchedule* UNUSED(Schedule))
{
    if (db)
    {
        import.Commit(NULL,db);
        sqlite3_close(db);
        db=NULL;
    }
    return false; // we dont sort!
}


// -------------------------------------------------------------

cEPGTimer::cEPGTimer(const char *EpgFile, const char *EPDir, const char *CodeSet, cEPGSources *Sources, cEPGMappings *Maps,
                     cTEXTMappings *Texts) : cThread("xmltv2vdr timer"),import(EpgFile,EPDir,CodeSet,Maps,Texts)
{
    sources=Sources;
    maps=Maps;
}

void cEPGTimer::Action()
{
    if (!import.DBExists()) return; // no database? -> exit immediately
    SetPriority(19);
    if (ioprio_set(1,getpid(),7 | 3 << 13)==-1)
    {
        dsyslog("failed to set ioprio to 3,7");
    }

    cSchedulesLock schedulesLock(true,10); // wait 10ms for lock!
    const cSchedules *schedules = cSchedules::Schedules(schedulesLock);
    if (!schedules) return;

    if (Timers.BeingEdited()) return;
    Timers.IncBeingEdited();

    sqlite3 *db=NULL;
    cEPGSource *source=sources->GetSource(EITSOURCE);
    for (cTimer *Timer = Timers.First(); Timer; Timer = Timers.Next(Timer))
    {
        cEvent *event=(cEvent *) Timer->Event();
        if (!event) continue;
        if (!event->ShortText()) continue; // no short text -> no episode
        if (!strlen(event->ShortText())) continue; // empty short text -> no episode
        if (maps->ProcessChannel(event->ChannelID())) continue; // already processed by xmltv2vdr

        cChannel *chan=Channels.GetByChannelID(event->ChannelID());
        if (!chan) continue;
        const char *ChannelID=strdup(*event->ChannelID().ToString());

        cXMLTVEvent *xevent=import.SearchXMLTVEvent(&db,ChannelID,event);
        if (!xevent)
        {
            xevent=import.AddXMLTVEvent(source,db,ChannelID,event,event->Description());
            if (!xevent)
            {
                free((void*)ChannelID);
                continue;
            }
        }
        free((void*)ChannelID);

        cSchedule* schedule = (cSchedule *) schedules->GetSchedule(chan,false);
        if (schedule)
        {
            import.PutEvent(source,db,schedule,event,xevent,USE_SEASON);
        }
        delete xevent;
    }
    if (db)
    {
        import.Commit(source,db);
        sqlite3_close(db);
    }
    Timers.DecBeingEdited();
    cSchedules::Cleanup(true);
}

// -------------------------------------------------------------

cHouseKeeping::cHouseKeeping(const char *EPGFile): cThread("xmltv2vdr housekeeping")
{
    epgfile=EPGFile;
}

void cHouseKeeping::Action()
{
    cSchedulesLock schedulesLock(true,10); // wait 10ms for lock!
    const cSchedules *schedules = cSchedules::Schedules(schedulesLock);
    if (!schedules) return;

    sqlite3 *db=NULL;
    if (sqlite3_open_v2(epgfile,&db,SQLITE_OPEN_READWRITE,NULL)==SQLITE_OK)
    {
        char *sql;
        if (asprintf(&sql,"delete from epg where ((starttime+duration) < %li)",time(NULL))!=-1)
        {
            char *errmsg;
            if (sqlite3_exec(db,sql,NULL,NULL,&errmsg)!=SQLITE_OK)
            {
                esyslog("%s",errmsg);
                sqlite3_free(errmsg);
            }
            else
            {
                int changes=sqlite3_changes(db);
                if (changes)
                {
                    isyslog("removed %i old entries from db",changes);
                    sqlite3_exec(db,"VACCUM;",NULL,NULL,NULL);
                }
            }
            free(sql);
        }
    }
    sqlite3_close(db);
}

// -------------------------------------------------------------

cPluginXmltv2vdr::cPluginXmltv2vdr(void) : epgexecutor(&epgsources)
{
    // Initialize any member variables here.
    // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
    // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
    confdir=NULL;
    epgfile=NULL;
    logfile=NULL;
    srcorder=NULL;
    epghandler=NULL;
    epgtimer=NULL;
    epdir=NULL;
    housekeeping=NULL;
    codeset=NULL;
    last_housetime_t=0;
    last_maintime_t=0;
    last_epcheck_t=0;
    wakeup=0;
    insetup=false;
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

int cPluginXmltv2vdr::GetLastImportSource()
{
    sqlite3 *db=NULL;
    if (sqlite3_open_v2(epgfile,&db,SQLITE_OPEN_READWRITE,NULL)!=SQLITE_OK) return -1;

    char sql[]="select srcidx from epg where srcidx<>99 order by starttime desc limit 1";
    sqlite3_stmt *stmt;
    
    int ret=sqlite3_prepare_v2(db,sql,strlen(sql),&stmt,NULL);
    if (ret!=SQLITE_OK)
    {
        esyslog("%i %s (glis)",ret,sqlite3_errmsg(db));      
        sqlite3_close(db);
        return -1;
    }

    int idx=-1;
    if (sqlite3_step(stmt)==SQLITE_ROW)
    {
        idx=sqlite3_column_int(stmt,0);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    tsyslog("lastimportsource=%i",idx);
    return idx;
}



bool cPluginXmltv2vdr::EPGSourceMove(int From, int To)
{
    if (From==To) return false;

    sqlite3 *db=NULL;
    if (sqlite3_open_v2(epgfile,&db,SQLITE_OPEN_READWRITE,NULL)==SQLITE_OK)
    {
        char *sql=NULL;
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
        free(sql);
    }
    sqlite3_close(db);
    epgsources.Move(From,To);
    return true;
}


const char *cPluginXmltv2vdr::CommandLineHelp(void)
{
    // Return a string that describes all known command line options.
    return "  -e DIR,   --episodes=DIR location of episode files: VDRSeriesTimer .episodes\n"
           "                           or TheTVDB .xml (default is ~/.eplists/lists,UTF8)\n"
           "                           Add UTF-8 or ISO8859-15 to the path to specify the\n"
           "                           charset used in the files, e.g. /vdr/myepisodes,UTF8\n"
           "  -E FILE,  --epgfile=FILE write the EPG data into the given FILE (default is\n"
           "                           'epg.db' in the video directory) - best performance\n"
           "                           if located on a ramdisk\n"
           "  -l FILE   --logfile=FILE write trace logs into the given FILE (default is\n"
           "                           no trace log\n";

}

bool cPluginXmltv2vdr::ProcessArgs(int argc, char *argv[])
{
    // Command line argument processing
    static struct option long_options[] =
    {
        { "episodes",     required_argument, NULL, 'e'},
        { "epgfile",      required_argument, NULL, 'E'},
        { "logfile",      required_argument, NULL, 'l'},
        { 0,0,0,0 }
    };

    int c;
    char *comma=NULL;
    while ((c = getopt_long(argc, argv, "l:e:E:", long_options, NULL)) != -1)
    {
        switch (c)
        {
        case 'e':
            if (epdir) free(epdir);
            epdir=strdup(optarg);
            if (!epdir) break;
            comma=strchr(epdir,',');
            if (comma) *comma=0;
            if (access(epdir,R_OK)!=-1)
            {
                *comma=',';
            }
            else
            {
                free(epdir);
                epdir=NULL;
            }
            break;
        case 'E':
            if (epgfile) free(epgfile);
            epgfile=strdup(optarg);
            if (!epgfile) break;
//            isyslog("using file '%s' for epgdata",optarg);
            break;
        case 'l':
            if (logfile) free(logfile);
            logfile=strdup(optarg);
            if (!logfile) break;
//            isyslog("using file '%s' for log",optarg);
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
    isyslog("using file '%s' for epg database",epgfile);
    if (!epdir)
    {
        struct passwd pwd,*pwdbuf;
        char buf[1024];
        getpwuid_r(getuid(),&pwd,buf,sizeof(buf),&pwdbuf);
        if (pwdbuf)
        {
            if (asprintf(&epdir,"%s/.eplists/lists",pwdbuf->pw_dir)!=-1)
            {
                if (access(epdir,R_OK))
                {
                    free(epdir);
                    epdir=NULL;
                }
                else
                {
                    isyslog("using dir '%s' (UTF-8) for episodes",epdir);
                }
            }
        }
    }
    else
    {
        char *cs=strchr(epdir,',');
        if (cs)
        {
            *cs=0;
            isyslog("using dir '%s' (%s) for episodes",epdir,cs+1);
            *cs=',';
        }
        else
        {
            isyslog("using dir '%s' (UTF-8) for episodes",epdir);
        }
    }
    if (setlocale(LC_CTYPE,""))
        codeset=strdup(nl_langinfo(CODESET));
    else
    {
        char *LangEnv=getenv("LANG");
        if (LangEnv)
        {
            codeset=strdup(strchr(LangEnv,'.'));
            if (codeset)
                codeset++; // skip dot
        }
    }
    if (!codeset)
    {
        codeset=strdup("US-ASCII//TRANSLIT");
    }
    isyslog("codeset is '%s'",codeset);

    ReadInEPGSources();
    epghandler = new cEPGHandler(epgfile,epdir,codeset,&epgsources,&epgmappings,&textmappings);
    epgtimer = new cEPGTimer(epgfile,epdir,codeset,&epgsources,&epgmappings,&textmappings);
    housekeeping = new cHouseKeeping(epgfile);

    if (sqlite3_threadsafe()==0) esyslog("sqlite3 not threadsafe!");
    cParse::InitLibXML();
    return true;
}

void cPluginXmltv2vdr::Stop(void)
{
    // Stop any background activities the plugin is performing.
    cSchedules::Cleanup(true);
    epgtimer->Stop();
    if (epgtimer)
    {
        delete epgtimer;
        epgtimer=NULL;
    }
    if (housekeeping)
    {
        delete housekeeping;
        housekeeping=NULL;
    }
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
    if (epdir)
    {
        free(epdir);
        epdir=NULL;
    }
    if (logfile)
    {
        free(logfile);
        logfile=NULL;
    }
    if (srcorder)
    {
        free(srcorder);
        srcorder=NULL;
    }
    if (codeset)
    {
        free(codeset);
        codeset=NULL;
    }
}

void cPluginXmltv2vdr::Housekeeping(void)
{
    // Perform any cleanup or other regular tasks.
    time_t now=time(NULL);
    if (now>(last_housetime_t+3600))
    {
        if (housekeeping)
        {
            struct stat statbuf;
            if (stat(epgfile,&statbuf)!=-1)
            {
                if (statbuf.st_size)
                {
                    housekeeping->Start();
                }
            }
        }
        last_housetime_t=(now / 3600)*3600;
    }
}

void cPluginXmltv2vdr::MainThreadHook(void)
{
    // Perform actions in the context of the main program thread.
    // WARNING: Use with great care - see PLUGINS.html!
    time_t now=time(NULL);
    if (now>=(last_maintime_t+60))
    {
        if (!epgexecutor.Active())
        {
            if (epgsources.RunItNow()) epgexecutor.Start();
        }
        last_maintime_t=(now/60)*60;
    }
    if (epall)
    {
        if (now>=(last_epcheck_t+600))
        {
            epgtimer->Start();
            last_epcheck_t=(now/600)*600;
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
    time_t nextruntime=epgsources.NextRunTime();
    if (nextruntime) nextruntime-=(time_t) 180;
    tsyslog("reporting wakeuptime %s",ctime(&nextruntime));
    return nextruntime;
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
        "UPDT [force]\n"
        "    Start epg update from db, with force download data before",
        NULL
    };
    return HelpPages;
}

cString cPluginXmltv2vdr::SVDRPCommand(const char *Command, const char *Option, int &ReplyCode)
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
            if (epgexecutor.Active())
            {
                ReplyCode=550;
                output="Update already running\n";
            }
            else
            {
                if (Option && strstr(Option,"force"))
                {
                    epgexecutor.SetForceDownload();
                }
                else
                {
                    epgexecutor.SetForceImport(GetLastImportSource());
                }
                if (epgexecutor.Start())
                {
                    ReplyCode=250;
                    output="Update started\n";
                }
                else
                {
                    ReplyCode=550;
                    output="Failed to start update\n";
                }
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
