/*
 * setup.cpp: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "setup.h"

#include <vdr/osdbase.h>
#include <vdr/i18n.h>
#include <time.h>
#include <pwd.h>

#define CHNUMWIDTH (numdigits(Channels.MaxNumber())+1)

#define NewTitle(x) new cOsdItem(cString::sprintf("%s%s%s", "---- ",x," ----"),osUnknown,false)

char *strcatrealloc(char *dest, const char *src)
{
    if (!src || !*src)
        return dest;

    size_t l = (dest ? strlen(dest) : 0) + strlen(src) + 1;
    if (dest)
    {
        dest = (char *)realloc(dest, l);
        strcat(dest, src);
    }
    else
    {
        dest = (char*)malloc(l);
        strcpy(dest, src);
    }
    return dest;
}


// --------------------------------------------------------------------------------------------------------

cMyMenuEditBitItem::cMyMenuEditBitItem(const char *Name, uint *Value, uint Mask, const char *FalseString, const char *TrueString)
        :cMenuEditBoolItem(Name,&(bit=0),FalseString,TrueString)
{
    value=Value;
    bit=(*value & Mask)!=0;
    mask=Mask;
    Set();
}

void cMyMenuEditBitItem::Set(void)
{
    *value=bit?*value|mask:*value&~mask;
    cMenuEditBoolItem::Set();
}

// --------------------------------------------------------------------------------------------------------

cMenuSetupXmltv2vdr::cMenuSetupXmltv2vdr(cPluginXmltv2vdr *Plugin)
{
    baseplugin=Plugin;
    sourcesBegin=sourcesEnd=mappingBegin=mappingEnd=mappingEntry=0;
    epall=(int) baseplugin->EPAll();
    wakeup=(int) baseplugin->WakeUp();
    cs=NULL;
    cm=NULL;
    Output();
}

cMenuSetupXmltv2vdr::~cMenuSetupXmltv2vdr()
{
    if (cs) cs->ClearMenu();
    if (cm) cm->ClearMenu();
}

void cMenuSetupXmltv2vdr::Output(void)
{
    if (!baseplugin) return;
    int current=Current();
    Clear();
    cOsdItem *first=NewTitle(tr("options"));
    Add(first,true);

    epEntry=0;
    struct passwd *pw=getpwuid(getuid());
    if (pw)
    {
        char *path=NULL;
        if (asprintf(&path,"%s/.eplists/lists",pw->pw_dir)!=-1)
        {
            if (!access(path,R_OK))
            {
                Add(new cMenuEditBoolItem(tr("add season/episode on all timers"),&epall),true);
            }
            free(path);
        }
    }
    Add(new cMenuEditBoolItem(tr("automatic wakeup"),&wakeup),true);

    Add(new cOsdItem(tr("text mapping")),true);
    mappingEntry=Current();
    Add(NewTitle(tr("epg sources")),true);

    if (!baseplugin->EPGSourceCount())
    {
        baseplugin->ReadInEPGSources();
        if (!baseplugin->EPGSourceCount())
        {
            Add(new cOsdItem(tr("no epgsources installed"),osUnknown,false));
            Display();
            return;
        }
    }
    sourcesBegin=Current()+1;
    for (int i=0; i<baseplugin->EPGSourceCount(); i++)
    {
        cEPGSource *epgsrc=baseplugin->EPGSource(i);
        if (epgsrc)
        {
            if (epgsrc->Disabled())
            {
                cString buffer = cString::sprintf("%s:\t%s",epgsrc->Name(),tr("disabled"));
                Add(new cOsdItem(buffer),true);
            }
            else
            {
                Add(new cOsdItem(epgsrc->Name()),true);
            }
        }
    }
    sourcesEnd=Current();

    Add(NewTitle(tr("epg source channels")),true);
    generatesumchannellist();
    mappingBegin=Current()+1;
    for (int i=0; i<channels.Size(); i++)
    {
        bool mapped=false;
        cEPGMapping *map=baseplugin->EPGMapping(channels[i]);
        if (map)
        {
            if (map->NumChannelIDs()) mapped=true;
        }
        cString buffer = cString::sprintf("%s:\t%s",channels[i],mapped ? tr("mapped") : "");
        Add(new cOsdItem(buffer),true);
    }
    mappingEnd=Current();

    if (current==-1)
    {
        SetCurrent(first);
        CursorDown();
    }
    else
    {
        SetCurrent(Get(current));
    }
    Display();
}

void cMenuSetupXmltv2vdr::generatesumchannellist()
{
    channels.Clear();
    for (int i=0; i<baseplugin->EPGSourceCount(); i++)
    {
        cEPGSource *epgsrc=baseplugin->EPGSource(i);
        if (epgsrc)
        {
            cEPGChannels *channellist=epgsrc->ChannelList();
            if (channellist)
            {
                for (int x=0; x<channellist->Count(); x++)
                {
                    if (channellist->Get(x)->InUse())
                    {
                        bool found=false;
                        for (int t=0; t<channels.Size(); t++)
                        {
                            if (!strcmp(channels[t],channellist->Get(x)->Name())) found=true;
                        }
                        if (!found)
                        {
                            channels.Append(strdup(channellist->Get(x)->Name()));
                        }

                    }
                }
            }
        }
    }
    channels.Sort();
}

void cMenuSetupXmltv2vdr::Store(void)
{

    char *order=NULL;
    for (int i=0; i<baseplugin->EPGSourceCount(); i++)
    {
        cEPGSource *epgsrc=baseplugin->EPGSource(i);
        if (epgsrc && epgsrc->Name())
        {
            if (epgsrc->Disabled())
            {
                order=strcatrealloc(order,"-");
            }
            order=strcatrealloc(order,epgsrc->Name());
            if (i<baseplugin->EPGSourceCount()-1) order=strcatrealloc(order,",");
        }
    }

    if (order)
    {
        SetupStore("source.order",order);
        free(order);
    }

    SetupStore("options.epall",epall);
    SetupStore("options.wakeup",wakeup);
    baseplugin->SetEPAll((bool) epall);
    baseplugin->SetWakeUp((bool) wakeup);
}

eOSState cMenuSetupXmltv2vdr::edit()
{
    if ((Current()>=sourcesBegin) && (Current()<=sourcesEnd))
    {
        cs=new cMenuSetupXmltv2vdrChannelSource(baseplugin,this,Current()-sourcesBegin);
        return AddSubMenu(cs);
    }

    if ((Current()>=mappingBegin) && (Current()<=mappingEnd))
    {
        cm=new cMenuSetupXmltv2vdrChannelMap(baseplugin,this,Current()-mappingBegin);
        return AddSubMenu(cm);
    }
    if (Current()==mappingEntry)
    {
        return AddSubMenu(new cMenuSetupXmltv2vdrTextMap(baseplugin));
    }
    return osUnknown;
}

eOSState cMenuSetupXmltv2vdr::ProcessKey(eKeys Key)
{
    eOSState state = cOsdMenu::ProcessKey(Key);
    if (HasSubMenu()) return osContinue;
    switch (state)
    {
    case osContinue:
        if ((Key==kLeft) || (Key==kRight) || (Key==(kLeft|k_Repeat)) || (Key==(kRight|k_Repeat)))
        {
            if ((epEntry) && (Current()==epEntry)) Output();
        }
        if ((Key==kDown) || (Key==kUp) || (Key==(kDown|k_Repeat)) || (Key==(kUp|k_Repeat)))
        {
            if ((Current()>=sourcesBegin) && (Current()<=sourcesEnd))
            {
                if ((sourcesEnd-sourcesBegin)>0)
                {
                    SetHelp(tr("Button$Up"),tr("Button$Down"),tr("Button$Log"),tr("Button$Edit"));
                }
                else
                {
                    SetHelp(NULL,NULL,tr("Button$Log"),tr("Button$Edit"));
                }
            }
            else if (Current()==mappingEntry)
            {
                SetHelp(NULL,NULL,NULL,tr("Button$Edit"));
            }
            else if ((Current()>=mappingBegin) && (Current()<=mappingEnd))
            {
                SetHelp(NULL,NULL,NULL,tr("Button$Edit"));
            }
            else
            {
                SetHelp(NULL,NULL,NULL,NULL);
            }
        }
        break;

    case osUnknown:
        if (Key==k0)
        {
            // disable/enable
            if ((Current()>=sourcesBegin) && (Current()<=sourcesEnd))
            {
                int srcid=Current()-sourcesBegin;
                cEPGSource *src=baseplugin->EPGSource(srcid);
                if (src)
                {
                    if (src->Disabled())
                    {
                        src->Enable();
                    }
                    else
                    {
                        src->Disable();
                    }
                    Output();
                    Store();
                }
            }
        }
        if (Key==kRed)
        {
            // move up
            if ((Current()>sourcesBegin) && (Current()<=sourcesEnd))
            {
                int From=Current()-sourcesBegin;
                int To=From-1;
                if (baseplugin->EPGSourceMove(From,To))
                {
                    CursorUp();
                    Output();
                    Store();
                }
            }
        }
        if (Key==kGreen)
        {
            // move down
            if ((Current()>=sourcesBegin) && (Current()<sourcesEnd))
            {
                int From=Current()-sourcesBegin;
                int To=From+1;
                if (baseplugin->EPGSourceMove(From,To))
                {
                    CursorDown();
                    Output();
                    Store();
                }
            }
        }
        if ((Key==kOk) || (Key==kBlue))
        {
            state=edit();
            if (state==osUnknown)
            {
                Store();
                state=osBack;
            }
        }
        if (Key==kYellow)
        {
            if ((Current()>=sourcesBegin) && (Current()<=sourcesEnd))
            {
                cEPGSource *src=baseplugin->EPGSource(Current()-sourcesBegin);
                if (src)
                {
                    return AddSubMenu(new cMenuSetupXmltv2vdrLog(baseplugin,src));
                }
            }
        }
        break;

    default:
        break;
    }
    return state;
}

// --------------------------------------------------------------------------------------------------------

cMenuSetupXmltv2vdrTextMap::cMenuSetupXmltv2vdrTextMap(cPluginXmltv2vdr *Plugin)
{
    baseplugin=Plugin;
    SetPlugin(baseplugin);
    SetSection(cString::sprintf("%s '%s' : %s",trVDR("Plugin"), baseplugin->Name(), tr("texts")));

    cTEXTMapping *textmap;

#define settval(dummy) textmap=baseplugin->TEXTMapping(#dummy); \
   if (textmap) { \
     strn0cpy(dummy,textmap->Value(),sizeof(dummy)-1); \
   } else { \
     strcpy(dummy,tr(#dummy)); \
   }

    Add(NewTitle(tr("country and date")));
    settval(country);
    settval(year);
    Add(new cMenuEditStrItem("country",country,sizeof(country)));
    Add(new cMenuEditStrItem("year",year,sizeof(year)));

    Add(NewTitle(tr("original title")));
    settval(originaltitle);
    Add(new cMenuEditStrItem("originaltitle",originaltitle,sizeof(originaltitle)));

    Add(NewTitle(tr("category")));
    settval(category);
    Add(new cMenuEditStrItem("category",category,sizeof(category)));

    Add(NewTitle(tr("credits")));
    settval(director);
    settval(actor);
    settval(writer);
    settval(adapter);
    settval(producer);
    settval(composer);
    settval(editor);
    settval(presenter);
    settval(commentator);
    settval(guest);
    Add(new cMenuEditStrItem("actor",actor,sizeof(actor)));
    Add(new cMenuEditStrItem("guest",guest,sizeof(guest)));
    Add(new cMenuEditStrItem("director",director,sizeof(director)));
    Add(new cMenuEditStrItem("writer",writer,sizeof(writer)));
    Add(new cMenuEditStrItem("composer",composer,sizeof(composer)));
    Add(new cMenuEditStrItem("editor",editor,sizeof(editor)));
    Add(new cMenuEditStrItem("producer",producer,sizeof(producer)));
    Add(new cMenuEditStrItem("adapter",adapter,sizeof(adapter)));
    Add(new cMenuEditStrItem("commentator",commentator,sizeof(commentator)));
    Add(new cMenuEditStrItem("presenter",presenter,sizeof(presenter)));

    Add(NewTitle(tr("video informations")));
    settval(video);
    settval(blacknwhite);
    Add(new cMenuEditStrItem("video",video,sizeof(video)));
    Add(new cMenuEditStrItem("blackandwhite",blacknwhite,sizeof(blacknwhite)));

    Add(NewTitle(tr("audio informations")));
    settval(audio);
    settval(dolby);
    settval(dolbydigital);
    settval(bilingual);
    Add(new cMenuEditStrItem("audio",audio,sizeof(audio)));
    Add(new cMenuEditStrItem("dolby",dolby,sizeof(dolby)));
    Add(new cMenuEditStrItem("dolby digital",dolbydigital,sizeof(dolbydigital)));
    Add(new cMenuEditStrItem("bilingual",bilingual,sizeof(bilingual)));

    Add(NewTitle(tr("review")));
    settval(review);
    Add(new cMenuEditStrItem("review",review,sizeof(review)));

    Add(NewTitle(tr("starrating")));
    settval(starrating);
    Add(new cMenuEditStrItem("starrating",starrating,sizeof(starrating)));

    Add(NewTitle(tr("season and episode")));
    settval(season);
    settval(episode);
    Add(new cMenuEditStrItem("season",season,sizeof(season)));
    Add(new cMenuEditStrItem("episode",episode,sizeof(episode)));

}

void cMenuSetupXmltv2vdrTextMap::Store()
{
    if (!baseplugin) return;
    cTEXTMapping *textmap;

#define savetval(dummy) textmap=baseplugin->TEXTMapping(#dummy); \
   if (textmap) { \
    textmap->ChangeValue(dummy); \
   } else { \
    baseplugin->TEXTMappingAdd(new cTEXTMapping(#dummy,dummy)); \
   }

    savetval(country);
    savetval(year);
    savetval(originaltitle);
    savetval(category);
    savetval(director);
    savetval(actor);
    savetval(writer);
    savetval(adapter);
    savetval(producer);
    savetval(composer);
    savetval(editor);
    savetval(presenter);
    savetval(commentator);
    savetval(guest);
    savetval(video);
    savetval(blacknwhite);
    savetval(audio);
    savetval(dolby);
    savetval(dolbydigital);
    savetval(bilingual);
    savetval(review);
    savetval(starrating);
    savetval(season);
    savetval(episode);

    SetupStore("textmap.country",country);
    SetupStore("textmap.year",year);
    SetupStore("textmap.originaltitle",originaltitle);
    SetupStore("textmap.category",category);
    SetupStore("textmap.actor",actor);
    SetupStore("textmap.adapter",adapter);
    SetupStore("textmap.commentator",commentator);
    SetupStore("textmap.composer",composer);
    SetupStore("textmap.director",director);
    SetupStore("textmap.editor",editor);
    SetupStore("textmap.guest",guest);
    SetupStore("textmap.presenter",presenter);
    SetupStore("textmap.producer",producer);
    SetupStore("textmap.writer",writer);
    SetupStore("textmap.video",video);
    SetupStore("textmap.blacknwhite",blacknwhite);
    SetupStore("textmap.audio",audio);
    SetupStore("textmap.dolby",dolby);
    SetupStore("textmap.dolbydigital",dolbydigital);
    SetupStore("textmap.bilingual",bilingual);
    SetupStore("textmap.review",review);
    SetupStore("textmap.starrating",starrating);
    SetupStore("textmap.season",season);
    SetupStore("textmap.episode",episode);
}

// --------------------------------------------------------------------------------------------------------

void cMenuSetupXmltv2vdrLog::output(void)
{
    int cur=Current();
    Clear();
    Add(NewTitle(tr("overview")));
    time_t nextrun=src->NextRunTime();    
    if (nextrun)
    {
        struct tm tm;
        localtime_r(&nextrun,&tm);
        strftime(nextrun_str,sizeof(nextrun_str)-1,"%d %b %H:%M:%S",&tm);
    }
    cString last;
    if (src->Active())
    {
        last=cString::sprintf("%s:\t%s",tr("next execution"),
                              tr("active"));
    }
    else
    {
        last=cString::sprintf("%s:\t%s",tr("next execution"),
                              nextrun ?  nextrun_str : "-");
    }
    Add(new cOsdItem(last,osUnknown,true));

    int tlen=strlen(3+tr("log"))+strlen(tr("Button$Errors"))+strlen(tr("Button$Info"))+strlen(tr("Button$Debug"));
    char *ntitle=(char *) malloc(tlen);
    if (ntitle)
    {
        strcpy(ntitle,tr("log"));
        strcat(ntitle," (");

        switch (level)
        {
        case VIEW_ERROR:
            strcat(ntitle,tr("Button$Errors"));
            break;
        case VIEW_INFO:
            strcat(ntitle,tr("Button$Info"));
            break;
        case VIEW_DEBUG:
            strcat(ntitle,tr("Button$Debug"));
            break;
        }
        strcat(ntitle,")");
        Add(NewTitle(ntitle));
        free(ntitle);

    }
    else
    {
        Add(NewTitle(tr("log")));
    }

    if (src && src->Log)
    {
        char *saveptr;
        char *log=strdup(src->Log);
        if (log)
        {
            char *pch=strtok_r(log,"\n",&saveptr);
            while (pch)
            {
                bool outp=false;
                if (level==VIEW_DEBUG) outp=true;
                if ((level==VIEW_INFO) && ((pch[0]=='I') || (pch[0]=='E'))) outp=true;
                if ((level==VIEW_ERROR) && (pch[0]=='E')) outp=true;
                if (outp)
                {
                    if (font->Width(pch+1)>width)
                    {
                        cTextWrapper wrap(pch+1,font,width);
                        for (int i=0; i<wrap.Lines();i++)
                        {
                            Add(new cOsdItem(wrap.GetLine(i),osUnknown,true));
                        }
                    }
                    else
                    {
                        Add(new cOsdItem(pch+1,osUnknown,true));
                    }
                }
                pch=strtok_r(NULL,"\n",&saveptr);
            }
            free(log);
        }
    }
    if (cur>Count()) cur=Count();
    SetCurrent(Get(cur));
    Display();
}

cMenuSetupXmltv2vdrLog::cMenuSetupXmltv2vdrLog(cPluginXmltv2vdr *Plugin, cEPGSource *Source)
        :cOsdMenu("",25)
{
    cString title=cString::sprintf("%s - %s '%s' : %s Log",
                                   trVDR("Setup"),trVDR("Plugin"),
                                   Plugin->Name(), Source->Name());
    SetTitle(title);
    src=Source;
    SetHelp(tr("Button$Info"));

    level=VIEW_ERROR;
    width=0;
    lastrefresh=(time_t) 0;
    font=NULL;
    nextrun_str[0]=0;
    
    cSkinDisplayMenu *disp=DisplayMenu();
    if (disp)
    {
        width=disp->GetTextAreaWidth();
        font=disp->GetTextAreaFont(false);
    }
    if (!width) width=Setup.OSDWidth;
    if (!font) font=cFont::GetFont(fontOsd);

    output();
}

eOSState cMenuSetupXmltv2vdrLog::ProcessKey(eKeys Key)
{
    eOSState state = cOsdMenu::ProcessKey(Key);
    if (HasSubMenu()) return osContinue;
    if (state==osUnknown)
    {
        switch (Key)
        {
        case kRed:
            switch (level)
            {
            case VIEW_ERROR:
                level=VIEW_INFO;
                SetHelp(tr("Button$Debug"));
                break;
            case VIEW_INFO:
                level=VIEW_DEBUG;
                SetHelp(tr("Button$Errors"));
                break;
            case VIEW_DEBUG:
                level=VIEW_ERROR;
                SetHelp(tr("Button$Info"));
                break;
            }
            output();
            break;
        case kGreen:
            level = VIEW_INFO;
            output();
            break;
        case kBlue:
            level = VIEW_DEBUG;
            output();
            break;
        case kNone:
            if (src->Active())
            {
                if (time(NULL)>(lastrefresh+5))
                {
                    output();
                    lastrefresh=time(NULL);
                }
            }
            else
            {
                if (lastrefresh)
                {
                    if (time(NULL)>(lastrefresh+5))
                    {
                        output();
                        lastrefresh=(time_t) 0;
                    }
                }
            }
            break;
        case kBack:
        case kOk:
            state=osBack;
            break;
        default:
            break;
        }
    }
    return state;
}


// --------------------------------------------------------------------------------------------------------

cMenuSetupXmltv2vdrChannelSource::cMenuSetupXmltv2vdrChannelSource(cPluginXmltv2vdr *Plugin, cMenuSetupXmltv2vdr *Menu, int Index)
{
    menu=Menu;
    baseplugin=Plugin;
    sel=NULL;

    day=0;
    weekday=127;
    start=0;
    upstart=1;

    days=0;
    pin[0]=0;

    updateEntry=0;

    epgsrc=baseplugin->EPGSource(Index);
    if (!epgsrc) return;

    SetSection(cString::sprintf("%s '%s' : %s",trVDR("Plugin"), baseplugin->Name(), epgsrc->Name()));

    upstart=epgsrc->ExecUpStart();
    weekday=epgsrc->ExecWeekDay();
    start=epgsrc->ExecTime();
    days=epgsrc->DaysInAdvance();

    output();
}

cMenuSetupXmltv2vdrChannelSource::~cMenuSetupXmltv2vdrChannelSource()
{
    if (menu)
    {
        menu->Output();
        menu->ClearCS();
    }
    if (sel) delete [] sel;
}

void cMenuSetupXmltv2vdrChannelSource::output(void)
{
    Clear();

    Add(NewTitle(tr("options")));

    Add(new cMenuEditBoolItem(tr("update"),&upstart,tr("on time"),tr("on start")),true);
    updateEntry=Current();
    if (!upstart)
    {
        Add(new cMenuEditDateItem(trVDR("Day"),&day,&weekday));
        Add(new cMenuEditTimeItem(tr("update at"),&start));
    }
    Add(new cMenuEditIntItem(tr("days in advance"),&days,1,epgsrc->DaysMax()));
    if (epgsrc->NeedPin())
    {
        if (epgsrc->Pin())
        {
            strncpy(pin,epgsrc->Pin(),sizeof(pin)-1);
            pin[sizeof(pin)-1]=0;
        }
        Add(new cMenuEditStrItem(tr("pin"),pin,sizeof(pin)));
    }
    Add(NewTitle(tr("channels provided")));

    cEPGChannels *channellist=epgsrc->ChannelList();
    if (!channellist) return;

    sel=new int[channellist->Count()];
    if (!sel) return;

    for (int i=0; i<channellist->Count(); i++)
    {
        if (channellist->Get(i)->InUse())
        {
            sel[i]=1;
        }
        else
        {
            sel[i]=0;
        }
        Add(new cMenuEditBoolItem(channellist->Get(i)->Name(),&sel[i],"",tr("selected")));
    }
    Display();
}

eOSState cMenuSetupXmltv2vdrChannelSource::ProcessKey(eKeys Key)
{
    eOSState state = cOsdMenu::ProcessKey(Key);
    if (HasSubMenu()) return osContinue;
    switch (state)
    {
    case osContinue:
        if ((Key==kLeft) || (Key==kRight) || (Key==(kLeft|k_Repeat)) || (Key==(kRight|k_Repeat)))
        {
            if (Current()==updateEntry) output();
        }

    case osUnknown:
        if (Key==kOk)
        {
            Store();
            state=osBack;
        }

    default:
        break;
    }
    return state;
}

void cMenuSetupXmltv2vdrChannelSource::Store(void)
{
    if ((!baseplugin) || (!sel) || (!epgsrc)) return;

    epgsrc->ChangeExec(upstart,start,weekday);
    epgsrc->ChangeChannelSelection(sel);
    epgsrc->ChangeDaysInAdvance(days);
    if (epgsrc->NeedPin())
        epgsrc->ChangePin(pin);
    epgsrc->Store();
}

// --------------------------------------------------------------------------------------------------------

cMenuSetupXmltv2vdrChannelMap::cMenuSetupXmltv2vdrChannelMap(cPluginXmltv2vdr *Plugin, cMenuSetupXmltv2vdr *Menu, int Index)
{
    baseplugin=Plugin;
    menu=Menu;
    hasmaps=false;
    flags=0;
    if (Index>menu->ChannelList()->Size()) return;
    channel=(*menu->ChannelList())[Index];
    if (!channel) return;

    SetPlugin(baseplugin);

    cEPGMapping *oldmap=baseplugin->EPGMapping(channel);
    if (oldmap)
    {
        map=new cEPGMapping(*oldmap);
    }
    else
    {
        map=new cEPGMapping(channel,NULL);
    }
    if (!map) return;

    title=cString::sprintf("%s - %s '%s' : %s",trVDR("Setup"),trVDR("Plugin"), baseplugin->Name(), channel);
    SetTitle(title);

    flags=map->Flags();
    c1=c2=c3=cm=0;
    SetHelp(NULL,NULL,tr("Button$Reset"),tr("Button$Copy"));
    output();
}

cMenuSetupXmltv2vdrChannelMap::~cMenuSetupXmltv2vdrChannelMap()
{
    if (menu)
    {
        menu->Output();
        menu->ClearCM();
    }
    if (map) delete map;
}

int cMenuSetupXmltv2vdrChannelMap::getdaysmax()
{
    if (!baseplugin) return 1;

    int ret=INT_MAX;
    for (int i=0; i<baseplugin->EPGSourceCount(); i++)
    {
        cEPGSource *epgsrc=baseplugin->EPGSource(i);
        if (epgsrc)
        {
            cEPGChannels *channellist=epgsrc->ChannelList();
            if (channellist)
            {
                for (int x=0; x<channellist->Count(); x++)
                {
                    if (!strcmp(channellist->Get(x)->Name(),channel))
                    {
                        if (channellist->Get(x)->InUse())
                        {
                            if (epgsrc->DaysMax()<ret) ret=epgsrc->DaysMax();
                        }
                        break;
                    }
                }
            }
        }
    }
    if (ret==INT_MAX) ret=1;
    return ret;
}

cOsdItem *cMenuSetupXmltv2vdrChannelMap::option(const char *s, bool yesno)
{
    cString buffer = cString::sprintf("%s:\t%s", s, yesno ? trVDR("yes") : trVDR("no"));
    return new cOsdItem(buffer,osUnknown,false);
}

void cMenuSetupXmltv2vdrChannelMap::output(void)
{
    if (!map) return;

    int current=Current();

    Clear();
    cOsdItem *first=NewTitle(tr("epg source channel options"));
    Add(first,true);

    Add(new cMyMenuEditBitItem(tr("type of processing"),&flags,OPT_APPEND,tr("merge"),tr("create")),true);
    c1=Current();
    if ((flags & OPT_APPEND)!=OPT_APPEND)
    {
        Add(new cMyMenuEditBitItem(tr("short text"),&flags,USE_SHORTTEXT),true);
        Add(new cMyMenuEditBitItem(tr("long text"),&flags,USE_LONGTEXT),true);
        c2=Current();
        if ((flags & USE_LONGTEXT)==USE_LONGTEXT)
        {
            Add(new cMyMenuEditBitItem(tr(" merge long texts"),&flags,OPT_MERGELTEXT),true);
        }
    }
    else
    {
        Add(option(tr("short text"),true),true);
        Add(option(tr("long text"),true),true);
        Add(option(tr(" merge long texts"),false),true);
    }
    Add(new cMyMenuEditBitItem(tr("country and date"),&flags,USE_COUNTRYDATE),true);
    Add(new cMyMenuEditBitItem(tr("original title"),&flags,USE_ORIGTITLE),true);
    Add(new cMyMenuEditBitItem(tr("category"),&flags,USE_CATEGORIES),true);
    Add(new cMyMenuEditBitItem(tr("credits"),&flags,USE_CREDITS),true);
    c3=Current();
    if ((flags & USE_CREDITS)==USE_CREDITS)
    {
        Add(new cMyMenuEditBitItem(tr(" actors"),&flags,CREDITS_ACTORS),true);
        Add(new cMyMenuEditBitItem(tr(" director"),&flags,CREDITS_DIRECTORS),true);
        Add(new cMyMenuEditBitItem(tr(" other crew"),&flags,CREDITS_OTHERS),true);
        Add(new cMyMenuEditBitItem(tr(" output"),&flags,CREDITS_LIST,tr("multiline"),tr("singleline")),true);
    }

    Add(new cMyMenuEditBitItem(tr("rating"),&flags,USE_RATING),true);
    Add(new cMyMenuEditBitItem(tr("starrating"),&flags,USE_STARRATING),true);
    Add(new cMyMenuEditBitItem(tr("review"),&flags,USE_REVIEW),true);
    Add(new cMyMenuEditBitItem(tr("video"),&flags,USE_VIDEO),true);
    Add(new cMyMenuEditBitItem(tr("audio"),&flags,USE_AUDIO),true);

    struct passwd *pw=getpwuid(getuid());
    if (pw)
    {
        char *path=NULL;
        if (asprintf(&path,"%s/.eplists/lists",pw->pw_dir)!=-1)
        {
            if (!access(path,R_OK))
                Add(new cMyMenuEditBitItem(tr("season and episode"),&flags,USE_SEASON),true);
            free(path);
        }
    }

    hasmaps=false;
    Add(NewTitle(tr("epg source channel mappings")),true);
    for (int i=0; i<map->NumChannelIDs(); i++)
    {
        cChannel *chan=Channels.GetByChannelID(map->ChannelIDs()[i]);
        if (chan)
        {
            cString buffer = cString::sprintf("%-*i %s", CHNUMWIDTH, chan->Number(),chan->Name());
            Add(new cOsdItem(buffer),true);
            if (!hasmaps) cm=Current();
            hasmaps=true;
        }
        else
        {
            // invalid channelid? remove from list
            map->RemoveChannel(map->ChannelIDs()[i],true);
        }
    }
    map->RemoveInvalidChannels();
    if (!hasmaps)
    {
        Add(new cOsdItem(tr("none")),true);
        cm=Current();
    }

    if (current==-1)
    {
        SetCurrent(first);
        CursorDown();
    }
    else
    {
        SetCurrent(Get(current));
    }
    Display();
}

eOSState cMenuSetupXmltv2vdrChannelMap::ProcessKey(eKeys Key)
{
    cOsdItem *item=NULL;
    eOSState state = cOsdMenu::ProcessKey(Key);
    if (HasSubMenu()) return osContinue;
    if (state==osContinue)
    {
        switch (Key)
        {
        case kLeft:
        case kLeft|k_Repeat:
        case kRight:
        case kRight|k_Repeat:
            if ((Current()==c1) || (Current()==c2) ||
                    (Current()==c3)) output();
            break;
        case kDown:
        case kDown|k_Repeat:
            if (Current()>=cm)
                SetHelp(tr("Button$Unmap"),tr("Button$Map"));
            break;
        case kUp:
        case kUp|k_Repeat:
            if (Current()<cm)
                SetHelp(NULL,NULL,tr("Button$Reset"),tr("Button$Copy"));
        default:
            break;
        }
    }

    if (state==osUnknown)
    {
        switch (Key)
        {
        case kOk:
            if ((Current()>=cm) && (!hasmaps))
            {
                return AddSubMenu(new cMenuSetupXmltv2vdrChannelsVDR(baseplugin,this,channel,title));
            }
            else
            {
                Store();
                state=osBack;
            }
            break;
        case kRed:
            if (Current()>=cm)
            {
                item=Get(Current());
                if (item)
                {
                    if (map)
                    {
                        map->RemoveChannel(atoi(item->Text()));
                        output();
                    }
                }
            }
            break;
        case kGreen:
            if (Current()>=cm)
                return AddSubMenu(new cMenuSetupXmltv2vdrChannelsVDR(baseplugin,this,channel,title));
            break;
        case kBlue: // copy
            if ((Current()<cm) && (baseplugin))
            {
                if (Skins.Message(mtInfo,tr("Copy to all mapped channels?"))==kOk)
                {
                    const char *oldchannel=channel;
                    cEPGMapping *tmap=map;
                    for (int i=0; i<baseplugin->EPGMappingCount();i++)
                    {
                        if (strcmp(baseplugin->EPGMapping(i)->ChannelName(),channel))
                        {
                            channel=baseplugin->EPGMapping(i)->ChannelName();
                            map=baseplugin->EPGMapping(i);
                            Store();
                        }
                    }
                    map=tmap;
                    channel=oldchannel;
                    state=osContinue;
                }
            }
            break;
        case kYellow: // reset
            if (Current()<cm)
            {
                if (Skins.Message(mtInfo,tr("Reset all channel settings?"))==kOk)
                {
                    const char *oldchannel=channel;
                    cEPGMapping *tmap=map;
                    flags=0;
                    for (int i=0; i<baseplugin->EPGMappingCount();i++)
                    {
                        channel=baseplugin->EPGMapping(i)->ChannelName();
                        map=baseplugin->EPGMapping(i);
                        Store();
                    }
                    map=tmap;
                    channel=oldchannel;
                    output();
                    state=osContinue;
                }
            }
            break;
        default:
            break;
        }
    }
    return state;
}

void cMenuSetupXmltv2vdrChannelMap::AddChannel2Map(int ChannelNumber)
{
    if (map)
    {
        map->AddChannel(ChannelNumber);
        output();
    }
}

bool cMenuSetupXmltv2vdrChannelMap::EPGMappingExists(tChannelID ChannelID)
{
    if (!map) return true;
    for (int i=0; i<map->NumChannelIDs(); i++)
    {
        if (map->ChannelIDs()[i]==ChannelID) return true;
    }
    return false;
}

void cMenuSetupXmltv2vdrChannelMap::epgmappingreplace(cEPGMapping *newmapping)
{
    if (!newmapping) return;
    cEPGMapping *map=baseplugin->EPGMapping(newmapping->ChannelName());
    if (!map)
    {
        map=new cEPGMapping(*newmapping);
        baseplugin->EPGMappingAdd(map);
    }
    else
    {
        map->ChangeFlags(newmapping->Flags());
        map->ReplaceChannels(newmapping->NumChannelIDs(),newmapping->ChannelIDs());
    }
}

void cMenuSetupXmltv2vdrChannelMap::Store(void)
{
    if (!channel) return;
    char *name=NULL;
    if (asprintf(&name,"channel.%s",channel)==-1) return;

    char *value=NULL;
    if (asprintf(&value,"0;%i;",flags)==-1)
    {
        free(name);
        return;
    }

    for (int i=0; i<map->NumChannelIDs(); i++)
    {
        cString ChannelID = map->ChannelIDs()[i].ToString();
        value=strcatrealloc(value,*ChannelID);
        if (i<map->NumChannelIDs()-1) value=strcatrealloc(value,";");
    }

    SetupStore(name,value);
    map->ChangeFlags(flags);
    epgmappingreplace(map);

    free(name);
    free(value);
}

// --------------------------------------------------------------------------------------------------------

cMenuSetupXmltv2vdrChannelsVDR::cMenuSetupXmltv2vdrChannelsVDR(cPluginXmltv2vdr *Plugin,
        cMenuSetupXmltv2vdrChannelMap *Map, const char *Channel, cString Title)
        :cOsdMenu("",CHNUMWIDTH)
{
    baseplugin=Plugin;
    map=Map;
    SetHelp(NULL,NULL,tr("Button$Choose"));
    SetTitle(Title);

    for (cChannel *channel = Channels.First(); channel; channel=Channels.Next(channel))
    {
        if (!channel->GroupSep())
        {
            cString buf= cString::sprintf("%d\t%s",channel->Number(),channel->Name());
            if ((epgmappingexists(channel->GetChannelID(),Channel)) || (map->EPGMappingExists(channel->GetChannelID())))
            {
                Add(new cOsdItem(buf,osUnknown,false));
            }
            else
            {
                Add(new cOsdItem(buf));
            }
        }
    }
}

bool cMenuSetupXmltv2vdrChannelsVDR::epgmappingexists(tChannelID channelid, const char *channel2ignore)
{
    if (!baseplugin) return true;
    if (!baseplugin->EPGMappingCount()) return false;
    for (int i=0; i<baseplugin->EPGMappingCount(); i++)
    {
        if (channel2ignore && !strcmp(baseplugin->EPGMapping(i)->ChannelName(),channel2ignore)) continue;
        for (int x=0; x<baseplugin->EPGMapping(i)->NumChannelIDs(); x++)
        {
            if (baseplugin->EPGMapping(i)->ChannelIDs()[x]==channelid) return true;
        }
    }
    return false;
}

eOSState cMenuSetupXmltv2vdrChannelsVDR::ProcessKey(eKeys Key)
{
    cOsdItem *item=NULL;
    eOSState state = cOsdMenu::ProcessKey(Key);
    if (HasSubMenu()) return osContinue;
    if (state==osUnknown)
    {
        switch (Key)
        {
        case kBack:
            state=osBack;
            break;
        case kYellow:
        case kOk:
            item=Get(Current());
            if (item)
            {
                if (map) map->AddChannel2Map(atoi(item->Text()));
            }
            state=osBack;
            break;
        default:
            break;
        }
    }
    return state;
}

