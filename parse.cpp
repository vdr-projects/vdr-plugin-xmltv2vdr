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

#include "parse.h"

extern char *strcatrealloc(char *dest, const char *src);

void cXMLTVEvent::SetTitle(const char *Title)
{
    title = strcpyrealloc(title, Title);
    if (title) title = compactspace(title);
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

bool cXMLTVEvent::Add2Description(const char *Value)
{
    description = strcatrealloc(description,Value);
    return (description);
}


bool cXMLTVEvent::Add2Description(const char *Name, const char *Value)
{
    description = strcatrealloc(description,Name);
    description = strcatrealloc(description,": ");
    description = strcatrealloc(description,Value);
    description = strcatrealloc(description,"\n");
    return (description);
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
    return (description);
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

void cXMLTVEvent::SetDirector(const char *Director)
{
    director=strcpyrealloc(director,Director);
    if (director) director=compactspace(director);
}

void cXMLTVEvent::AddActor(const char *Actor, const char *ActorRole)
{
    if (ActorRole)
    {
        char *value=NULL;
        if (asprintf(&value,"%s (%s)",Actor,ActorRole)==-1) return;
        actors.Append(value);
    }
    else
    {
        actors.Append(strdup(Actor));
    }
}

void cXMLTVEvent::AddOther(const char *OtherType, const char *Other)
{
    char *value=NULL;
    if (asprintf(&value,"%s|%s",OtherType,Other)==-1) return;
    others.Append(value);
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
    if (director)
    {
        free(director);
        director=NULL;
    }
    year=0;
    starttime = 0;
    duration = 0;
    vps= (time_t) 0;
    eventid=0;
    actors.Clear();
    others.Clear();
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
    director=NULL;
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
    if (!xevent->Duration()) return NULL;
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
    int eventTimeDiff=xevent->Duration()/4;
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
                            dsyslog("xmltv2vdr: '%s' found '%s' for '%s'",name,p->Title(),xevent->Title());
                        f=p;
                        maxdiff=diff;
                    }
                }

            }
        }
    }
    return f;
}

bool cParse::PutEvent(cSchedule* schedule, cEvent *event, cXMLTVEvent *xevent, cEPGMapping *map)
{
    if (!schedule) return false;
    if (!xevent) return false;
    if (!map) return false;

    time_t start;
    if (!event)
    {
        if ((map->Flags() & OPT_APPEND)==OPT_APPEND)
        {
            start=xevent->StartTime();
            event=new cEvent(xevent->EventID());
            if (!event) return false;
            event->SetStartTime(start);
            event->SetDuration(xevent->Duration());
            event->SetTitle(xevent->Title());
            event->SetShortText(xevent->ShortText());
            event->SetDescription(xevent->Description());
            event->SetVersion(0);
            schedule->AddEvent(event);
            dsyslog("xmltv2vdr: '%s' adding event '%s' @%s",name,xevent->Title(),ctime(&start));
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
        dsyslog("xmltv2vdr: '%s' changing event '%s' @%s",name,event->Title(),ctime(&start));
    }
    if ((map->Flags() & USE_SHORTTEXT)==USE_SHORTTEXT)
    {
        if (xevent->ShortText() && (strlen(xevent->ShortText())>0))
        {
            if (!strcmp(xevent->ShortText(),event->Title()))
            {
                dsyslog("xmltv2vdr: '%s' title and subtitle equal, clearing subtitle",name);
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
        cStringList *actors=xevent->Actors();
        cStringList *others=xevent->Others();

        if ((map->Flags() & CREDITS_ACTORS)==CREDITS_ACTORS)
        {
            if ((map->Flags() & CREDITS_ACTORS_LIST)==CREDITS_ACTORS_LIST)
            {
                if (actors->Size())
                {
                    addExt=xevent->Add2Description(tr("With"));
                    addExt=xevent->Add2Description(" ");
                }
                for (int i=0; i<actors->Size(); i++)
                {
                    addExt=xevent->Add2Description((*actors)[i]);
                    if (i<actors->Size()-1) addExt=xevent->Add2Description(",");
                }
                if (actors->Size()) addExt=xevent->Add2Description("\n");
            }
            else
            {
                cTEXTMapping *text=TEXTMapping("actor");
                if (text)
                {
                    for (int i=0; i<actors->Size(); i++)
                    {
                        addExt=xevent->Add2Description(text->Value(),(*actors)[i]);
                    }
                }
            }
        }

        if ((map->Flags() & CREDITS_OTHERS)==CREDITS_OTHERS)
        {
            for (int i=0; i<others->Size(); i++)
            {
                char *val=strdup((*others)[i]);
                if (val)
                {
                    char *oth=strchr(val,'|');
                    if (oth)
                    {
                        *oth=0;
                        oth++;
                        cTEXTMapping *text=TEXTMapping(val);
                        if (text)
                        {
                            addExt=xevent->Add2Description(text->Value(),oth);
                        }
                    }
                    free(val);
                }
            }
        }

        if ((map->Flags() & CREDITS_DIRECTOR)==CREDITS_DIRECTOR)
        {
            if (xevent->Director())
            {
                cTEXTMapping *text=TEXTMapping("director");
                if (text) addExt=xevent->Add2Description(text->Value(),xevent->Director());
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
        cTEXTMapping *text;
        text=TEXTMapping("originaltitle");
        if (text) addExt=xevent->Add2Description(text->Value(),xevent->OrigTitle());
    }
    if (((map->Flags() & USE_RATING)==USE_RATING) && (xevent->Rating()) && (xevent->RatingSystem()))
    {
        addExt=xevent->Add2Description(xevent->RatingSystem(),xevent->Rating());
    }
    if (((map->Flags() & USE_REVIEW)==USE_REVIEW) && (xevent->Review()))
    {
        cTEXTMapping *text;
        text=TEXTMapping("review");
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
                if (lang && slang && !xmlStrncasecmp(lang, (const xmlChar *) slang,2))
                {
                    xmlChar *content=xmlNodeListGetString(node->doc,node->xmlChildrenNode,1);
                    if (content)
                    {
                        xevent.SetTitle(conv->Convert((const char *) content));
                        xmlFree(content);
                    }
                    xmlFree(lang);
                }
                else
                {
                    xmlChar *content=xmlNodeListGetString(node->doc,node->xmlChildrenNode,1);
                    if (content)
                    {
                        xevent.SetOrigTitle(conv->Convert((const char *) content));
                        xmlFree(content);
                    }
                }
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
                                xevent.AddActor(conv->Convert((const char *) content));
                                xmlFree(content);
                            }
                        }
                        else if ((!xmlStrcasecmp(vnode->name, (const xmlChar *) "director")))
                        {
                            xmlChar *content=xmlNodeListGetString(vnode->doc,vnode->xmlChildrenNode,1);
                            if (content)
                            {
                                xevent.SetDirector(conv->Convert((const char *) content));
                                xmlFree(content);
                            }
                        }
                        else
                        {
                            xmlChar *content=xmlNodeListGetString(vnode->doc,vnode->xmlChildrenNode,1);
                            if (content)
                            {
                                xevent.AddOther((const char *) vnode->name,conv->Convert((const char *) content));
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
            else
            {
                esyslog("xmltv2vdr: '%s' unknown element %s, please report!",name,node->name);
            }
        }
        node=node->next;
    }
    return true;
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

bool cParse::Process(char *buffer, int bufsize)
{
    if (!buffer) return false;
    if (!bufsize) return false;

    xmlDocPtr xmltv;
    xmltv=xmlReadMemory(buffer,bufsize,NULL,NULL,0);
    if (!xmltv) return false;

    xmlNodePtr rootnode=xmlDocGetRootElement(xmltv);
    if (!rootnode)
    {
        xmlFreeDoc(xmltv);
        return false;
    }

    time_t begin=time(NULL);
    xmlNodePtr node=rootnode->xmlChildrenNode;
    cEPGMapping *oldmap=NULL;
    while (node)
    {
        if (node->type==XML_ELEMENT_NODE)
        {
            if ((!xmlStrcasecmp(node->name, (const xmlChar *) "programme")))
            {
                xmlChar *channelid=xmlGetProp(node,(const xmlChar *) "channel");
                if (channelid)
                {
                    cEPGMapping *map=EPGMapping((const char *) channelid);
                    if (map)
                    {
                        time_t end=begin+(86000*map->Days())+3600; // 1 hour overlap
                        if (oldmap!=map)
                        {
                            dsyslog("xmltv2vdr: '%s' processing '%s'",name,channelid);
                            dsyslog("xmltv2vdr: '%s' from %s",name,ctime(&begin));
                            dsyslog("xmltv2vdr: '%s' till %s",name,ctime(&end));
                        }
                        oldmap=map;
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

                        if (starttime && (starttime>begin) && (starttime<end))
                        {
                            xevent.Clear();

                            xmlChar *vpsstart=xmlGetProp(node,(const xmlChar *) "vps-start");
                            if (vpsstart)
                            {
                                time_t vps=ConvertXMLTVTime2UnixTime((char *) vpsstart);
                                xevent.SetVps(vps);
                                xmlFree(vpsstart);
                            }

                            xevent.SetStartTime(starttime);
                            if (stoptime) xevent.SetDuration(stoptime-starttime);
                            FetchEvent(node);

                            cSchedulesLock *schedulesLock = new cSchedulesLock(true,2000); // to be safe ;)
                            const cSchedules *schedules = cSchedules::Schedules(*schedulesLock);
                            if (schedules)
                            {
                                for (int i=0; i<map->NumChannelIDs(); i++)
                                {
                                    bool addevents=false;
                                    if ((map->Flags() & OPT_APPEND)==OPT_APPEND) addevents=true;

                                    cChannel *channel=Channels.GetByChannelID(map->ChannelIDs()[i]);
                                    if (channel)
                                    {
                                        cSchedule* schedule = (cSchedule *) schedules->GetSchedule(channel,addevents);
                                        if (schedule)
                                        {
                                            cEvent *event=NULL;
                                            if ((event=SearchEvent(schedule,&xevent)))
                                            {
                                                PutEvent(schedule,event,&xevent,map);
                                            }
                                            else
                                            {
                                                if (addevents)
                                                {
                                                    PutEvent(schedule,event,&xevent,map);
                                                }
                                                else
                                                {
                                                    time_t start=xevent.StartTime();
                                                    esyslog("xmltv2vdr: '%s' cannot find existing event in epg.data for xmltv-event %s@%s",
                                                            name,xevent.Title(),ctime(&start));
                                                }
                                            }
                                        }
                                        else
                                        {
                                            esyslog("xmltv2vdr: '%s' cannot get schedule for channel %s (no import)",
                                                    name,channel->Name());
                                        }
                                    }
                                    else
                                    {
                                        esyslog("xmltv2vdr: '%s' channel %s not found in channels.conf",
                                                name,*map->ChannelIDs()[i].ToString());
                                    }
                                }
                            }
                            else
                            {
                                esyslog("xmltv2vdr: '%s' cannot get schedules",name);
                            }
                            delete schedulesLock;
                        }
                    }
                    else
                    {
                        esyslog("xmltv2vdr: '%s' no mapping for channelid %s",name,channelid);
                    }
                    xmlFree(channelid);
                }
                else
                {
                    esyslog("xmltv2vdr: '%s' missing channelid in xmltv file",name);
                }
            }
        }
        node=node->next;
    }
    xmlFreeDoc(xmltv);
    return true;
}

void cParse::InitLibXML()
{
    xmlInitParser();
}

void cParse::CleanupLibXML()
{
    xmlCleanupParser();
}

cParse::cParse(const char *Name, cEPGMappings *Maps, cTEXTMappings *Texts)
{
#if VDRVERSNUM < 10701 || defined(__FreeBSD__)
    conv = new cCharSetConv("UTF-8",cCharSetConv::SystemCharacterTable() ?
                            cCharSetConv::SystemCharacterTable() : "UTF-8");
#else
    conv = new cCharSetConv("UTF-8",NULL);
#endif
    name=strdup(Name);
    maps=Maps;
    texts=Texts;
}

cParse::~cParse()
{
    delete conv;
    free(name);
}
