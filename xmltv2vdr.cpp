/*
 * xmltv2vdr.cpp: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <vdr/plugin.h>
#include <string.h>
#include <sys/wait.h>
#include "xmltv2vdr.h"
#include "extpipe.h"
#include "setup.h"

#if __GNUC__ > 3
#define UNUSED(v) UNUSED_ ## v __attribute__((unused))
#else
#define UNUSED(x) x
#endif

// -------------------------------------------------------------

cEPGChannel::cEPGChannel(const char *Name, bool InUse)
{
    name=strdup(Name);
    inuse=InUse;
}

cEPGChannel::~cEPGChannel()
{
    if (name) free((void *) name);
}

int cEPGChannel::Compare(const cListObject &ListObject) const
{
    cEPGChannel *epgchannel= (cEPGChannel *) &ListObject;
    return strcmp(name,epgchannel->Name());
}

// -------------------------------------------------------------

cEPGExecute::cEPGExecute(const char *Name, cEPGMappings *Maps, cTEXTMappings *Texts):cThread(Name)
{
    dsyslog("xmltv2vdr: added epgsource '%s'",Name);
    name=strdup(Name);
    pipe=false;
    daysinadvance=0;
    ready2parse=ReadConfig();
    parse=new cParse(Name, Maps, Texts);
}

cEPGExecute::~cEPGExecute()
{
    Stop();
    free((void*) name);
    if (parse) delete parse;
}

bool cEPGExecute::ReadConfig()
{
    dsyslog("xmltv2vdr: reading config of epgsource '%s'",name);
    char *fname=NULL;
    if (asprintf(&fname,"%s/%s",EPGSOURCES,name)!=-1)
    {
        FILE *f=fopen(fname,"r+");
        if (f)
        {
            size_t lsize;
            char *line=NULL;
            int linenr=1;
            while (getline(&line,&lsize,f)!=-1)
            {
                if (linenr==1)
                {
                    if (!strncmp(line,"pipe",4))
                    {
                        dsyslog("xmltv2vdr: '%s' is providing data through a pipe",name);
                        pipe=true;
                    }
                    else
                    {
                        dsyslog("xmltv2vdr: '%s' is providing data through a file",name);
                        pipe=false;
                    }
                }
                if (linenr==2)
                {
                    char *semicolon=strchr(line,';');
                    if (semicolon)
                    {
                        *semicolon=0;
                        semicolon++;
                        daysinadvance=atoi(line);
                        daysmax=atoi(semicolon);
                        dsyslog("xmltv2vdr: '%s' days=%i/%i",name,daysinadvance,daysmax);
                    }
                }
                if (linenr>2)
                {
                    // channels
                    char *semicolon=strchr(line,';');
                    if (semicolon) *semicolon=0;
                    char *lf=strchr(line,10);
                    if (lf) *lf=0;
                    bool used=false;
                    char *cname=line;
                    if (line[0]=='*')
                    {
                        cname++;
                        used=true;
                    }
                    cEPGChannel *epgchannel= new cEPGChannel(cname,used);
                    if (epgchannel) channels.Add(epgchannel);
                }
                linenr++;
            }
            if (line) free(line);
            channels.Sort();
            fclose(f);
            free(fname);
            return true;
        }
        else
        {
            esyslog("xmltv2vdr: '%s' cannot read config file",name);
        }
        free(fname);
    }
    return false;
}

void cEPGExecute::SetChannelSelection(int *Selection)
{
    for (int i=0; i<channels.Count(); i++)
    {
        channels.Get(i)->SetUsage(Selection[i]);
    }
}

void cEPGExecute::Action()
{
    if (!ready2parse) return;
    if (!parse) return;
    char *result=NULL;
    int l=0;

    if (pipe)
    {
        cExtPipe p;
        if (p.Open(name,"r"))
        {
            dsyslog("xmltv2vdr: executing epgsource '%s'",name);
            int c;
            while ((c=fgetc(p))!=EOF)
            {
                if (l%20==0) result=(char *) realloc(result, l+21);
                result[l++]=c;
            }
            int status;
            if (p.Close(status)>0)
            {
                int returncode=WEXITSTATUS(status);
                if ((!returncode) && (result))
                {
                    dsyslog("xmltv2vdr: parsing output of '%s'",name);
                    result[l]=0;
                    if (!parse->Process(result,l))
                    {
                        esyslog("xmltv2vdr: failed to parse output of '%s'",name);
                    }
                }
                else
                {
                    esyslog("xmltv2vdr: epgsource '%s' returned with %i",name,returncode);
                }
            }
            if (result) free(result);
        }
        else
        {
            esyslog("xmltv2vdr: failed to open pipe for '%s'",name);
        }
    }
    else
    {
        char *fname=NULL;
        if (asprintf(&fname,"%s/%s.xmltv",EPGSOURCES,name)!=-1)
        {
            int fd=open(fname,O_RDONLY);
            if (fd!=-1)
            {
                struct stat statbuf;
                if (fstat(fd,&statbuf)!=-1)
                {
                    l=statbuf.st_size;
                    result=(char *) malloc(l+1);
                    if (result)
                    {
                        if (read(fd,result,statbuf.st_size)==statbuf.st_size)
                        {
                            parse->Process(result,l);
                        }
                        free(result);
                    }
                }
                close(fd);
            }
            else
            {
                esyslog("xmltv2vdr: failed to open file '%s' for '%s'",fname,name);
            }
            free(fname);
        }
    }
}

// -------------------------------------------------------------

cEPGSource::cEPGSource(const char *Name, cEPGMappings *Maps, cTEXTMappings *Texts):exec(Name,Maps,Texts)
{
    name=strdup(Name);
}

cEPGSource::~cEPGSource()
{
    dsyslog("xmltv2vdr: removed epgsource '%s'",name);
    free((void *) name);
}

bool cEPGSource::Execute()
{
    if (exec.Active()) return false;
    exec.Start();
    return true;
}

void cEPGSource::Store(void)
{
    char *fname1=NULL;
    char *fname2=NULL;
    if (asprintf(&fname1,"%s/%s",EPGSOURCES,name)==-1) return;
    if (asprintf(&fname2,"%s/%s.new",EPGSOURCES,name)==-1)
    {
        free(fname1);
        return;
    }

    FILE *r=fopen(fname1,"r+");
    if (!r)
    {
        free(fname1);
        free(fname2);
        return;
    }
    int oldmask=umask(0664);
    FILE *w=fopen(fname2,"w+");
    umask(oldmask);
    if (!w)
    {
        fclose(w);
        unlink(fname2);
        free(fname1);
        free(fname2);
        return;
    }

    char *line=NULL;
    size_t lsize;
    int linenr=1;
    while (getline(&line,&lsize,r)!=-1)
    {
        if (linenr==2)
        {
            fprintf(w,"%i;%i\n",DaysInAdvance(),DaysMax());
        }
        else if (linenr>2)
        {
            char *txt=line;
            if (txt[0]=='*') txt++;
            for (int i=0; i<ChannelList()->Count(); i++)
            {
                if (!strncmp(txt,ChannelList()->Get(i)->Name(),strlen(ChannelList()->Get(i)->Name())))
                {
                    if (ChannelList()->Get(i)->InUse())
                    {
                        fprintf(w,"*%s",txt);
                    }
                    else
                    {
                        fprintf(w,"%s",txt);
                    }
                    break;
                }
            }
        }
        else
        {
            fprintf(w,"%s",line);
        }
        linenr++;
    }
    if (line) free(line);
    struct stat statbuf;
    if (fstat(fileno(r),&statbuf)!=-1)
    {
        if (fchown(fileno(w),statbuf.st_uid,statbuf.st_gid)) {};
        if (fchmod(fileno(w),statbuf.st_mode | S_IRGRP | S_IWGRP)) {};
    }
    fclose(w);
    fclose(r);
    rename(fname2,fname1);
    free(fname1);
    free(fname2);
}

// -------------------------------------------------------------

bool cPluginXmltv2vdr::epgsourceexists(const char *name)
{
    if (!epgsources.Count()) return false;
    for (cEPGSource *epgs=epgsources.First(); epgs; epgs=epgsources.Next(epgs))
    {
        if (!strcmp(epgs->Name(),name)) return true;
    }
    return false;
}

void cPluginXmltv2vdr::removeepgmappings()
{
    cEPGMapping *maps;
    while ((maps=epgmappings.Last())!=NULL)
    {
        epgmappings.Del(maps);
    }
}

void cPluginXmltv2vdr::removetextmappings()
{
    cTEXTMapping *maps;
    while ((maps=textmappings.Last())!=NULL)
    {
        textmappings.Del(maps);
    }
}

void cPluginXmltv2vdr::removeepgsources()
{
    cEPGSource *epgs;
    while ((epgs=epgsources.Last())!=NULL)
    {
        epgsources.Del(epgs);
    }
}

bool cPluginXmltv2vdr::epgsourcesactive()
{
    bool ret=false;
    for (cEPGSource *epgs=epgsources.First(); epgs; epgs=epgsources.Next(epgs))
    {
        ret|=epgs->Active();
    }
    return ret;
}

bool cPluginXmltv2vdr::executeepgsources()
{
    bool ret=false;
    for (cEPGSource *epgs=epgsources.First(); epgs; epgs=epgsources.Next(epgs))
    {
        ret|=epgs->Execute();
    }
    return ret;
}

cEPGMapping *cPluginXmltv2vdr::EPGMapping(const char *ChannelName)
{
    if (!epgmappings.Count()) return NULL;
    for (cEPGMapping *maps=epgmappings.First(); maps; maps=epgmappings.Next(maps))
    {
        if (!strcmp(maps->ChannelName(),ChannelName)) return maps;
    }
    return NULL;
}

cTEXTMapping *cPluginXmltv2vdr::TEXTMapping(const char *Name)
{
    if (!textmappings.Count()) return NULL;
    for (cTEXTMapping *textmap=textmappings.First(); textmap; textmap=textmappings.Next(textmap))
    {
        if (!strcmp(textmap->Name(),Name)) return textmap;
    }
    return NULL;
}

void cPluginXmltv2vdr::ReadInEPGSources(bool Reload)
{
    if (Reload) removeepgsources();
    DIR *dir=opendir(EPGSOURCES);
    if (!dir) return;
    struct dirent *dirent;
    while (dirent=readdir(dir))
    {
        if (strchr(&dirent->d_name[0],'.')) continue;
        if (!epgsourceexists(dirent->d_name))
        {
            char *path=NULL;
            if (asprintf(&path,"%s/%s",EPGSOURCES,dirent->d_name)!=-1)
            {
                if (access(path,R_OK|W_OK)!=-1)
                {
                    int fd=open(path,O_RDONLY);
                    if (fd!=-1)
                    {
                        char id[5];
                        if (read(fd,id,4)!=4)
                        {
                            esyslog("xmltv2vdr: cannot read config file '%s'",dirent->d_name);
                        }
                        else
                        {
                            id[4]=0;
                            if (!strcmp(id,"file") || !strcmp(id,"pipe"))
                            {
                                epgsources.Add(new cEPGSource(dirent->d_name,&epgmappings,&textmappings));
                            }
                            else
                            {
                                dsyslog("xmltv2vdr: ignoring non config file '%s'",dirent->d_name);
                            }
                            close(fd);
                        }
                    }
                    else
                    {
                        esyslog("xmltv2vdr: cannot open config file '%s'",dirent->d_name);
                    }
                }
                else
                {
                    esyslog("xmltv2vdr: cannot access config file '%s'",dirent->d_name);
                }
                free(path);
            }
        }
    }
    closedir(dir);
}

void cPluginXmltv2vdr::SetExecTime(int ExecTime)
{
    exectime=ExecTime;
    exectime_t=cTimer::SetTime(time(NULL),cTimer::TimeToInt(exectime));
    last_exectime_t=0;
}

cPluginXmltv2vdr::cPluginXmltv2vdr(void)
{
    // Initialize any member variables here.
    // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
    // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
    wakeup=0;
    SetExecTime(200);
    TEXTMappingAdd(new cTEXTMapping("country",tr("country")));
    TEXTMappingAdd(new cTEXTMapping("date",tr("year")));
    TEXTMappingAdd(new cTEXTMapping("originaltitle",tr("originaltitle")));
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
    TEXTMappingAdd(new cTEXTMapping("review",tr("review")));
}

cPluginXmltv2vdr::~cPluginXmltv2vdr()
{
    // Clean up after yourself!
}

const char *cPluginXmltv2vdr::CommandLineHelp(void)
{
    // Return a string that describes all known command line options.
    return NULL;
}

bool cPluginXmltv2vdr::ProcessArgs(int UNUSED(argc), char *UNUSED(argv[]))
{
    // Implement command line argument processing here if applicable.
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
    cParse::InitLibXML();
    ReadInEPGSources();
    return true;
}

void cPluginXmltv2vdr::Stop(void)
{
    // Stop any background activities the plugin is performing.
    removeepgsources();
    removeepgmappings();
    removetextmappings();
    cParse::CleanupLibXML();
}

void cPluginXmltv2vdr::Housekeeping(void)
{
    // Perform any cleanup or other regular tasks.
}

void cPluginXmltv2vdr::MainThreadHook(void)
{
    // Perform actions in the context of the main program thread.
    // WARNING: Use with great care - see PLUGINS.html!
    time_t now=time(NULL);
    if (((now>=exectime_t) && (now<(exectime_t+10))) &&
            (last_exectime_t!=exectime_t))
    {
        executeepgsources();
        last_exectime_t=exectime_t;
    }
}

cString cPluginXmltv2vdr::Active(void)
{
    // Return a message string if shutdown should be postponed
    if (epgsourcesactive())
    {
        return tr("xmltv2vdr plugin still working");
    }
    return NULL;
}

time_t cPluginXmltv2vdr::WakeupTime(void)
{
    // Return custom wakeup time for shutdown script
    if (wakeup)
    {
        time_t Now=time(NULL);
        time_t Time=cTimer::SetTime(Now,cTimer::TimeToInt(exectime));
        Time-=180;
        if (Time<=Now)
            Time=cTimer::IncDay(Time,1);
        return Time;
    }
    else
    {
        return 0;
    }
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

        cEPGMapping *map = new cEPGMapping(&Name[8],Value);
        epgmappings.Add(map);
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
            cTEXTMapping *textmap = new cTEXTMapping(&Name[8],Value);
            textmappings.Add(textmap);
        }
    }
    else if (!strcasecmp(Name,"options.exectime"))
    {
        SetExecTime(atoi(Value));
    }
    else if (!strcasecmp(Name,"options.wakeup"))
    {
        wakeup=atoi(Value);
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
            if (executeepgsources())
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
