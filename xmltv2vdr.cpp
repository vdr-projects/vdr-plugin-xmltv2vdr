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

cEPGExecutor::cEPGExecutor(cEPGSources *Sources) : cThread("xmltv2vdr importer")
{
    sources=Sources;
}

void cEPGExecutor::Action()
{
    if (!sources) return;
    int ret=0;
    for (cEPGSource *epgs=sources->First(); epgs; epgs=sources->Next(epgs))
    {
        int retries=0;
        while (retries<2)
        {
            ret=epgs->Execute();
            if ((ret>0) && (ret<126))
            {
                dsyslog("xmltv2vdr: '%s' waiting 60 seconds",epgs->Name());
                sleep(60);
                retries++;
            }
            else
            {
                break;
            }
        }
        if (retries>=2) esyslog("xmltv2vdr: '%s' ERROR skipping after %i retries",epgs->Name(),retries);
        if (!ret) break; // TODO: check if we must execute second/third source!
    }
    if (!ret) cSchedules::Cleanup(true);
}

// -------------------------------------------------------------

cEPGSource::cEPGSource(const char *Name, const char *ConfDir, cEPGMappings *Maps, cTEXTMappings *Texts)
{
    dsyslog("xmltv2vdr: '%s' added epgsource",Name);
    name=strdup(Name);
    confdir=strdup(ConfDir);
    pin=NULL;
    pipe=false;
    needpin=false;
    daysinadvance=0;
    ready2parse=ReadConfig();
    parse=new cParse(Name, Maps, Texts);
    dsyslog("xmltv2vdr: '%s' is%sready2parse",Name,(ready2parse && parse) ? " " : " not ");
}

cEPGSource::~cEPGSource()
{
    dsyslog("xmltv2vdr: '%s' epgsource removed",name);
    free((void *) name);
    free((void *) confdir);
    if (pin) free((void *) pin);
    if (parse) delete parse;
}

bool cEPGSource::ReadConfig()
{
    char *fname=NULL;
    if (asprintf(&fname,"%s/%s",EPGSOURCES,name)==-1)
    {
        esyslog("xmltv2vdr: '%s' out of memory",name);
        return false;
    }
    FILE *f=fopen(fname,"r+");
    if (!f)
    {
        esyslog("xmltv2vdr: '%s' ERROR cannot read config file %s",name,fname);
        free(fname);
        return false;
    }
    dsyslog("xmltv2vdr: '%s' reading source config",name);
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
            char *ndt=strchr(line,';');
            if (ndt)
            {
                *ndt=0;
                ndt++;
                char *pn=strchr(ndt,';');
                if (pn)
                {
                    *pn=0;
                    pn++;
                }
                /*
                  newdatatime=atoi(ndt);
                  if (!newdatatime) dsyslog("xmltv2vdr: '%s' updates source data @%02i:%02i",name,1,2);
                */
                if (pn)
                {
                    pn=compactspace(pn);
                    if (pn[0]=='1')
                    {
                        dsyslog("xmltv2vdr: '%s' is needing a pin",name);
                        needpin=true;
                    }
                }
            }
        }
        if (linenr==2)
        {
            char *semicolon=strchr(line,';');
            if (semicolon)
            {
                // backward compatibility
                *semicolon=0;
                semicolon++;
                daysmax=atoi(semicolon);
            }
            else
            {
                daysmax=atoi(line);
            }
            dsyslog("xmltv2vdr: '%s' daysmax=%i",name,daysmax);
        }
        if (linenr>2)
        {
            // channels
            char *semicolon=strchr(line,';');
            if (semicolon) *semicolon=0;
            char *lf=strchr(line,10);
            if (lf) *lf=0;
            char *cname=line;
            if (line[0]=='*')
            {
                // backward compatibility
                cname++;
            }
            cEPGChannel *epgchannel= new cEPGChannel(cname,false);
            if (epgchannel) channels.Add(epgchannel);
        }
        linenr++;
    }
    if (line) free(line);
    channels.Sort();
    fclose(f);
    free(fname);

    /* --------------- */

    if (asprintf(&fname,"%s/%s",confdir,name)==-1)
    {
        esyslog("xmltv2vdr: '%s' out of memory",name);
        return false;
    }
    f=fopen(fname,"r+");
    if (!f)
    {
        if (errno!=ENOENT)
        {
            esyslog("xmltv2vdr: '%s' ERROR cannot read config file %s",name,fname);
            free(fname);
            return true;
        }
        /* still no config? -> ok */
        free(fname);
        return true;
    }
    dsyslog("xmltv2vdr: '%s' reading plugin config",name);
    line=NULL;
    linenr=1;
    while (getline(&line,&lsize,f)!=-1)
    {
        if ((linenr==1) && (needpin))
        {
            char *lf=strchr(line,10);
            if (lf) *lf=0;
            if (strcmp(line,"#no pin"))
            {
                ChangePin(line);
                dsyslog("xmltv2vdr: '%s' pin set",name);
            }
        }
        if (linenr==2)
        {
            daysinadvance=atoi(line);
            dsyslog("xmltv2vdr: '%s' daysinadvance=%i",name,daysinadvance);
        }
        if (linenr>2)
        {
            // channels
            char *lf=strchr(line,10);
            if (lf) *lf=0;

            for (int x=0; x<channels.Count(); x++)
            {
                if (!strcmp(line,channels.Get(x)->Name()))
                {
                    channels.Get(x)->SetUsage(true);
                    break;
                }
            }
        }
        linenr++;
    }
    if (line) free(line);
    channels.Sort();
    fclose(f);
    free(fname);

    return true;
}

int cEPGSource::Execute()
{
    if (!ready2parse) return false;
    if (!parse) return false;
    char *result=NULL;
    int l=0;

    int ret=0;
    cExtPipe p;

    char *cmd=NULL;
    if (asprintf(&cmd,"%s %i '%s'",name,daysinadvance,pin ? pin : "")==-1)
    {
        esyslog("xmltv2vdr: '%s' ERROR out of memory",name);
        return 134;
    }

    for (int x=0; x<channels.Count(); x++)
    {
        if (channels.Get(x)->InUse())
        {
            int len=strlen(cmd);
            int clen=strlen(channels.Get(x)->Name());
            char *ncmd=(char *) realloc(cmd,len+clen+5);
            if (!ncmd)
            {
                free(cmd);
                esyslog("xmltv2vdr: '%s' ERROR out of memory",name);
                return 134;
            }
            cmd=ncmd;
            strcat(cmd," ");
            strcat(cmd,channels.Get(x)->Name());
            strcat(cmd," ");
        }
    }
    dsyslog("xmltv2vdr: '%s' %s",name,cmd);
    if (!p.Open(cmd,"r"))
    {
        free(cmd);
        esyslog("xmltv2vdr: '%s' ERROR failed to open pipe",name);
        return 141;
    }
    free(cmd);
    dsyslog("xmltv2vdr: '%s' executing epgsource",name);
    if (pipe)
    {
        int c;
        while ((c=fgetc(p.Out()))!=EOF)
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
                dsyslog("xmltv2vdr: '%s' parsing output",name);
                result[l]=0;
                if (!parse->Process(result,l))
                {
                    esyslog("xmltv2vdr: '%s' ERROR failed to parse output",name);
                    ret=141;
                }
            }
            else
            {
                esyslog("xmltv2vdr: '%s' ERROR epgsource returned %i",name,returncode);
                ret=returncode;
            }
        }
        else
        {
            esyslog("xmltv2vdr: '%s' ERROR failed to execute",name);
            ret=126;
        }
        if (result) free(result);
    }
    else
    {
        while ((fgetc(p.Out()))!=EOF) { }

        int status;
        if (p.Close(status)>0)
        {
            int returncode=WEXITSTATUS(status);
            if (!returncode)
            {
                char *fname=NULL;
                if (asprintf(&fname,"%s/%s.xmltv",EPGSOURCES,name)==-1)
                {
                    esyslog("xmltv2vdr: '%s' ERROR out of memory",name);
                    return 134;
                }
                dsyslog("xmltv2vdr: '%s' reading from '%s'",name,fname);

                int fd=open(fname,O_RDONLY);
                if (fd==-1)
                {
                    esyslog("xmltv2vdr: '%s' ERROR failed to open '%s'",name,fname);
                    free(fname);
                    return 157;
                }

                struct stat statbuf;
                if (fstat(fd,&statbuf)==-1)
                {
                    esyslog("xmltv2vdr: '%s' ERROR failed to stat '%s'",name,fname);
                    close(fd);
                    free(fname);
                    return 157;
                }
                l=statbuf.st_size;
                result=(char *) malloc(l+1);
                if (!result)
                {
                    close(fd);
                    free(fname);
                    esyslog("xmltv2vdr: '%s' ERROR out of memory",name);
                    return 134;
                }
                if (read(fd,result,statbuf.st_size)==statbuf.st_size)
                {
                    if (!parse->Process(result,l))
                    {
                        esyslog("xmltv2vdr: '%s' failed to parse output",name);
                        ret=149;
                    }
                }
                else
                {
                    esyslog("xmltv2vdr: '%s' ERROR failed to read '%s'",name,fname);
                    ret=149;
                }
                free(result);
                close(fd);
                free(fname);
            }
            else
            {
                esyslog("xmltv2vdr: '%s' ERROR epgsource returned %i",name,returncode);
                ret=returncode;
            }
        }
    }
    return ret;
}

void cEPGSource::ChangeChannelSelection(int *Selection)
{
    for (int i=0; i<channels.Count(); i++)
    {
        channels.Get(i)->SetUsage(Selection[i]);
    }
}

void cEPGSource::Store(void)
{
    char *fname1=NULL;
    char *fname2=NULL;
    if (asprintf(&fname1,"%s/%s",confdir,name)==-1) return;
    if (asprintf(&fname2,"%s/%s.new",confdir,name)==-1)
    {
        esyslog("xmltv2vdr: '%s' out of memory",name);
        free(fname1);
        return;
    }

    FILE *w=fopen(fname2,"w+");
    if (!w)
    {
        esyslog("xmltv2vdr: '%s' cannot create %s",name,fname2);
        unlink(fname2);
        free(fname1);
        free(fname2);
        return;
    }

    if (pin)
    {
        fprintf(w,"%s\n",pin);
    }
    else
    {
        fprintf(w,"#no pin\n");
    }
    fprintf(w,"%i\n",DaysInAdvance());
    for (int i=0; i<ChannelList()->Count(); i++)
    {
        if (ChannelList()->Get(i)->InUse())
        {
            fprintf(w,"%s\n",ChannelList()->Get(i)->Name());
        }
    }
    fclose(w);

    struct stat statbuf;
    if (stat(confdir,&statbuf)!=-1)
    {
        if (chown(fname2,statbuf.st_uid,statbuf.st_gid)) {}
    }

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

cEPGMapping *cPluginXmltv2vdr::EPGMapping(const char *ChannelName)
{
    if (!ChannelName) return NULL;
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
                                epgsources.Add(new cEPGSource(dirent->d_name,confdir,&epgmappings,&textmappings));
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
    if (exectime_t<=time(NULL)) exectime_t+=86000;
}

cPluginXmltv2vdr::cPluginXmltv2vdr(void) : epgexecutor(&epgsources)
{
    // Initialize any member variables here.
    // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
    // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
    confdir=NULL;
    WakeUp=0;
    UpStart=0;
    last_exectime_t=0;
    exectime=200;
    SetExecTime(exectime);
    TEXTMappingAdd(new cTEXTMapping("country",tr("country")));
    TEXTMappingAdd(new cTEXTMapping("date",tr("year")));
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
    confdir=strdup(ConfigDirectory(PLUGIN_NAME_I18N)); // creates internally the confdir!
    cParse::InitLibXML();
    ReadInEPGSources();
    if (UpStart)
    {
        exectime_t=time(NULL)+60;
    }
    else
    {
        exectime_t=time(NULL)-60;
        last_exectime_t=exectime_t;
    }
    return true;
}

void cPluginXmltv2vdr::Stop(void)
{
    // Stop any background activities the plugin is performing.
    removeepgsources();
    removeepgmappings();
    removetextmappings();
    cParse::CleanupLibXML();
    if (confdir)
    {
        free(confdir);
        confdir=NULL;
    }
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
        epgexecutor.Start();
        last_exectime_t=exectime_t;
        SetExecTime(exectime);
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
    if (WakeUp)
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
        WakeUp=atoi(Value);
    }
    else if (!strcasecmp(Name,"options.upstart"))
    {
        UpStart=atoi(Value);
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
