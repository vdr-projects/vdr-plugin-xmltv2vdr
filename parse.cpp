/*
 * parse.cpp: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <locale.h>
#include <langinfo.h>
#include <time.h>

#include "xmltv2vdr.h"
#include "parse.h"

extern char *strcatrealloc(char *dest, const char *src);

void cXMLTVEvent::SetTitle(const char *Title)
{
    title = strcpyrealloc(title, Title);
    if (title)
    {
        title = compactspace(title);
    }
}

void cXMLTVEvent::SetOrigTitle(const char *OrigTitle)
{
    origtitle = strcpyrealloc(origtitle, OrigTitle);
}

void cXMLTVEvent::SetShortText(const char *ShortText)
{
    shorttext=strcpyrealloc(shorttext,ShortText);
    if (shorttext) shorttext=compactspace(shorttext);
}

void cXMLTVEvent::SetDescription(const char *Description)
{
    description = strcpyrealloc(description, Description);
    if (description)
    {
        description = compactspace(description);
        if (description[strlen(description)-1]!='\n')
            description=strcatrealloc(description,"\n");
    }
}

bool cXMLTVEvent::RemoveLastCharFromDescription()
{
    if (!description) return false;
    int len=strlen(description);
    if (!len) return true;
    description[len-1]=0;
    return true;
}

bool cXMLTVEvent::Add2Description(const char *Value)
{
    description = strcatrealloc(description,Value);
    return (description!=NULL);
}


bool cXMLTVEvent::Add2Description(const char *Name, const char *Value)
{
    description = strcatrealloc(description,Name);
    description = strcatrealloc(description,": ");
    description = strcatrealloc(description,Value);
    description = strcatrealloc(description,"\n");
    return (description!=NULL);
}

bool cXMLTVEvent::Add2Description(const char *Name, int Value)
{
    char *value=NULL;
    if (asprintf(&value,"%i",Value)==-1) return false;
    description = strcatrealloc(description,Name);
    description = strcatrealloc(description,": ");
    description = strcatrealloc(description,value);
    description = strcatrealloc(description,"\n");
    free(value);
    return (description!=NULL);
}

void cXMLTVEvent::SetCountry(const char *Country)
{
    country=strcpyrealloc(country, Country);
    if (country) country=compactspace(country);
}

void cXMLTVEvent::SetReview(const char *Review)
{
    review=strcpyrealloc(review, Review);
    if (review) review=compactspace(review);
}

void cXMLTVEvent::SetRating(const char *System, const char *Rating)
{
    system=strcpyrealloc(system, System);
    if (system) system=compactspace(system);

    rating=strcpyrealloc(rating, Rating);
    if (rating) rating=compactspace(rating);
}

void cXMLTVEvent::AddCategory(const char *Category)
{
    categories.Append(compactspace(strdup(Category)));
    categories.Sort();
}

void cXMLTVEvent::AddCredits(const char *CreditType, const char *Credit, const char *Addendum)
{
    char *value=NULL;
    if (Addendum)
    {
        if (asprintf(&value,"%s|%s (%s)",CreditType,Credit,Addendum)==-1) return;
    }
    else
    {
        if (asprintf(&value,"%s|%s",CreditType,Credit)==-1) return;
    }
    credits.Append(value);
    credits.Sort();
}

void cXMLTVEvent::Clear()
{
    if (title)
    {
        free(title);
        title=NULL;
    }
    if (shorttext)
    {
        free(shorttext);
        shorttext=NULL;
    }
    if (description)
    {
        free(description);
        description=NULL;
    }
    if (country)
    {
        free(country);
        country=NULL;
    }
    if (system)
    {
        free(system);
        system=NULL;
    }
    if (review)
    {
        free(review);
        review=NULL;
    }
    if (rating)
    {
        free(rating);
        rating=NULL;
    }
    if (origtitle)
    {
        free(origtitle);
        origtitle=NULL;
    }
    year=0;
    starttime = 0;
    duration = 0;
    vps= (time_t) 0;
    eventid=0;
    credits.Clear();
    categories.Clear();
}

cXMLTVEvent::cXMLTVEvent()
{
    title=NULL;
    shorttext=NULL;
    description=NULL;
    country=NULL;
    system=NULL;
    review=NULL;
    rating=NULL;
    origtitle=NULL;
    Clear();
}

cXMLTVEvent::~cXMLTVEvent()
{
    Clear();
}

// -------------------------------------------------------

time_t cParse::ConvertXMLTVTime2UnixTime(char *xmltvtime)
{
    time_t offset=0;
    if (!xmltvtime) return (time_t) 0;
    char *withtz=strchr(xmltvtime,' ');
    int len;
    if (withtz)
    {
        len=strlen(xmltvtime)-(withtz-xmltvtime)-1;
        *withtz=':';
        if ((withtz[1]=='+') || (withtz[1]=='-'))
        {
            if (len==5)
            {
                int val=atoi(&withtz[1]);
                int h=val/100;
                int m=val-(h*100);
                offset=h*3600+m*60;
                setenv("TZ",":UTC",1);
            }
            else
            {
                setenv("TZ",":UTC",1);
            }
        }
        else
        {
            if (len>2)
            {
                setenv("TZ",withtz,1);
            }
            else
            {
                setenv("TZ",":UTC",1);
            }
        }
    }
    else
    {
        withtz=&xmltvtime[strlen(xmltvtime)];
        setenv("TZ",":UTC",1);
    }
    tzset();

    len=withtz-xmltvtime;
    if (len<4)
    {
        unsetenv("TZ");
        tzset();
        return (time_t) 0;
    }
    len-=2;
    char fmt[]="%Y%m%d%H%M%S";
    fmt[len]=0;

    struct tm tm;
    memset(&tm,0,sizeof(tm));
    if (!strptime(xmltvtime,fmt,&tm))
    {
        unsetenv("TZ");
        tzset();
        return (time_t) 0;
    }
    if (tm.tm_mday==0) tm.tm_mday=1;
    time_t ret=mktime(&tm);
    ret-=offset;
    unsetenv("TZ");
    tzset();
    return ret;
}

struct cParse::split cParse::split(char *in, char delim)
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

char *cParse::RemoveNonASCII(const char *src)
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

cEvent *cParse::SearchEvent(cSchedule* schedule, cXMLTVEvent *xevent)
{
    if (!xevent) return NULL;
    if (!xevent->StartTime()) return NULL;
    if (!xevent->Title()) return NULL;
    cEvent *f=NULL;

    time_t start=xevent->StartTime();
    if (!xevent->EventID())
    {
        xevent->SetEventID(DoSum((u_long) start,xevent->Title(),strlen(xevent->Title())));
    }

    // try to find an event,
    // 1st with our own EventID
    if (xevent->EventID()) f=(cEvent *) schedule->GetEvent((tEventID) xevent->EventID());
    if (f) return f;
    // 2nd with StartTime
    f=(cEvent *) schedule->GetEvent((tEventID) 0,start);
    if (f) return f;
    // 3rd with StartTime +/- WaitTime
    int maxdiff=INT_MAX;
    int eventTimeDiff=0;
    if (xevent->Duration()) eventTimeDiff=xevent->Duration()/4;
    if (eventTimeDiff<600) eventTimeDiff=600;
    for (cEvent *p = schedule->Events()->First(); p; p = schedule->Events()->Next(p))
    {
        int diff=abs((int) difftime(p->StartTime(),start));
        if (diff<=eventTimeDiff)
        {
            // found event with exact the same title
            if (!strcmp(p->Title(),xevent->Title()))
            {
                if (diff<=maxdiff)
                {
                    f=p;
                    maxdiff=diff;
                }
            }
            else
            {
                // cut both titles into pieces and check
                // if we have at least one match with
                // minimum length of 4 characters

                // first remove all non ascii characters
                // we just want the following codes
                // 0x20,0x30-0x39,0x41-0x5A,0x61-0x7A
                int wfound=0;
                char *s1=RemoveNonASCII(p->Title());
                char *s2=RemoveNonASCII(xevent->Title());
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
                    free(s1);
                    free(s2);
                }
                if (wfound)
                {
                    if (diff<=maxdiff)
                    {
                        if (p->TableID()!=0)
                            source->Dlog("found '%s' for '%s'",p->Title(),xevent->Title());
                        f=p;
                        maxdiff=diff;
                    }
                }
            }
        }
    }
    return f;
}

cEvent *cParse::GetEventBefore(cSchedule* schedule, time_t start)
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

bool cParse::PutEvent(cSchedule* schedule, cEvent *event, cXMLTVEvent *xevent, cEPGMapping *map)
{
    if (!schedule) return false;
    if (!xevent) return false;
    if (!map) return false;

    struct tm tm;
    char from[80];
    char till[80];
    time_t start,end;
    if (!event)
    {
        if ((map->Flags() & OPT_APPEND)==OPT_APPEND)
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
            event->SetShortText(xevent->ShortText());
            event->SetDescription(xevent->Description());
            event->SetVersion(0);
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
            return true;
        }
    }
    else
    {
        if (event->TableID()==0) return true;
        start=event->StartTime();
        end=event->EndTime();
        localtime_r(&start,&tm);
        strftime(from,sizeof(from)-1,"%b %d %H:%M",&tm);
        localtime_r(&end,&tm);
        strftime(till,sizeof(till)-1,"%b %d %H:%M",&tm);
        source->Dlog("changing '%s'@%s-%s",event->Title(),from,till);
    }
    if ((map->Flags() & USE_SHORTTEXT)==USE_SHORTTEXT)
    {
        if (xevent->ShortText() && (strlen(xevent->ShortText())>0))
        {
            if (!strcmp(xevent->ShortText(),event->Title()))
            {
                source->Dlog("title and subtitle equal, clearing subtitle");
                event->SetShortText("");
            }
            else
            {
                event->SetShortText(xevent->ShortText());
            }
        }
    }
    if ((map->Flags() & USE_LONGTEXT)==USE_LONGTEXT)
    {
        if (xevent->Description() && (strlen(xevent->Description())>0))
        {
            event->SetDescription(xevent->Description());
        }
        else
        {
            xevent->SetDescription(event->Description());
        }
    }
    else
    {
        xevent->SetDescription(event->Description());
    }
    bool addExt=false;
    if ((map->Flags() & USE_CREDITS)==USE_CREDITS)
    {
        cStringList *credits=xevent->Credits();
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
                        if (((map->Flags() & CREDITS_ACTORS)!=CREDITS_ACTORS) &&
                                (!strcasecmp(ctype,"actor"))) add=false;
                        if (((map->Flags() & CREDITS_DIRECTORS)!=CREDITS_DIRECTORS) &&
                                (!strcasecmp(ctype,"director"))) add=false;
                        if (((map->Flags() & CREDITS_OTHERS)!=CREDITS_OTHERS) &&
                                (add) && (strcasecmp(ctype,"actor")) &&
                                (strcasecmp(ctype,"director"))) add=false;
                        if (add)
                        {
                            cTEXTMapping *text=TEXTMapping(ctype);
                            if ((map->Flags() & CREDITS_LIST)==CREDITS_LIST)
                            {
                                if (oldtext!=text)
                                {
                                    if (oldtext)
                                    {
                                        addExt=xevent->RemoveLastCharFromDescription();
                                        addExt=xevent->RemoveLastCharFromDescription();
                                        addExt=xevent->Add2Description("\n");
                                    }
                                    addExt=xevent->Add2Description(text->Value());
                                    addExt=xevent->Add2Description(": ");
                                }
                                addExt=xevent->Add2Description(cval);
                                addExt=xevent->Add2Description(", ");
                            }
                            else
                            {
                                if (text)
                                {
                                    addExt=xevent->Add2Description(text->Value(),cval);
                                }
                            }
                            oldtext=text;
                        }
                    }
                    free(ctype);
                }
            }
            if ((oldtext) && ((map->Flags() & CREDITS_LIST)==CREDITS_LIST))
            {
                addExt=xevent->RemoveLastCharFromDescription();
                addExt=xevent->RemoveLastCharFromDescription();
                addExt=xevent->Add2Description("\n");
            }
        }
    }

    if ((map->Flags() & USE_COUNTRYDATE)==USE_COUNTRYDATE)
    {
        if (xevent->Country())
        {
            cTEXTMapping *text=TEXTMapping("country");
            if (text) addExt=xevent->Add2Description(text->Value(),xevent->Country());
        }

        if (xevent->Year())
        {
            cTEXTMapping *text=TEXTMapping("date");
            if (text) addExt=xevent->Add2Description(text->Value(),xevent->Year());
        }
    }
    if (((map->Flags() & USE_ORIGTITLE)==USE_ORIGTITLE) && (xevent->OrigTitle()))
    {
        cTEXTMapping *text=TEXTMapping("originaltitle");
        if (text) addExt=xevent->Add2Description(text->Value(),xevent->OrigTitle());
    }
    if (((map->Flags() & USE_CATEGORIES)==USE_CATEGORIES) && (xevent->Categories()->Size()))
    {
        cTEXTMapping *text=TEXTMapping("category");
        if (text)
        {
            cStringList *categories=xevent->Categories();
            addExt=xevent->Add2Description(text->Value(),(*categories)[0]);
            for (int i=1; i<categories->Size(); i++)
            {
                // prevent duplicates
                if (strcasecmp((*categories)[i],(*categories)[i-1]))
                    addExt=xevent->Add2Description(text->Value(),(*categories)[i]);
            }
        }
    }
    if (((map->Flags() & USE_RATING)==USE_RATING) && (xevent->Rating()) && (xevent->RatingSystem()))
    {
        addExt=xevent->Add2Description(xevent->RatingSystem(),xevent->Rating());
    }
    if (((map->Flags() & USE_REVIEW)==USE_REVIEW) && (xevent->Review()))
    {
        cTEXTMapping *text=TEXTMapping("review");
        if (text) addExt=xevent->Add2Description(text->Value(),xevent->Review());
    }
    if (addExt) event->SetDescription(xevent->Description());
    event->SetTableID(0); // prevent EIT EPG to update this event
    return true;
}

u_long cParse::DoSum(u_long sum, const char *buf, int nBytes)
{
    int nleft=nBytes;
    u_short *w = (u_short*)buf;

    while (nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1)
    {
        u_short answer = 0;
        *(u_char*)(&answer) = *(u_char*)w;
        sum += answer;
    }
    return sum;
}

bool cParse::FetchEvent(xmlNodePtr enode)
{
    char *slang=getenv("LANG");
    xmlNodePtr node=enode->xmlChildrenNode;
    while (node)
    {
        if (node->type==XML_ELEMENT_NODE)
        {
            if ((!xmlStrcasecmp(node->name, (const xmlChar *) "title")))
            {
                xmlChar *lang=xmlGetProp(node,(const xmlChar *) "lang");
                xmlChar *content=xmlNodeListGetString(node->doc,node->xmlChildrenNode,1);
                if (content)
                {
                    if (lang && slang && !xmlStrncasecmp(lang, (const xmlChar *) slang,2))
                    {
                        xevent.SetTitle(conv->Convert((const char *) content));
                    }
                    else
                    {
                        if (!xevent.Title())
                        {
                            xevent.SetTitle(conv->Convert((const char *) content));
                        }
                        else
                        {
                            xevent.SetOrigTitle(conv->Convert((const char *) content));
                        }
                    }
                    xmlFree(content);
                }
                if (lang) xmlFree(lang);
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "sub-title")))
            {
                // what to do with attribute lang?
                xmlChar *content=xmlNodeListGetString(node->doc,node->xmlChildrenNode,1);
                if (content)
                {
                    xevent.SetShortText(conv->Convert((const char *) content));
                    xmlFree(content);
                }
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "desc")))
            {
                // what to do with attribute lang?
                xmlChar *content=xmlNodeListGetString(node->doc,node->xmlChildrenNode,1);
                if (content)
                {
                    xevent.SetDescription(conv->Convert((const char *) content));
                    xmlFree(content);
                }
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "credits")))
            {
                xmlNodePtr vnode=node->xmlChildrenNode;
                while (vnode)
                {
                    if (vnode->type==XML_ELEMENT_NODE)
                    {
                        if ((!xmlStrcasecmp(vnode->name, (const xmlChar *) "actor")))
                        {
                            xmlChar *content=xmlNodeListGetString(vnode->doc,vnode->xmlChildrenNode,1);
                            if (content)
                            {
                                const char *role=NULL;
                                xmlChar *arole=xmlGetProp(node,(const xmlChar *) "actor role");
                                if (arole)
                                {
                                    role=strdup(conv->Convert((const char *) arole));
                                    xmlFree(arole);
                                }
                                xevent.AddCredits((const char *) vnode->name,conv->Convert((const char *) content),role);
                                if (role) free((void *) role);
                                xmlFree(content);
                            }
                        }
                        else
                        {
                            xmlChar *content=xmlNodeListGetString(vnode->doc,vnode->xmlChildrenNode,1);
                            if (content)
                            {
                                xevent.AddCredits((const char *) vnode->name,conv->Convert((const char *) content));
                                xmlFree(content);
                            }
                        }
                    }
                    vnode=vnode->next;
                }
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "date")))
            {
                xmlChar *content=xmlNodeListGetString(node->doc,node->xmlChildrenNode,1);
                if (content)
                {
                    xevent.SetYear(atoi((const char *) content));
                    xmlFree(content);
                }
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "category")))
            {
                // what to do with attribute lang?
                xmlChar *content=xmlNodeListGetString(node->doc,node->xmlChildrenNode,1);
                if (content)
                {
                    if (isdigit(content[0]))
                    {
                        xevent.SetEventID(atoi((const char *) content));
                    }
                    else
                    {
                        xevent.AddCategory(conv->Convert((const char *) content));
                    }
                    xmlFree(content);
                }
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "country")))
            {
                xmlChar *content=xmlNodeListGetString(node->doc,node->xmlChildrenNode,1);
                if (content)
                {
                    xevent.SetCountry(conv->Convert((const char *) content));
                    xmlFree(content);
                }
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "video")))
            {
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "audio")))
            {
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "rating")))
            {
                xmlChar *system=xmlGetProp(node,(const xmlChar *) "system");
                if (system)
                {
                    xmlNodePtr vnode=node->xmlChildrenNode;
                    while (vnode)
                    {
                        if (vnode->type==XML_ELEMENT_NODE)
                        {
                            if ((!xmlStrcasecmp(vnode->name, (const xmlChar *) "value")))
                            {
                                xmlChar *content=xmlNodeListGetString(vnode->doc,vnode->xmlChildrenNode,1);
                                if (content)
                                {
                                    const char *crating=strdup(conv->Convert((const char *) content));
                                    const char *csystem=strdup(conv->Convert((const char *) system));
                                    xevent.SetRating(csystem,crating);
                                    if (crating) free((void *) crating);
                                    if (csystem) free((void *) csystem);
                                    xmlFree(content);
                                }
                            }
                        }
                        vnode=vnode->next;
                    }
                    xmlFree(system);
                }
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "review")))
            {
                xmlChar *type=xmlGetProp(node,(const xmlChar *) "type");
                if (type && !xmlStrcasecmp(type, (const xmlChar *) "text"))
                {
                    xmlChar *content=xmlNodeListGetString(node->doc,node->xmlChildrenNode,1);
                    if (content)
                    {
                        xevent.SetReview(conv->Convert((const char *) content));
                        xmlFree(content);
                    }
                    xmlFree(type);
                }
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "icon")))
            {
                // http-link inside -> just ignore
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "length")))
            {
                // length without advertisements -> just ignore
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "episode-num")))
            {
                // episode-num in not usable format -> just ignore
            }
            else
            {
                source->Elog("unknown element %s, please report!",node->name);
            }
        }
        node=node->next;
    }
    return (xevent.Title()!=NULL);
}

cTEXTMapping *cParse::TEXTMapping(const char *Name)
{
    if (!texts->Count()) return NULL;
    for (cTEXTMapping *textmap=texts->First(); textmap; textmap=texts->Next(textmap))
    {
        if (!strcmp(textmap->Name(),Name)) return textmap;
    }
    return NULL;
}

cEPGMapping *cParse::EPGMapping(const char *ChannelName)
{
    if (!maps->Count()) return NULL;
    for (cEPGMapping *map=maps->First(); map; map=maps->Next(map))
    {
        if (!strcmp(map->ChannelName(),ChannelName)) return map;
    }
    return NULL;
}

int cParse::Process(cEPGExecutor &myExecutor,char *buffer, int bufsize)
{
    if (!buffer) return 134;
    if (!bufsize) return 134;

    xmlDocPtr xmltv;
    xmltv=xmlReadMemory(buffer,bufsize,NULL,NULL,0);
    if (!xmltv)
    {
        source->Elog("failed to parse xmltv");
        return 141;
    }

    xmlNodePtr rootnode=xmlDocGetRootElement(xmltv);
    if (!rootnode)
    {
        source->Elog("no rootnode in xmltv");
        xmlFreeDoc(xmltv);
        return 141;
    }

    const cSchedules *schedules=NULL;
    int l=0;
    while (l<300)
    {
        cSchedulesLock schedulesLock(true,200); // wait up to 60 secs for lock!
        schedules = cSchedules::Schedules(schedulesLock);
        if (!myExecutor.StillRunning())
        {
            source->Ilog("request to stop from vdr");
            return 0;
        }
        if (schedules) break;
        l++;
    }

    if (!schedules)
    {
        source->Elog("cannot get schedules now, trying later");
        return 1;
    }

    time_t begin=time(NULL);
    xmlNodePtr node=rootnode->xmlChildrenNode;
    cEPGMapping *oldmap=NULL;
    int lerr=0;
    while (node)
    {
        if (node->type!=XML_ELEMENT_NODE)
        {
            node=node->next;
            continue;
        }
        if ((xmlStrcasecmp(node->name, (const xmlChar *) "programme")))
        {
            node=node->next;
            continue;
        }
        xmlChar *channelid=xmlGetProp(node,(const xmlChar *) "channel");
        if (!channelid)
        {
            if (lerr!=PARSE_NOCHANNELID)
                source->Elog("missing channelid in xmltv file");
            lerr=PARSE_NOCHANNELID;
            node=node->next;
            continue;
        }
        cEPGMapping *map=EPGMapping((const char *) channelid);
        if (!map)
        {
            if (lerr!=PARSE_NOMAPPING)
                source->Elog("no mapping for channelid %s",channelid);
            lerr=PARSE_NOMAPPING;
            xmlFree(channelid);
            node=node->next;
            continue;
        }
        int days=map->Days();
        if ((map->Flags() & OPT_APPEND)!=OPT_APPEND) days=1; // only one day with merge
        time_t end=begin+(86000*days)+3600; // 1 hour overlap
        xmlChar *start,*stop;
        time_t starttime=(time_t) 0;
        time_t stoptime=(time_t) 0;
        start=xmlGetProp(node,(const xmlChar *) "start");
        if (start)
        {
            starttime=ConvertXMLTVTime2UnixTime((char *) start);
            if (starttime)
            {
                stop=xmlGetProp(node,(const xmlChar *) "stop");
                if (stop)
                {
                    stoptime=ConvertXMLTVTime2UnixTime((char *) stop);
                    xmlFree(stop);
                }
            }
            xmlFree(start);
        }

        if (!starttime)
        {
            if (lerr!=PARSE_XMLTVERR)
                source->Elog("xmltv2vdr: '%s' no starttime, check xmltv file");
            lerr=PARSE_XMLTVERR;
            xmlFree(channelid);
            node=node->next;
            continue;
        }

        if ((starttime<begin) || (starttime>end))
        {
            xmlFree(channelid);
            node=node->next;
            continue;
        }
        xevent.Clear();

        if (oldmap!=map)
        {
            source->Dlog("processing '%s'",channelid);
            source->Dlog("from %s",ctime_r(&begin,(char *) &cbuf));
            source->Dlog("till %s",ctime_r(&end,(char *) &cbuf));
        }
        oldmap=map;
        xmlFree(channelid);

        if ((map->Flags() & OPT_VPS)==OPT_VPS)
        {
            xmlChar *vpsstart=xmlGetProp(node,(const xmlChar *) "vps-start");
            if (vpsstart)
            {
                time_t vps=ConvertXMLTVTime2UnixTime((char *) vpsstart);
                xevent.SetVps(vps);
                xmlFree(vpsstart);
            }
        }

        xevent.SetStartTime(starttime);
        if (stoptime) xevent.SetDuration(stoptime-starttime);
        if (!FetchEvent(node)) // sets xevent
        {
            source->Dlog("failed to fetch event");
            node=node->next;
            continue;
        }
        for (int i=0; i<map->NumChannelIDs(); i++)
        {
            bool addevents=false;
            if ((map->Flags() & OPT_APPEND)==OPT_APPEND) addevents=true;

            cChannel *channel=Channels.GetByChannelID(map->ChannelIDs()[i]);
            if (!channel)
            {
                if (lerr!=PARSE_NOCHANNEL)
                    source->Elog("channel %s not found in channels.conf",
                                 *map->ChannelIDs()[i].ToString());
                lerr=PARSE_NOCHANNEL;
                continue;
            }
            cSchedule* schedule = (cSchedule *) schedules->GetSchedule(channel,addevents);
            if (!schedule)
            {
                if (lerr!=PARSE_NOSCHEDULE)
                    source->Elog("cannot get schedule for channel %s%s",
                                 channel->Name(),addevents ? "" : " - try add option");
                lerr=PARSE_NOSCHEDULE;
                continue;
            }
            if (addevents)
            {
                cEvent *event=SearchEvent(schedule,&xevent);
                if (!event)
                    PutEvent(schedule,event,&xevent,map);
            }
            else
            {
                if (!schedule->Events()->Count())
                {
                    if (lerr!=PARSE_EMPTYSCHEDULE)
                        source->Elog("cannot merge into empty epg (%s) - try add option",
                                     channel->Name());
                    lerr=PARSE_EMPTYSCHEDULE;
                }
                else
                {
                    cEvent *event=schedule->Events()->Last();
                    if (event->StartTime()>xevent.StartTime())
                    {
                        if ((event=SearchEvent(schedule,&xevent)))
                        {
                            PutEvent(schedule,event,&xevent,map);
                        }
                        else
                        {
                            time_t start=xevent.StartTime();
                            source->Elog("cannot find existing event in epg.data for xmltv-event %s@%s",
                                         xevent.Title(),ctime_r(&start,(char *) &cbuf));
                        }
                    }
                }
            }
        }
        node=node->next;
        if (!myExecutor.StillRunning())
        {
            xmlFreeDoc(xmltv);
            source->Ilog("request to stop from vdr");
            return 0;
        }
    }
    xmlFreeDoc(xmltv);
    return 0;
}

void cParse::InitLibXML()
{
    xmlInitParser();
}

void cParse::CleanupLibXML()
{
    xmlCleanupParser();
}

cParse::cParse(cEPGSource *Source, cEPGMappings *Maps, cTEXTMappings *Texts)
{
    source=Source;
    maps=Maps;
    texts=Texts;

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
    source->Dlog("vdr codeset is '%s'",CodeSet ? CodeSet : "US-ASCII//TRANSLIT");
    conv = new cCharSetConv("UTF-8",CodeSet ? CodeSet : "US-ASCII//TRANSLIT");
}

cParse::~cParse()
{
    delete conv;
}
