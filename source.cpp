/*
 * source.cpp: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <vdr/timers.h>
#include <time.h>
#include <string.h>
#include <sys/ioctl.h>

#include "xmltv2vdr.h"
#include "source.h"
#include "extpipe.h"

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

cEPGExecutor::cEPGExecutor(cPluginXmltv2vdr *Plugin,cEPGSources *Sources) : cThread("xmltv2vdr importer")
{
    baseplugin=Plugin;
    sources=Sources;
}

void cEPGExecutor::Action()
{
    if (!sources) return;
    if (!baseplugin) return;

    for (cEPGSource *epgs=sources->First(); epgs; epgs=sources->Next(epgs))
    {
        if (epgs->RunItNow())
        {
            // if timer thread is runnung, wait till it's stopped
            baseplugin->Wait4TimerThread();

            int retries=0;
            while (retries<=2)
            {
                int ret=epgs->Execute(*this);
                if ((ret>0) && (ret<126) && (retries<2))
                {
                    epgs->Dlog("waiting 60 seconds");
                    int l=0;
                    while (l<300)
                    {
                        struct timespec req;
                        req.tv_sec=0;
                        req.tv_nsec=200000000; // 200ms
                        nanosleep(&req,NULL);
                        if (!Running())
                        {
                            epgs->Ilog("request to stop from vdr");
                            return;
                        }
                        l++;
                    }
                    retries++;
                }
                if ((retries==2) || (!ret)) break;
            }
            if (retries>=2) epgs->Elog("skipping after %i retries",retries);
        }
    }

    int ret=1;
    for (cEPGSource *epgs=sources->First(); epgs; epgs=sources->Next(epgs))
    {
        if (!epgs->LastRetCode())
        {
            ret=epgs->Import(*this);
            break; // only import from the first successful source!
        }
    }
    if (!ret) cSchedules::Cleanup(true);
}

// -------------------------------------------------------------

cEPGSource::cEPGSource(const char *Name, const char *ConfDir, const char *EPGFile,
                       cEPGMappings *Maps, cTEXTMappings *Texts)
{
    if (strcmp(Name,EITSOURCE))
    {
        dsyslog("xmltv2vdr: '%s' added epgsource",Name);
    }
    name=strdup(Name);
    confdir=ConfDir;
    epgfile=EPGFile;
    pin=NULL;
    Log=NULL;
    loglen=0;
    usepipe=false;
    needpin=false;
    running=false;
    upstartdone=false;
    daysinadvance=0;
    exec_upstart=1;
    exec_time=0;
    exec_weekday=127; // Mon->Sun
    lastretcode=255;
    lastexec=(time_t) 0;
    disabled=false;
    if (strcmp(Name,EITSOURCE))
    {
        ready2parse=ReadConfig();
        parse=new cParse(this, Maps);
        import=new cImport(this,Maps,Texts);
        Dlog("is%sready2parse",(ready2parse && parse) ? " " : " not ");
    }
    else
    {
        ready2parse=false;
        disabled=true;
        parse=NULL;
        import=NULL;
    }
}

cEPGSource::~cEPGSource()
{
    if (strcmp(name,EITSOURCE))
    {
        dsyslog("xmltv2vdr: '%s' epgsource removed",name);
    }
    free((void *) name);
    if (pin) free((void *) pin);
    if (Log) free((void *) Log);
    if (parse) delete parse;
    if (import) delete import;
}

time_t cEPGSource::NextRunTime()
{
    if (disabled) return 0; // never!
    if (exec_upstart) return 0; // only once!

    time_t t,now=time(NULL);
    t=cTimer::SetTime(now,cTimer::TimeToInt(exec_time));

    while ((exec_weekday & (1<<cTimer::GetWDay(t)))==0)
    {
        t=cTimer::IncDay(t,1);
    }

    if (t<now) t=cTimer::IncDay(t,1);
    return t;
}

bool cEPGSource::RunItNow()
{
    if (disabled) return false;
    struct stat statbuf;
    if (stat(epgfile,&statbuf)==-1) return true; // no database? -> execute immediately
    if (!statbuf.st_size) return true; // no database? -> execute immediately

    if (exec_upstart)
    {
        if (upstartdone) return false;
        return true;
    }
    else
    {
        time_t t=time(NULL);
        time_t nrt=NextRunTime();
        if (!nrt) return false;
        if ((t>nrt) && t<(nrt+5)) return true;
        return false;
    }
}

bool cEPGSource::ReadConfig()
{
    char *fname=NULL;
    if (asprintf(&fname,"%s/%s",EPGSOURCES,name)==-1)
    {
        Elog("out of memory");
        return false;
    }
    FILE *f=fopen(fname,"r");
    if (!f)
    {
        Elog("cannot read config file %s",fname);
        free(fname);
        return false;
    }
    Dlog("reading source config");
    size_t lsize;
    char *line=NULL;
    int linenr=1;
    while (getline(&line,&lsize,f)!=-1)
    {
        if (linenr==1)
        {
            if (!strncmp(line,"pipe",4))
            {
                Dlog("is providing data through a pipe");
                usepipe=true;
            }
            else
            {
                Dlog("is providing data through a file");
                usepipe=false;
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
                  if (!newdatatime) Dlog("updates source data @%02i:%02i",1,2);
                */
                if (pn)
                {
                    pn=compactspace(pn);
                    if (pn[0]=='1')
                    {
                        Dlog("is needing a pin");
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
            Dlog("daysmax=%i",daysmax);
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
            if (!strchr(cname,' ') && (strlen(cname)>0))
            {
                cEPGChannel *epgchannel= new cEPGChannel(cname,false);
                if (epgchannel) channels.Add(epgchannel);
            }
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
        Elog("out of memory");
        return false;
    }
    f=fopen(fname,"r+");
    if (!f)
    {
        if (errno!=ENOENT)
        {
            Elog("cannot read config file %s",fname);
            free(fname);
            return true;
        }
        /* still no config? -> ok */
        free(fname);
        return true;
    }
    Dlog("reading plugin config");
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
                Dlog("pin set");
            }
        }
        if (linenr==2)
        {
            sscanf(line,"%2d;%1d;%3d;%10d",&daysinadvance,&exec_upstart,&exec_weekday,&exec_time);
            Dlog("daysinadvance=%i",daysinadvance);
            Dlog("upstart=%i",exec_upstart);
            if (!exec_upstart)
            {
                Dlog("weekdays=%s",*cTimer::PrintDay(0,exec_weekday,true));
                time_t nrt=NextRunTime();
                Dlog("nextrun on %s",ctime(&nrt));
            }
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

int cEPGSource::ReadOutput(char *&result, size_t &l)
{
    int ret=0;
    char *fname=NULL;
    if (asprintf(&fname,"%s/%s.xmltv",EPGSOURCES,name)==-1)
    {
        Elog("out of memory");
        return 134;
    }
    Dlog("reading from '%s'",fname);

    int fd=open(fname,O_RDONLY);
    if (fd==-1)
    {
        Elog("failed to open '%s'",fname);
        free(fname);
        return 157;
    }

    struct stat statbuf;
    if (fstat(fd,&statbuf)==-1)
    {
        Elog("failed to stat '%s'",fname);
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
        Elog("out of memory");
        return 134;
    }
    if (read(fd,result,statbuf.st_size)!=statbuf.st_size)
    {
        Elog("failed to read '%s'",fname);
        ret=149;
        free(result);
        result=NULL;
    }
    close(fd);
    free(fname);
    return ret;
}

int cEPGSource::Import(cEPGExecutor &myExecutor)
{
    Dlog("importing from db");
    return import->Process(myExecutor);
}

int cEPGSource::Execute(cEPGExecutor &myExecutor)
{
    if (!ready2parse) return false;
    if (!parse) return false;
    char *r_out=NULL;
    char *r_err=NULL;
    int l_out=0;
    int l_err=0;
    int ret=0;

    if ((Log) && (lastexec))
    {
        free(Log);
        Log=NULL;
        loglen=0;
    }

    char *cmd=NULL;
    if (asprintf(&cmd,"%s %i '%s'",name,daysinadvance,pin ? pin : "")==-1)
    {
        Elog("out of memory");
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
                Elog("out of memory");
                return 134;
            }
            cmd=ncmd;
            strcat(cmd," ");
            strcat(cmd,channels.Get(x)->Name());
            strcat(cmd," ");
        }
    }
    char *pcmd=strdup(cmd);
    if (pcmd)
    {
        char *pa=strchr(pcmd,'\'');
        char *pe=strchr(pa+1,'\'');
        if (pa && pe)
        {
            pa++;
            for (char *c=pa; c<pe; c++)
            {
                if (c==pa)
                {
                    *c='X';
                }
                else
                {
                    *c='@';
                }
            }
            pe=pcmd;
            while (*pe)
            {
                if (*pe=='@')
                {
                    memmove(pe,pe+1,strlen(pe));
                }
                else
                {
                    pe++;
                }
            }
            Ilog("%s",pcmd);
        }
        free(pcmd);
    }
    cExtPipe p;
    if (!p.Open(cmd))
    {
        free(cmd);
        Elog("failed to open pipe");
        return 141;
    }
    free(cmd);
    Dlog("executing epgsource");
    running=true;

    int fdsopen=2;
    while (fdsopen>0)
    {
        struct pollfd fds[2];
        fds[0].fd=p.Out();
        fds[0].events=POLLIN;
        fds[1].fd=p.Err();
        fds[1].events=POLLIN;
        if (poll(fds,2,500)>=0)
        {
            if (fds[0].revents & POLLIN)
            {
                int n;
                if (ioctl(p.Out(),FIONREAD,&n)<0)
                {
                    n=1;
                }
                char *tmp=(char *) realloc(r_out, l_out+n+1);
                if (tmp)
                {
                    r_out=tmp;
                    int l=read(p.Out(),r_out+l_out,n);
                    if (l>0)
                    {
                        l_out+=l;
                    }
                }
                else
                {
                    free(r_out);
                    r_out=NULL;
                    l_out=0;
                    break;
                }
            }
            if (fds[1].revents & POLLIN)
            {
                int n;
                if (ioctl(p.Err(),FIONREAD,&n)<0)
                {
                    n=1;
                }
                char *tmp=(char *) realloc(r_err, l_err+n+1);
                if (tmp)
                {
                    r_err=tmp;
                    int l=read(p.Err(),r_err+l_err,n);
                    if (l>0)
                    {
                        l_err+=l;
                    }
                }
                else
                {
                    free(r_err);
                    r_err=NULL;
                    l_err=0;
                    break;
                }
            }
            if (fds[0].revents & POLLHUP)
            {
                fdsopen--;
            }
            if (fds[1].revents & POLLHUP)
            {
                fdsopen--;
            }
            if (!myExecutor.StillRunning())
            {
                int status;
                p.Close(status);
                if (r_out) free(r_out);
                if (r_err) free(r_err);
                Ilog("request to stop from vdr");
                running=false;
                return 0;
            }
        }
        else
        {
            Elog("failed polling");
            break;
        }
    }
    if (r_out) r_out[l_out]=0;
    if (r_err) r_err[l_err]=0;

    if (usepipe)
    {
        int status;
        if (p.Close(status)>0)
        {
            int returncode=WEXITSTATUS(status);
            if ((!returncode) && (r_out))
            {
                Dlog("parsing output");
                ret=parse->Process(myExecutor,r_out,l_out);
            }
            else
            {
                Elog("epgsource returned %i",returncode);
                ret=returncode;
            }
        }
        else
        {
            Elog("failed to execute");
            ret=126;
        }
    }
    else
    {
        int status;
        if (p.Close(status)>0)
        {
            int returncode=WEXITSTATUS(status);
            if (!returncode)
            {
                size_t l;
                char *result=NULL;
                ret=ReadOutput(result,l);
                if ((!ret) && (result))
                {
                    Dlog("parsing output");
                    ret=parse->Process(myExecutor,result,l);
                }
                if (result) free(result);
            }
            else
            {
                Elog("epgsource returned %i",returncode);
                ret=returncode;
            }
        }
    }
    if (r_out) free(r_out);

    if (!ret)
    {
        lastexec=time(NULL);
        upstartdone=true;
        lastretcode=ret;
    }

    if (r_err)
    {
        char *saveptr;
        char *pch=strtok_r(r_err,"\n",&saveptr);
        char *last=(char *) "";
        while (pch)
        {
            if (strcmp(last,pch))
            {
                Elog("%s",pch);
                last=pch;
            }
            pch=strtok_r(NULL,"\n",&saveptr);
        }
        free(r_err);
    }
    running=false;
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
        Elog("out of memory");
        free(fname1);
        return;
    }

    FILE *w=fopen(fname2,"w+");
    if (!w)
    {
        Elog("cannot create %s",fname2);
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
    fprintf(w,"%i;%i;%i;%i\n",daysinadvance,exec_upstart,exec_weekday,exec_time);
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

void cEPGSource::add2Log(const char Prefix, const char *line)
{
    if (!line) return;

    struct tm tm;
    time_t now=time(NULL);
    localtime_r(&now,&tm);
    char dt[30];
    strftime(dt,sizeof(dt)-1,"%H:%M ",&tm);

    loglen+=strlen(line)+3+strlen(dt);
    char *nptr=(char *) realloc(Log,loglen);
    if (nptr)
    {
        if (!Log) nptr[0]=0;
        Log=nptr;
        char prefix[2];
        prefix[0]=Prefix;
        prefix[1]=0;
        strcat(Log,prefix);
        strcat(Log,dt);
        strcat(Log,line);
        strcat(Log,"\n");
        Log[loglen-1]=0;
    }
}

void cEPGSource::Elog(const char *format, ...)
{
    va_list ap;
    char fmt[255];
    if (snprintf(fmt,sizeof(fmt),"xmltv2vdr: '%s' ERROR %s",name,format)==-1) return;
    va_start(ap, format);
    char *ptr;
    if (vasprintf(&ptr,fmt,ap)==-1) return;
    va_end(ap);
    esyslog(ptr);
    add2Log('E',ptr+20+strlen(name));
    free(ptr);
}

void cEPGSource::Dlog(const char *format, ...)
{
    va_list ap;
    char fmt[255];
    if (snprintf(fmt,sizeof(fmt),"xmltv2vdr: '%s' %s",name,format)==-1) return;
    va_start(ap, format);
    char *ptr;
    if (vasprintf(&ptr,fmt,ap)==-1) return;
    va_end(ap);
    dsyslog(ptr);
    add2Log('D',ptr+14+strlen(name));
    free(ptr);
}

void cEPGSource::Ilog(const char *format, ...)
{
    va_list ap;
    char fmt[255];
    if (snprintf(fmt,sizeof(fmt),"xmltv2vdr: '%s' %s",name,format)==-1) return;
    va_start(ap, format);
    char *ptr;
    if (vasprintf(&ptr,fmt,ap)==-1) return;
    va_end(ap);
    isyslog(ptr);
    add2Log('I',ptr+14+strlen(name));
    free(ptr);
}

// -------------------------------------------------------------

bool cEPGSources::Exists(const char* Name)
{
    if (!Name) return false;
    if (!Count()) return false;
    for (int i=0; i<Count(); i++)
    {
        if (!strcmp(Name,Get(i)->Name())) return true;
    }
    return false;
}

cEPGSource *cEPGSources::GetSource(const char* Name)
{
    if (!Name) return NULL;
    if (!Count()) return NULL;
    for (int i=0; i<Count(); i++)
    {
        if (!strcmp(Name,Get(i)->Name())) return Get(i);
    }
    return NULL;
}

int cEPGSources::GetSourceIdx(const char* Name)
{
    if (!Name) return -1;
    if (!Count()) return -1;
    for (int i=0; i<Count(); i++)
    {
        if (!strcmp(Name,Get(i)->Name())) return i;
    }
    return -1;
}

void cEPGSources::Remove()
{
    cEPGSource *epgs;
    while ((epgs=Last())!=NULL)
    {
        Del(epgs);
    }
}

bool cEPGSources::RunItNow()
{
    if (!Count()) return false;
    for (int i=0; i<Count(); i++)
    {
        if (Get(i)->RunItNow()) return true;
    }
    return false;
}

time_t cEPGSources::NextRunTime()
{
    time_t next=(time_t) -1;
    if (!Count()) return (time_t) 0;

    for (int i=0; i<Count(); i++)
    {
        time_t nrt=Get(i)->NextRunTime();
        if (nrt)
        {
            if (nrt<next) next=nrt;
        }
    }
    if (next<0) next=(time_t) 0;
    return next;
}

void cEPGSources::ReadIn(const char *ConfDir, const char *EpgFile, cEPGMappings *EPGMappings,
                         cTEXTMappings *TextMappings, const char *SourceOrder, bool Reload)
{
    if (Reload) Remove();
    DIR *dir=opendir(EPGSOURCES);
    if (!dir) return;
    struct dirent *dirent;
    while (dirent=readdir(dir))
    {
        if (strchr(&dirent->d_name[0],'.')) continue;
        if (!Exists(dirent->d_name))
        {
            char *path=NULL;
            if (asprintf(&path,"%s/%s",EPGSOURCES,dirent->d_name)!=-1)
            {
                if (access(path,R_OK)!=-1)
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
                                Add(new cEPGSource(dirent->d_name,ConfDir,EpgFile,EPGMappings,TextMappings));
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

    if (!Exists(EITSOURCE))
    {
        Add(new cEPGSource(EITSOURCE,ConfDir,EpgFile,EPGMappings,TextMappings));
    }

    if (!SourceOrder) return;
    char *buf=strdup(SourceOrder);
    if (!buf) return;
    char *saveptr;
    char *pch=strtok_r(buf,",",&saveptr);
    int newpos=0;
    while (pch)
    {
        bool disable=false;
        if (*pch=='-')
        {
            disable=true;
            pch++;
        }
        int oldpos=GetSourceIdx(pch);
        if (oldpos>-1)
        {
            if (disable)
            {
                isyslog("xmltv2vdr: disabling source '%s'",Get(oldpos)->Name());
                Get(oldpos)->Disable();
            }
            if (oldpos!=newpos) Move(oldpos,newpos);
            newpos++;
        }
        pch=strtok_r(NULL,",",&saveptr);
    }
    free(buf);
}


