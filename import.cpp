/*
 * import.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <sqlite3.h>
#include <ctype.h>
#include <time.h>
#include <locale.h>
#include <langinfo.h>
#include <pcrecpp.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <vdr/channels.h>

#include "xmltv2vdr.h"
#include "import.h"
#include "event.h"

extern char *strcatrealloc(char *, const char*);

struct cImport::split cImport::split(char *in, char delim)
{
    struct split sp;
    sp.count=1;
    sp.pointers[0]=in;
    while (*++in)
    {
        if (*in==delim)
        {
            *in=0;
            sp.pointers[sp.count++]=in+1;
        }
    }
    return sp;
}

char *cImport::RemoveNonASCII(const char *src)
{
    if (!src) return NULL;
    int len=strlen(src);
    if (!len) return NULL;
    char *dst=(char *) malloc(len+1);
    if (!dst) return NULL;
    char *tmp=dst;
    bool lspc=false;
    while (*src)
    {
        // 0x20,0x30-0x39,0x41-0x5A,0x61-0x7A
        if ((*src==0x20) && (!lspc))
        {
            *tmp++=0x20;
            lspc=true;
        }
        if (*src==':')
        {
            *tmp++=0x20;
            lspc=true;
        }
        if ((*src>=0x30) && (*src<=0x39))
        {
            *tmp++=*src;
            lspc=false;
        }
        if ((*src>=0x41) && (*src<=0x5A))
        {
            *tmp++=tolower(*src);
            lspc=false;
        }
        if ((*src>=0x61) && (*src<=0x7A))
        {
            *tmp++=*src;
            lspc=false;
        }
        src++;
    }
    *tmp=0;
    return dst;
}

cEvent *cImport::SearchVDREvent(cSchedule* schedule, cXMLTVEvent *xevent)
{
    cEvent *f=NULL;

    // try to find an event,
    // 1st with our own EventID
    if (xevent->EITEventID()) f=(cEvent *) schedule->GetEvent(xevent->EITEventID());
    if (f) return f;

    if (xevent->EventID() && !xevent->Mixing()) f=(cEvent *) schedule->GetEvent(xevent->EventID());
    if (f) return f;

    // 2nd with StartTime
    f=(cEvent *) schedule->GetEvent((tEventID) 0,xevent->StartTime());
    if (f)
    {
        if (!strcmp(f->Title(),conv->Convert(xevent->Title())))
        {
            return f;
        }
    }
    // 3rd with StartTime +/- WaitTime
    int maxdiff=INT_MAX;
    int eventTimeDiff=0;
    if (xevent->Duration()) eventTimeDiff=xevent->Duration()/4;
    if (eventTimeDiff<780) eventTimeDiff=780;

    for (cEvent *p = schedule->Events()->First(); p; p = schedule->Events()->Next(p))
    {
        int diff=abs((int) difftime(p->StartTime(),xevent->StartTime()));
        if (diff<=eventTimeDiff)
        {
            // found event with exact the same title
            if (!strcmp(p->Title(),conv->Convert(xevent->Title())))
            {
                if (diff<=maxdiff)
                {
                    f=p;
                    maxdiff=diff;
                }
            }
            else
            {
                if (f) continue; // we already have an event!
                // cut both titles into pieces and check
                // if we have at least one match with
                // minimum length of 4 characters

                // first remove all non ascii characters
                // we just want the following codes
                // 0x20,0x30-0x39,0x41-0x5A,0x61-0x7A
                int wfound=0;
                char *s1=RemoveNonASCII(p->Title());
                char *s2=RemoveNonASCII(conv->Convert(xevent->Title()));
                if (s1 && s2)
                {
                    if (!strcmp(s1,s2))
                    {
                        wfound++;
                    }
                    else
                    {
                        struct split w1 = split(s1,' ');
                        struct split w2 = split(s2,' ');
                        if ((w1.count) && (w2.count))
                        {
                            for (int i1=0; i1<w1.count; i1++)
                            {
                                for (int i2=0; i2<w2.count; i2++)
                                {
                                    if (!strcmp(w1.pointers[i1],w2.pointers[i2]))
                                    {
                                        if (strlen(w1.pointers[i1])>3) wfound++;
                                    }
                                }
                            }
                        }
                    }
                }
                if (s1) free(s1);
                if (s2) free(s2);

                if (wfound)
                {
                    if (diff<=maxdiff)
                    {
                        if (p->TableID()!=0)
                            source->Dlog("found '%s' for '%s'",p->Title(),conv->Convert(xevent->Title()));
                        f=p;
                        maxdiff=diff;
                    }
                }
            }
        }
    }
    return f;
}

cEvent *cImport::GetEventBefore(cSchedule* schedule, time_t start)
{
    if (!schedule) return NULL;
    if (!schedule->Events()) return NULL;
    if (!schedule->Events()->Count()) return NULL;
    cEvent *last=schedule->Events()->Last();
    if ((last) && (last->StartTime()<start)) return last;
    for (cEvent *p=schedule->Events()->First(); p; p=schedule->Events()->Next(p))
    {
        if (p->StartTime()>start)
        {
            return (cEvent *) p->Prev();
        }
    }
    if (last) return last;
    return NULL;
}

char *cImport::RemoveLastCharFromDescription(char *description)
{
    if (description)
    {
        int len=strlen(description);
        if (!len) return description;
        description[len-1]=0;
    }
    return description;
}

char *cImport::Add2Description(char *description, const char *Value)
{
    description = strcatrealloc(description,Value);
    return description;
}

char *cImport::Add2Description(char *description, const char *Name, const char *Value)
{
    description = strcatrealloc(description,Name);
    description = strcatrealloc(description,": ");
    description = strcatrealloc(description,Value);
    description = strcatrealloc(description,"\n");
    return description;
}

char *cImport::Add2Description(char *description, const char *Name, int Value)
{
    char *value=NULL;
    if (asprintf(&value,"%i",Value)==-1) return false;
    description = strcatrealloc(description,Name);
    description = strcatrealloc(description,": ");
    description = strcatrealloc(description,value);
    description = strcatrealloc(description,"\n");
    free(value);
    return description;
}

char *cImport::AddEOT2Description(char *description)
{
    const char nbsp[]={0xc2,0xa0,0};
    description=strcatrealloc(description,nbsp);
    return description;
}

bool cImport::WasChanged(cEvent* Event)
{
    if (!Event) return false;
    if (!Event->Description()) return false;
    int len=strlen(Event->Description());
    if (len<1) return false;
    if ((uchar)(Event->Description()[len-1])==0xA0) return true;
    return false;
}

bool cImport::PutEvent(cEPGSource *source, sqlite3 *db, cSchedule* schedule,
                       cEvent *event, cXMLTVEvent *xevent,int Flags, int Option)
{
    if (!source) return false;
    if (!schedule) return false;
    if (!xevent) return false;

#define CHANGED_NOTHING     0
#define CHANGED_SHORTTEXT   1
#define CHANGED_DESCRIPTION 2
#define CHANGED_ALL         3

    struct tm tm;
    char from[80];
    char till[80];
    time_t start,end;

    int changed=CHANGED_NOTHING;
    bool append=false;
    if ((Flags & OPT_APPEND)==OPT_APPEND) append=true;

    if (append && !event)
    {
        start=xevent->StartTime();
        end=start+xevent->Duration();

        /* checking the "space" for our new event */
        cEvent *prev=GetEventBefore(schedule,start);
        if (prev)
        {
            if (cEvent *next=(cEvent *) prev->Next())
            {
                if (prev->EndTime()==next->StartTime())
                {
                    localtime_r(&start,&tm);
                    strftime(from,sizeof(from)-1,"%b %d %H:%M",&tm);
                    localtime_r(&end,&tm);
                    strftime(till,sizeof(till)-1,"%b %d %H:%M",&tm);
                    source->Elog("cannot add '%s'@%s-%s",xevent->Title(),from,till);

                    time_t pstart=prev->StartTime();
                    time_t pstop=prev->EndTime();
                    localtime_r(&pstart,&tm);
                    strftime(from,sizeof(from)-1,"%b %d %H:%M",&tm);
                    localtime_r(&pstop,&tm);
                    strftime(till,sizeof(till)-1,"%b %d %H:%M",&tm);
                    source->Elog("found '%s'@%s-%s",prev->Title(),from,till);

                    time_t nstart=next->StartTime();
                    time_t nstop=next->EndTime();
                    localtime_r(&nstart,&tm);
                    strftime(from,sizeof(from)-1,"%b %d %H:%M",&tm);
                    localtime_r(&nstop,&tm);
                    strftime(till,sizeof(till)-1,"%b %d %H:%M",&tm);
                    source->Elog("found '%s'@%s-%s",next->Title(),from,till);
                    return false;
                }

                if (end>next->StartTime())
                {
                    int diff=(int) difftime(prev->EndTime(),start);
                    if (diff>300)
                    {

                        localtime_r(&start,&tm);
                        strftime(from,sizeof(from)-1,"%b %d %H:%M",&tm);
                        localtime_r(&end,&tm);
                        strftime(till,sizeof(till)-1,"%b %d %H:%M",&tm);
                        source->Elog("cannot add '%s'@%s-%s",xevent->Title(),from,till);

                        time_t nstart=next->StartTime();
                        time_t nstop=next->EndTime();
                        localtime_r(&nstart,&tm);
                        strftime(from,sizeof(from)-1,"%b %d %H:%M",&tm);
                        localtime_r(&nstop,&tm);
                        strftime(till,sizeof(till)-1,"%b %d %H:%M",&tm);
                        source->Elog("found '%s'@%s-%s",next->Title(),from,till);
                        return false;
                    }
                    else
                    {
                        xevent->SetDuration(xevent->Duration()-diff);
                    }
                }
            }

            if (prev->EndTime()>start)
            {
                int diff=(int) difftime(prev->EndTime(),start);
                if (diff>300)
                {
                    localtime_r(&start,&tm);
                    strftime(from,sizeof(from)-1,"%b %d %H:%M",&tm);
                    localtime_r(&end,&tm);
                    strftime(till,sizeof(till)-1,"%b %d %H:%M",&tm);
                    source->Elog("cannot add '%s'@%s-%s",xevent->Title(),from,till);

                    time_t pstart=prev->StartTime();
                    time_t pstop=prev->EndTime();
                    localtime_r(&pstart,&tm);
                    strftime(from,sizeof(from)-1,"%b %d %H:%M",&tm);
                    localtime_r(&pstop,&tm);
                    strftime(till,sizeof(till)-1,"%b %d %H:%M",&tm);
                    source->Elog("found '%s'@%s-%s",prev->Title(),from,till);
                    return false;
                }
                else
                {
                    prev->SetDuration(prev->Duration()-diff);
                }
            }

            if (!xevent->Duration())
            {
                if (!prev->Duration())
                {
                    prev->SetDuration(start-prev->StartTime());
                }
            }
        }
        /* add event */
        event=new cEvent(xevent->EventID());
        if (!event) return false;
        event->SetStartTime(start);
        event->SetDuration(xevent->Duration());
        event->SetTitle(xevent->Title());
        event->SetVersion(0);
        event->SetTableID(0);
        schedule->AddEvent(event);
        schedule->Sort();
        localtime_r(&start,&tm);
        strftime(from,sizeof(from)-1,"%b %d %H:%M",&tm);
        localtime_r(&end,&tm);
        strftime(till,sizeof(till)-1,"%b %d %H:%M",&tm);
        source->Dlog("adding '%s'@%s-%s",xevent->Title(),from,till);
    }
    else
    {
        append=false;
    }

    if (!event) return false;

    if (((Flags & USE_SHORTTEXT)==USE_SHORTTEXT) || (append))
    {
        if (xevent->ShortText() && (strlen(xevent->ShortText())>0))
        {
            if (!strcasecmp(xevent->ShortText(),event->Title()))
            {
                source->Dlog("title and subtitle equal, clearing subtitle");
                event->SetShortText("");
            }
            else
            {
                const char *dp=conv->Convert(xevent->ShortText());
                if (!event->ShortText() || strcmp(event->ShortText(),dp))
                {
                    event->SetShortText(dp);
                    changed|=CHANGED_SHORTTEXT; // shorttext really changed
                }
            }
        }
    }

    if (Option!=IMPORT_SHORTTEXT)
    {
        char *description=NULL;
        if (((Flags & USE_LONGTEXT)==USE_LONGTEXT) || ((Flags & OPT_APPEND)==OPT_APPEND))
        {
            if (xevent->Description() && (strlen(xevent->Description())>0))
            {
                description=strdup(xevent->Description());
            }
        }

        if (!description && xevent->EITDescription() && (strlen(xevent->EITDescription())>0))
        {
            description=strdup(xevent->EITDescription());
        }

        if (!description && event->Description() && (strlen(event->Description())>0))
        {
            if (WasChanged(event)) return true;
            UpdateXMLTVEvent(source,source->EPGFile(),db,event,source->Name(),xevent->EventID(),
                             event->EventID(),event->Description());
            description=strdup(event->Description());
        }

        if (description) description=Add2Description(description,"\n");

        if ((Flags & USE_CREDITS)==USE_CREDITS)
        {
            cXMLTVStringList *credits=xevent->Credits();
            if (credits->Size())
            {
                cTEXTMapping *oldtext=NULL;
                for (int i=0; i<credits->Size(); i++)
                {
                    char *ctype=strdup((*credits)[i]);
                    if (ctype)
                    {
                        char *cval=strchr(ctype,'|');
                        if (cval)
                        {
                            *cval=0;
                            cval++;
                            bool add=true;
                            if (((Flags & CREDITS_ACTORS)!=CREDITS_ACTORS) &&
                                    (!strcasecmp(ctype,"actor"))) add=false;
                            if (((Flags & CREDITS_DIRECTORS)!=CREDITS_DIRECTORS) &&
                                    (!strcasecmp(ctype,"director"))) add=false;
                            if (((Flags & CREDITS_OTHERS)!=CREDITS_OTHERS) &&
                                    (add) && (strcasecmp(ctype,"actor")) &&
                                    (strcasecmp(ctype,"director"))) add=false;
                            if (add)
                            {
                                cTEXTMapping *text=texts->GetMap(ctype);
                                if ((Flags & CREDITS_LIST)==CREDITS_LIST)
                                {
                                    if (oldtext!=text)
                                    {
                                        if (oldtext)
                                        {
                                            description=RemoveLastCharFromDescription(description);
                                            description=RemoveLastCharFromDescription(description);
                                            description=Add2Description(description,"\n");
                                        }
                                        description=Add2Description(description,text->Value());
                                        description=Add2Description(description,": ");
                                    }
                                    description=Add2Description(description,cval);
                                    description=Add2Description(description,", ");
                                }
                                else
                                {
                                    if (text)
                                    {
                                        description=Add2Description(description,text->Value(),cval);
                                    }
                                }
                                oldtext=text;
                            }
                        }
                        free(ctype);
                    }
                }
                if ((oldtext) && ((Flags & CREDITS_LIST)==CREDITS_LIST))
                {
                    description=RemoveLastCharFromDescription(description);
                    description=RemoveLastCharFromDescription(description);
                    description=Add2Description(description,"\n");
                }
            }
        }

        if ((Flags & USE_COUNTRYDATE)==USE_COUNTRYDATE)
        {
            if (xevent->Country())
            {
                cTEXTMapping *text=texts->GetMap("country");
                if (text) description=Add2Description(description,text->Value(),xevent->Country());
            }

            if (xevent->Year())
            {
                cTEXTMapping *text=texts->GetMap("year");
                if (text) description=Add2Description(description,text->Value(),xevent->Year());
            }
        }
        if (((Flags & USE_ORIGTITLE)==USE_ORIGTITLE) && (xevent->OrigTitle()))
        {
            cTEXTMapping *text=texts->GetMap("originaltitle");
            if (text) description=Add2Description(description,text->Value(),xevent->OrigTitle());
        }
        if (((Flags & USE_CATEGORIES)==USE_CATEGORIES) && (xevent->Category()->Size()))
        {
            cTEXTMapping *text=texts->GetMap("category");
            if (text)
            {
                cXMLTVStringList *categories=xevent->Category();
                description=Add2Description(description,text->Value(),(*categories)[0]);
                for (int i=1; i<categories->Size(); i++)
                {
                    // prevent duplicates
                    if (strcasecmp((*categories)[i],(*categories)[i-1]))
                        description=Add2Description(description,text->Value(),(*categories)[i]);
                }
            }
        }

        if (((Flags & USE_VIDEO)==USE_VIDEO) && (xevent->Video()->Size()))
        {
            cTEXTMapping *text=texts->GetMap("video");
            if (text)
            {
                description=Add2Description(description,text->Value());
                description=Add2Description(description,": ");
                cXMLTVStringList *video=xevent->Video();
                for (int i=0; i<video->Size(); i++)
                {
                    char *vtype=strdup((*video)[i]);
                    if (vtype)
                    {
                        char *vval=strchr(vtype,'|');
                        if (vval)
                        {
                            *vval=0;
                            vval++;

                            if (i)
                            {
                                description=Add2Description(description,", ");
                            }

                            if (!strcasecmp(vtype,"colour"))
                            {
                                if (!strcasecmp(vval,"no"))
                                {
                                    cTEXTMapping *text=texts->GetMap("blacknwhite");
                                    description=Add2Description(description,text->Value());
                                }
                            }
                            else
                            {
                                description=Add2Description(description,vval);
                            }
                        }
                        free(vtype);
                    }
                }
                description=Add2Description(description,"\n");
            }
        }

        if ((Flags & USE_AUDIO)==USE_AUDIO)
        {
            if (xevent->Audio())
            {
                cTEXTMapping *text=texts->GetMap("audio");
                if (text)
                {
                    description=Add2Description(description,text->Value());
                    description=Add2Description(description,": ");

                    if ((!strcasecmp(xevent->Audio(),"mono")) || (!strcasecmp(xevent->Audio(),"stereo")))
                    {
                        description=Add2Description(description,xevent->Audio());
                        description=Add2Description(description,"\n");
                    }
                    else
                    {
                        cTEXTMapping *text=texts->GetMap(xevent->Audio());
                        if (text)
                        {
                            description=Add2Description(description,text->Value());
                            description=Add2Description(description,"\n");
                        }
                    }
                }
            }
        }
        if ((Flags & USE_SEASON)==USE_SEASON)
        {
            if (xevent->Season())
            {
                cTEXTMapping *text=texts->GetMap("season");
                if (text) description=Add2Description(description,text->Value(),xevent->Season());
            }

            if (xevent->Episode())
            {
                cTEXTMapping *text=texts->GetMap("episode");
                if (text) description=Add2Description(description,text->Value(),xevent->Episode());
            }
        }

        if (((Flags & USE_RATING)==USE_RATING) && (xevent->Rating()->Size()))
        {
            cXMLTVStringList *rating=xevent->Rating();
            int rv=0;
            for (int i=0; i<rating->Size(); i++)
            {
                char *rtype=strdup((*rating)[i]);
                if (rtype)
                {
                    char *rval=strchr(rtype,'|');
                    if (rval)
                    {
                        *rval=0;
                        rval++;
#if VDRVERSNUM < 10711 && !EPGHANDLER
                        description=Add2Description(description,rtype);
                        description=Add2Description(description,": ");
                        description=Add2Description(description,rval);
                        description=Add2Description(description,"\n");
#else
                        if ((Flags & OPT_RATING_TEXT)==OPT_RATING_TEXT)
                        {
                            description=Add2Description(description,rtype);
                            description=Add2Description(description,": ");
                            description=Add2Description(description,rval);
                            description=Add2Description(description,"\n");
                        }
                        int r=atoi(rval);
                        if (r>rv) rv=r;
#endif
                    }
                    free(rtype);
                }
            }
#if VDRVERSNUM >= 10711 || EPGHANDLER
            if ((rv>0) && (rv<=18))
            {
                event->SetParentalRating(rv);
            }
#endif
        }

        if (((Flags & USE_STARRATING)==USE_STARRATING) && (xevent->StarRating()->Size()))
        {
            cTEXTMapping *text=texts->GetMap("starrating");
            if (text)
            {
                description=Add2Description(description,text->Value());
                description=Add2Description(description,": ");
                cXMLTVStringList *starrating=xevent->StarRating();
                for (int i=0; i<starrating->Size(); i++)
                {
                    char *rtype=strdup((*starrating)[i]);
                    if (rtype)
                    {
                        char *rval=strchr(rtype,'|');
                        if (rval)
                        {
                            *rval=0;
                            rval++;

                            if (i)
                            {
                                description=Add2Description(description,", ");
                            }
                            if (strcasecmp(rtype,"*"))
                            {
                                description=Add2Description(description,rtype);
                                description=Add2Description(description," ");
                            }
                            description=Add2Description(description,rval);
                        }
                        free(rtype);
                    }
                }
                description=Add2Description(description,"\n");
            }
        }

        if (((Flags & USE_REVIEW)==USE_REVIEW) && (xevent->Review()->Size()))
        {
            cTEXTMapping *text=texts->GetMap("review");
            if (text)
            {
                cXMLTVStringList *review=xevent->Review();
                for (int i=0; i<review->Size(); i++)
                {
                    description=Add2Description(description,text->Value(),(*review)[i]);
                }
            }
        }

        if (description)
        {
            description=RemoveLastCharFromDescription(description);
            description=AddEOT2Description(description);
            const char *dp=conv->Convert(description);
            if (!event->Description() || strcmp(event->Description(),dp))
            {
                event->SetDescription(dp);
                changed|=CHANGED_DESCRIPTION;
            }
            free(description);
        }
    }
#if VDRVERSNUM < 10726 && (!EPGHANDLER)
    event->SetTableID(0); // prevent EIT EPG to update this event
#endif
    if ((!append) && (changed))
    {
        start=event->StartTime();
        end=event->EndTime();
        localtime_r(&start,&tm);
        strftime(from,sizeof(from)-1,"%b %d %H:%M",&tm);
        localtime_r(&end,&tm);
        strftime(till,sizeof(till)-1,"%b %d %H:%M",&tm);
        switch (changed)
        {
        case CHANGED_SHORTTEXT:
            source->Dlog("changing shorttext of '%s'@%s-%s",event->Title(),from,till);
            break;
        case CHANGED_DESCRIPTION:
            source->Dlog("changing description of '%s'@%s-%s",event->Title(),from,till);
            break;
        case CHANGED_ALL:
            source->Dlog("changing stext+descr of '%s'@%s-%s",event->Title(),from,till);
            break;
        }
    }
    return true;
}

bool cImport::FetchXMLTVEvent(sqlite3_stmt *stmt, cXMLTVEvent *xevent)
{
    if (!stmt) return false;
    if (!xevent) return false;
    xevent->Clear();
    int cols=sqlite3_column_count(stmt);
    for (int col=0; col<cols; col++)
    {
        switch (col)
        {
        case 0:
            xevent->SetChannelID((const char *) sqlite3_column_text(stmt,col));
            break;
        case 1:
            xevent->SetEventID(sqlite3_column_int(stmt,col));
            break;
        case 2:
            xevent->SetStartTime(sqlite3_column_int(stmt,col));
            break;
        case 3:
            xevent->SetDuration(sqlite3_column_int(stmt,col));
            break;
        case 4:
            xevent->SetTitle((const char *) sqlite3_column_text(stmt,col));
            break;
        case 5:
            xevent->SetOrigTitle((const char *) sqlite3_column_text(stmt,col));
            break;
        case 6:
            xevent->SetShortText((const char *) sqlite3_column_text(stmt,col));
            break;
        case 7:
            xevent->SetDescription((const char *) sqlite3_column_text(stmt,col));
            break;
        case 8:
            xevent->SetCountry((const char *) sqlite3_column_text(stmt,col));
            break;
        case 9:
            xevent->SetYear(sqlite3_column_int(stmt,col));
            break;
        case 10:
            xevent->SetCredits((const char *) sqlite3_column_text(stmt,col));
            break;
        case 11:
            xevent->SetCategory((const char *) sqlite3_column_text(stmt,col));
            break;
        case 12:
            xevent->SetReview((const char *) sqlite3_column_text(stmt,col));
            break;
        case 13:
            xevent->SetRating((const char *) sqlite3_column_text(stmt,col));
            break;
        case 14:
            xevent->SetStarRating((const char *) sqlite3_column_text(stmt,col));
            break;
        case 15:
            xevent->SetVideo((const char *) sqlite3_column_text(stmt,col));
            break;
        case 16:
            xevent->SetAudio((const char *) sqlite3_column_text(stmt,col));
            break;
        case 17:
            xevent->SetSeason(sqlite3_column_int(stmt,col));
            break;
        case 18:
            xevent->SetEpisode(sqlite3_column_int(stmt,col));
            break;
        case 19:
            if (sqlite3_column_int(stmt,col)==1) xevent->SetMixing();
            break;
        case 20: // source
            xevent->SetSource((const char *) sqlite3_column_text(stmt,col));
            break;
        case 21: // eiteventid
            xevent->SetEITEventID(sqlite3_column_int(stmt,col));
            break;
        case 22: // eitdescription
            xevent->SetEITDescription((const char *) sqlite3_column_text(stmt,col));
            break;
        }
    }
    return true;
}

cXMLTVEvent *cImport::PrepareAndReturn(sqlite3 *db, char *sql, sqlite3_stmt *stmt)
{
    if (sqlite3_prepare_v2(db,sql,strlen(sql),&stmt,NULL)!=SQLITE_OK)
    {
        esyslog("epghander: failed to prepare %s",sql);
        free(sql);
        return NULL;
    }

    cXMLTVEvent *xevent=NULL;
    if (sqlite3_step(stmt)==SQLITE_ROW)
    {
        xevent = new cXMLTVEvent();
        FetchXMLTVEvent(stmt,xevent);
    }
    sqlite3_finalize(stmt);
    free(sql);
    return xevent;
}

cXMLTVEvent *cImport::AddXMLTVEvent(const char *EPGFile, const char *ChannelID, const cEvent *Event,
                                    const char *EITDescription)
{
    struct passwd pwd,*pwdbuf;
    char buf[1024],*epdir=NULL;
    iconv_t conv=(iconv_t) -1;
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
                conv=iconv_open("US-ASCII//TRANSLIT","UTF-8");
            }
        }
        else
        {
            epdir=NULL;
        }
    }
    if (!epdir) return NULL;
    if (conv==(iconv_t) -1)
    {
        free(epdir);
        return NULL;
    }

    int season,episode;
    if (!cParse::FetchSeasonEpisode(conv,epdir,Event->Title(),
                                    Event->ShortText(),season,episode))
    {
        free(epdir);
        iconv_close(conv);
        return NULL;
    }

    sqlite3 *db=NULL;
    if (sqlite3_open_v2(EPGFile,&db,SQLITE_OPEN_READWRITE,NULL)!=SQLITE_OK)
    {
        esyslog("epghandler: failed to open or create %s",EPGFile);
        free(epdir);
        iconv_close(conv);
        return NULL;
    }

    cXMLTVEvent *xevent = new cXMLTVEvent();
    if (!xevent)
    {
        sqlite3_close(db);
        esyslog("epghandler: out of memory");
        free(epdir);
        iconv_close(conv);
        return NULL;
    }

    xevent->SetTitle(Event->Title());
    xevent->SetStartTime(Event->StartTime());
    xevent->SetDuration(Event->Duration());
    xevent->SetShortText(Event->ShortText());
    xevent->SetEventID(Event->EventID());
    xevent->SetEITEventID(Event->EventID());
    xevent->SetDescription(EITDescription);
    xevent->SetEITDescription(EITDescription);
    xevent->SetSeason(season);
    xevent->SetEpisode(episode);

    const char *sql=xevent->GetSQL((const char *) EITSOURCE,99,ChannelID);
    char *errmsg;
    if (sqlite3_exec(db,sql,NULL,NULL,&errmsg)!=SQLITE_OK)
    {
        esyslog("epghandler: %s",errmsg);
        sqlite3_free(errmsg);
        delete xevent;
        xevent=NULL;
    }
    sqlite3_close(db);
    free(epdir);
    iconv_close(conv);
    return xevent;
}

void cImport::UpdateXMLTVEvent(cEPGSource *Source, const char *EPGFile, sqlite3 *Db, const cEvent *Event,
                               const char *SourceName, tEventID EventID, tEventID EITEventID,
                               const char *EITDescription)
{
    if (!Source) return;
    bool closedb=false;
    if (!Db)
    {
        if (sqlite3_open_v2(EPGFile,&Db,SQLITE_OPEN_READWRITE,NULL)!=SQLITE_OK)
        {
            Source->Elog("failed to open %s",EPGFile);
            return;
        }
        closedb=true;
    }

    char *sql=NULL;
    if (EITDescription)
    {
        char *eitdescription=strdup(EITDescription);
        if (!eitdescription)
        {
            Source->Elog("out of memory");
            if (closedb) sqlite3_close(Db);
            return;
        }

        string ed=eitdescription;

        int reps;
        reps=pcrecpp::RE("'").GlobalReplace("''",&ed);
        if (reps)
        {
            eitdescription=(char *) realloc(eitdescription,ed.size()+1);
            strcpy(eitdescription,ed.c_str());
        }

        if (asprintf(&sql,"update epg set eiteventid=%li, eitdescription='%s' where eventid=%li and src='%s'",
                     (long int) EITEventID,eitdescription,(long int) EventID,SourceName)==-1)
        {
            free(eitdescription);
            Source->Elog("out of memory");
            if (closedb) sqlite3_close(Db);
            return;
        }
        free(eitdescription);

        if (Event)
        {
            struct tm tm;
            char from[80];
            char till[80];
            time_t start,end;
            start=Event->StartTime();
            end=Event->EndTime();
            localtime_r(&start,&tm);
            strftime(from,sizeof(from)-1,"%b %d %H:%M",&tm);
            localtime_r(&end,&tm);
            strftime(till,sizeof(till)-1,"%b %d %H:%M",&tm);
            Source->Dlog("updating description of '%s'@%s-%s in db",Event->Title(),from,till);
        }
    }
    else
    {
        if (asprintf(&sql,"update epg set eiteventid=%li where eventid=%li and src='%s'",
                     (long int) EITEventID,(long int) EventID,SourceName)==-1)
        {
            Source->Elog("out of memory");
            if (closedb) sqlite3_close(Db);
            return;
        }
    }

    char *errmsg;
    if (sqlite3_exec(Db,sql,NULL,NULL,&errmsg)!=SQLITE_OK)
    {
        Source->Elog("%s -> %s",sql,errmsg);
        free(sql);
        sqlite3_free(errmsg);
        if (closedb) sqlite3_close(Db);
        return;
    }

    free(sql);
    if (closedb) sqlite3_close(Db);
    return;
}

cXMLTVEvent *cImport::SearchXMLTVEvent(const char *EPGFile, const char *ChannelID, const cEvent *Event)
{

    if (!Event) return NULL;
    if (!EPGFile) return NULL;

    cXMLTVEvent *xevent=NULL;
    sqlite3_stmt *stmt=NULL;
    sqlite3 *db=NULL;
    char *sql=NULL;

    if (sqlite3_open_v2(EPGFile,&db,SQLITE_OPEN_READONLY,NULL)!=SQLITE_OK)
    {
        esyslog("epghandler: failed to open %s",EPGFile);
        return NULL;
    }

    if (asprintf(&sql,"select channelid,eventid,starttime,duration,title,origtitle,shorttext,description," \
                 "country,year,credits,category,review,rating,starrating,video,audio,season,episode," \
                 "mixing,src,eiteventid,eitdescription from epg where eiteventid=%li and channelid='%s' " \
                 "order by srcidx asc limit 1",(long int) Event->EventID(),ChannelID)==-1)
    {
        sqlite3_close(db);
        esyslog("epghandler: out of memory");
        return NULL;
    }

    xevent=PrepareAndReturn(db,sql,stmt);
    if (xevent)
    {
        sqlite3_close(db);
        return xevent;
    }

    int eventTimeDiff=0;
    if (Event->Duration()) eventTimeDiff=Event->Duration()/4;
    if (eventTimeDiff<780) eventTimeDiff=780;

    char *sqltitle=strdup(Event->Title());
    if (!sqltitle)
    {
        sqlite3_close(db);
        esyslog("epghandler: out of memory");
        return NULL;
    }

    string st=sqltitle;

    int reps;
    reps=pcrecpp::RE("'").GlobalReplace("''",&st);
    if (reps)
    {
        char *tmp_sqltitle=(char *) realloc(sqltitle,st.size()+1);
        if (tmp_sqltitle)
        {
            sqltitle=tmp_sqltitle;
            strcpy(sqltitle,st.c_str());
        }
    }

    if (asprintf(&sql,"select channelid,eventid,starttime,duration,title,origtitle,shorttext,description," \
                 "country,year,credits,category,review,rating,starrating,video,audio,season,episode," \
                 "mixing,src,eiteventid,eitdescription,abs(starttime-%li) as diff from epg where " \
                 " (starttime>=%li and starttime<=%li) and title='%s' and channelid='%s' " \
                 " order by diff,srcidx asc limit 1",Event->StartTime(),Event->StartTime()-eventTimeDiff,
                 Event->StartTime()+eventTimeDiff,sqltitle,ChannelID)==-1)
    {
        free(sqltitle);
        sqlite3_close(db);
        esyslog("epghandler: out of memory");
        return NULL;
    }
    free(sqltitle);

    xevent=PrepareAndReturn(db,sql,stmt);
    if (xevent)
    {
        sqlite3_close(db);
        return xevent;
    }

    sqlite3_close(db);
    return NULL;
}

int cImport::Process(cEPGExecutor &myExecutor)
{
    if (!source) return 0;
    time_t begin=time(NULL);
    time_t end=begin+(source->DaysInAdvance()*86400);
#if VDRVERSNUM < 10726 && (!EPGHANDLER)
    time_t endoneday=begin+86400;
#endif
    const cSchedules *schedules=NULL;
    cSchedulesLock *schedulesLock=NULL;
    int l=0;
    while (l<300)
    {
        if (schedulesLock) delete schedulesLock;
        schedulesLock = new cSchedulesLock(true,200); // wait up to 60 secs for lock!
        schedules = cSchedules::Schedules(*schedulesLock);
        if (schedules) break;
        if (!myExecutor.StillRunning())
        {
            delete schedulesLock;
            source->Ilog("request to stop from vdr");
            return 0;
        }
        l++;
    }

    sqlite3 *db=NULL;
    if (sqlite3_open_v2(source->EPGFile(),&db,SQLITE_OPEN_READWRITE,NULL)!=SQLITE_OK)
    {
        source->Elog("failed to open %s",source->EPGFile());
        delete schedulesLock;
        return 141;
    }

    char *sql;
    if (asprintf(&sql,"select channelid,eventid,starttime,duration,title,origtitle,shorttext,description," \
                 "country,year,credits,category,review,rating,starrating,video,audio,season,episode," \
                 "mixing,src,eiteventid,eitdescription from epg where (starttime > %li or " \
                 " (starttime + duration) > %li) and (starttime + duration) < %li "\
                 " and src='%s'",begin,begin,end,source->Name())==-1)
    {
        sqlite3_close(db);
        source->Elog("out of memory");
        delete schedulesLock;
        return 134;
    }

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db,sql,strlen(sql),&stmt,NULL)!=SQLITE_OK)
    {
        sqlite3_close(db);
        source->Elog("failed to prepare %s",sql);
        free(sql);
        delete schedulesLock;
        return 141;
    }
    free(sql);

    int lerr=0;
    for (;;)
    {
        if (sqlite3_step(stmt)==SQLITE_ROW)
        {
            cXMLTVEvent xevent;
            if (FetchXMLTVEvent(stmt,&xevent))
            {
                cEPGMapping *map=maps->GetMap(xevent.ChannelID());
                if (!map)
                {
                    if (lerr!=IMPORT_NOMAPPING)
                        source->Elog("no mapping for channelid %s",xevent.ChannelID());
                    lerr=IMPORT_NOMAPPING;
                    continue;
                }

                bool addevents=false;
                if ((map->Flags() & OPT_APPEND)==OPT_APPEND) addevents=true;

                for (int i=0; i<map->NumChannelIDs(); i++)
                {
                    cChannel *channel=Channels.GetByChannelID(map->ChannelIDs()[i]);
                    if (!channel)
                    {
                        if (lerr!=IMPORT_NOCHANNEL)
                            source->Elog("channel %s not found in channels.conf",
                                         *map->ChannelIDs()[i].ToString());
                        lerr=IMPORT_NOCHANNEL;
                        continue;
                    }
                    cSchedule* schedule = (cSchedule *) schedules->GetSchedule(channel,addevents);
                    if (!schedule)
                    {
                        if (lerr!=IMPORT_NOSCHEDULE)
                            source->Elog("cannot get schedule for channel %s%s",
                                         channel->Name(),addevents ? "" : " - try add option");
                        lerr=IMPORT_NOSCHEDULE;
                        continue;
                    }

                    cEvent *event=SearchVDREvent(schedule, &xevent);

                    if (addevents && event && (event->EventID() != xevent.EventID()))
                    {
                        source->Elog("found another event with different eventid");
                        int newflags=map->Flags();
                        newflags &=~OPT_APPEND;
                        map->ChangeFlags(newflags);
                    }

#if VDRVERSNUM < 10726 && (!EPGHANDLER)
                    if ((!addevents) && (xevent.StartTime()>endoneday)) continue;
#endif
                    PutEvent(source, db, schedule, event, &xevent, map->Flags());
                }
            }
        }
        else
        {
            break;
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    delete schedulesLock;
    return 0;
}

cImport::cImport(cEPGSource *Source, cEPGMappings* Maps, cTEXTMappings *Texts)
{
    maps=Maps;
    source=Source;
    texts=Texts;
    if (source) epgfile=source->EPGFile();

    char *CodeSet=NULL;
    if (setlocale(LC_CTYPE,""))
        CodeSet=nl_langinfo(CODESET);
    else
    {
        char *LangEnv=getenv("LANG");
        if (LangEnv)
        {
            CodeSet=strchr(LangEnv,'.');
            if (CodeSet)
                CodeSet++;
        }
    }
    if (source) source->Dlog("vdr codeset is '%s'",CodeSet ? CodeSet : "US-ASCII//TRANSLIT");
    conv = new cCharSetConv("UTF-8",CodeSet ? CodeSet : "US-ASCII//TRANSLIT");
}

cImport::~cImport()
{
    delete conv;
}
