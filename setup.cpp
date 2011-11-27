/*
 * setup.cpp: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "setup.h"

#include <vdr/osdbase.h>
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
    wakeup=(int) baseplugin->WakeUp;
    upstart=(int) baseplugin->UpStart;
    exectime=baseplugin->ExecTime();
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

    Add(new cMenuEditBoolItem(tr("update"),&upstart,tr("on time"),tr("on start")),true);
    updateEntry=Current();
    if (!upstart)
    {
        Add(new cMenuEditTimeItem(tr("update at"),&exectime),true);
        Add(new cMenuEditBoolItem(tr("automatic wakeup"),&wakeup),true);
    }
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
            Add(new cOsdItem(epgsrc->Name()),true);
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
    SetupStore("options.exectime",exectime);
    SetupStore("options.wakeup",wakeup);
    SetupStore("options.upstart",upstart);

    baseplugin->UpStart=upstart;
    baseplugin->WakeUp=wakeup;
    baseplugin->SetExecTime(exectime);
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
        if ((Key==kLeft) || (Key==kRight) || (Key==kLeft|k_Repeat) || (Key==kRight|k_Repeat))
        {
            if (Current()==updateEntry) Output();
        }
        if ((Key==kDown) || (Key==kUp) || (Key==kDown|k_Repeat) || (Key==kUp|k_Repeat))
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

    textmap=baseplugin->TEXTMapping("country");
    if (textmap)
    {
        strn0cpy(country,textmap->Value(),sizeof(country)-1);
    }
    else
    {
        strcpy(country,tr("country"));
    }

    textmap=baseplugin->TEXTMapping("date");
    if (textmap)
    {
        strn0cpy(date,textmap->Value(),sizeof(date)-1);
    }
    else
    {
        strcpy(date,tr("year"));
    }

    textmap=baseplugin->TEXTMapping("originaltitle");
    if (textmap)
    {
        strn0cpy(originaltitle,textmap->Value(),sizeof(originaltitle)-1);
    }
    else
    {
        strcpy(originaltitle,tr("originaltitle"));
    }

    textmap=baseplugin->TEXTMapping("director");
    if (textmap)
    {
        strn0cpy(director,textmap->Value(),sizeof(director)-1);
    }
    else
    {
        strcpy(director,tr("director"));
    }

    textmap=baseplugin->TEXTMapping("actor");
    if (textmap)
    {
        strn0cpy(actor,textmap->Value(),sizeof(actor)-1);
    }
    else
    {
        strcpy(actor,tr("actor"));
    }

    textmap=baseplugin->TEXTMapping("writer");
    if (textmap)
    {
        strn0cpy(writer,textmap->Value(),sizeof(writer)-1);
    }
    else
    {
        strcpy(writer,tr("writer"));
    }

    textmap=baseplugin->TEXTMapping("adapter");
    if (textmap)
    {
        strn0cpy(adapter,textmap->Value(),sizeof(adapter)-1);
    }
    else
    {
        strcpy(adapter,tr("adapter"));
    }

    textmap=baseplugin->TEXTMapping("producer");
    if (textmap)
    {
        strn0cpy(producer,textmap->Value(),sizeof(producer)-1);
    }
    else
    {
        strcpy(producer,tr("producer"));
    }

    textmap=baseplugin->TEXTMapping("composer");
    if (textmap)
    {
        strn0cpy(composer,textmap->Value(),sizeof(composer)-1);
    }
    else
    {
        strcpy(composer,tr("composer"));
    }

    textmap=baseplugin->TEXTMapping("editor");
    if (textmap)
    {
        strn0cpy(editor,textmap->Value(),sizeof(editor)-1);
    }
    else
    {
        strcpy(editor,tr("editor"));
    }

    textmap=baseplugin->TEXTMapping("presenter");
    if (textmap)
    {
        strn0cpy(presenter,textmap->Value(),sizeof(presenter)-1);
    }
    else
    {
        strcpy(presenter,tr("presenter"));
    }

    textmap=baseplugin->TEXTMapping("commentator");
    if (textmap)
    {
        strn0cpy(commentator,textmap->Value(),sizeof(commentator)-1);
    }
    else
    {
        strcpy(commentator,tr("commentator"));
    }

    textmap=baseplugin->TEXTMapping("guest");
    if (textmap)
    {
        strn0cpy(guest,textmap->Value(),sizeof(guest)-1);
    }
    else
    {
        strcpy(guest,tr("guest"));
    }

    textmap=baseplugin->TEXTMapping("review");
    if (textmap)
    {
        strn0cpy(review,textmap->Value(),sizeof(review)-1);
    }
    else
    {
        strcpy(review,tr("review"));
    }

    textmap=baseplugin->TEXTMapping("category");
    if (textmap)
    {
        strn0cpy(category,textmap->Value(),sizeof(category)-1);
    }
    else
    {
        strcpy(category,tr("category"));
    }

    textmap=baseplugin->TEXTMapping("season");
    if (textmap)
    {
        strn0cpy(season,textmap->Value(),sizeof(season)-1);
    }
    else
    {
        strcpy(season,tr("season"));
    }

    textmap=baseplugin->TEXTMapping("episode");
    if (textmap)
    {
        strn0cpy(episode,textmap->Value(),sizeof(episode)-1);
    }
    else
    {
        strcpy(episode,tr("episode"));
    }

    Add(NewTitle(tr("country and date")));
    Add(new cMenuEditStrItem("country",country,sizeof(country)));
    Add(new cMenuEditStrItem("date",date,sizeof(date)));

    Add(NewTitle(tr("original title")));
    Add(new cMenuEditStrItem("originaltitle",originaltitle,sizeof(originaltitle)));

    Add(NewTitle(tr("category")));
    Add(new cMenuEditStrItem("category",category,sizeof(category)));

    Add(NewTitle(tr("credits")));
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

    Add(NewTitle(tr("review")));
    Add(new cMenuEditStrItem("review",review,sizeof(review)));

    Add(NewTitle(tr("season and episode")));
    Add(new cMenuEditStrItem("season",season,sizeof(season)));
    Add(new cMenuEditStrItem("episode",episode,sizeof(episode)));

}

void cMenuSetupXmltv2vdrTextMap::Store()
{
    if (!baseplugin) return;
    cTEXTMapping *textmap;
    textmap=baseplugin->TEXTMapping("country");
    if (textmap)
    {
        textmap->ChangeValue(country);
    }
    else
    {
        baseplugin->TEXTMappingAdd(new cTEXTMapping("country",country));
    }
    textmap=baseplugin->TEXTMapping("date");
    if (textmap)
    {
        textmap->ChangeValue(date);
    }
    else
    {
        baseplugin->TEXTMappingAdd(new cTEXTMapping("date",date));
    }
    textmap=baseplugin->TEXTMapping("originaltitle");
    if (textmap)
    {
        textmap->ChangeValue(originaltitle);
    }
    else
    {
        baseplugin->TEXTMappingAdd(new cTEXTMapping("originaltitle",originaltitle));
    }
    textmap=baseplugin->TEXTMapping("category");
    if (textmap)
    {
        textmap->ChangeValue(category);
    }
    else
    {
        baseplugin->TEXTMappingAdd(new cTEXTMapping("category",category));
    }
    textmap=baseplugin->TEXTMapping("actor");
    if (textmap)
    {
        textmap->ChangeValue(actor);
    }
    else
    {
        baseplugin->TEXTMappingAdd(new cTEXTMapping("actor",actor));
    }
    textmap=baseplugin->TEXTMapping("adapter");
    if (textmap)
    {
        textmap->ChangeValue(adapter);
    }
    else
    {
        baseplugin->TEXTMappingAdd(new cTEXTMapping("adapter",adapter));
    }
    textmap=baseplugin->TEXTMapping("commentator");
    if (textmap)
    {
        textmap->ChangeValue(commentator);
    }
    else
    {
        baseplugin->TEXTMappingAdd(new cTEXTMapping("commentator",commentator));
    }
    textmap=baseplugin->TEXTMapping("composer");
    if (textmap)
    {
        textmap->ChangeValue(composer);
    }
    else
    {
        baseplugin->TEXTMappingAdd(new cTEXTMapping("composer",composer));
    }
    textmap=baseplugin->TEXTMapping("director");
    if (textmap)
    {
        textmap->ChangeValue(director);
    }
    else
    {
        baseplugin->TEXTMappingAdd(new cTEXTMapping("director",director));
    }
    textmap=baseplugin->TEXTMapping("editor");
    if (textmap)
    {
        textmap->ChangeValue(editor);
    }
    else
    {
        baseplugin->TEXTMappingAdd(new cTEXTMapping("editor",editor));
    }
    textmap=baseplugin->TEXTMapping("guest");
    if (textmap)
    {
        textmap->ChangeValue(guest);
    }
    else
    {
        baseplugin->TEXTMappingAdd(new cTEXTMapping("guest",guest));
    }
    textmap=baseplugin->TEXTMapping("presenter");
    if (textmap)
    {
        textmap->ChangeValue(presenter);
    }
    else
    {
        baseplugin->TEXTMappingAdd(new cTEXTMapping("presenter",presenter));
    }
    textmap=baseplugin->TEXTMapping("producer");
    if (textmap)
    {
        textmap->ChangeValue(producer);
    }
    else
    {
        baseplugin->TEXTMappingAdd(new cTEXTMapping("producer",producer));
    }
    textmap=baseplugin->TEXTMapping("writer");
    if (textmap)
    {
        textmap->ChangeValue(writer);
    }
    else
    {
        baseplugin->TEXTMappingAdd(new cTEXTMapping("writer",writer));
    }
    textmap=baseplugin->TEXTMapping("review");
    if (textmap)
    {
        textmap->ChangeValue(review);
    }
    else
    {
        baseplugin->TEXTMappingAdd(new cTEXTMapping("review",review));
    }
    textmap=baseplugin->TEXTMapping("season");
    if (textmap)
    {
        textmap->ChangeValue(season);
    }
    else
    {
        baseplugin->TEXTMappingAdd(new cTEXTMapping("season",season));
    }
    textmap=baseplugin->TEXTMapping("episode");
    if (textmap)
    {
        textmap->ChangeValue(episode);
    }
    else
    {
        baseplugin->TEXTMappingAdd(new cTEXTMapping("episode",episode));
    }

    SetupStore("textmap.country",country);
    SetupStore("textmap.date",date);
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
    SetupStore("textmap.review",review);
    SetupStore("textmap.season",season);
    SetupStore("textmap.episode",episode);
}

// --------------------------------------------------------------------------------------------------------

void cMenuSetupXmltv2vdrLog::output(void)
{
    int cur=Current();
    Clear();
    Add(NewTitle(tr("overview")));
    time_t lastexec=src->LastExecution();
    if (lastexec)
    {
        struct tm tm;
        localtime_r(&lastexec,&tm);
        strftime(lastexec_str,sizeof(lastexec_str)-1,"%d %b %H:%M:%S",&tm);
    }
    cString last;
    if (src->Active())
    {
        last=cString::sprintf("%s:\t%s",tr("last execution"),
                              tr("active"));
    }
    else
    {
        last=cString::sprintf("%s:\t%s",tr("last execution"),
                              lastexec ?  lastexec_str : "-");
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
    days=0;
    pin[0]=0;

    epgsrc=baseplugin->EPGSource(Index);
    if (!epgsrc) return;

    SetSection(cString::sprintf("%s '%s' : %s",trVDR("Plugin"), baseplugin->Name(), epgsrc->Name()));

    Add(NewTitle(tr("options")));
    days=epgsrc->DaysInAdvance();
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

void cMenuSetupXmltv2vdrChannelSource::Store(void)
{
    if ((!baseplugin) || (!sel) || (!epgsrc)) return;

    epgsrc->ChangeChannelSelection(sel);
    epgsrc->ChangeDaysInAdvance(days);
    if (epgsrc->NeedPin())
        epgsrc->ChangePin(pin);
    epgsrc->Store();

    cEPGChannels *channellist=epgsrc->ChannelList();
    if (channellist)
    {
        for (int i=0; i<channellist->Count(); i++)
        {
            if (channellist->Get(i)->InUse())
            {
                cEPGMapping *epgmap=baseplugin->EPGMapping(channellist->Get(i)->Name());
                if (epgmap)
                {
                    if (epgmap->Days()>days)
                    {
                        char *name=NULL;
                        if (asprintf(&name,"channel.%s",channellist->Get(i)->Name())==-1) return;

                        char *value=NULL;
                        if (asprintf(&value,"%i;%i;",days,epgmap->Flags())==-1)
                        {
                            free(name);
                            return;
                        }
                        for (int i=0; i<epgmap->NumChannelIDs(); i++)
                        {
                            cString ChannelID = epgmap->ChannelIDs()[i].ToString();
                            value=strcatrealloc(value,*ChannelID);
                            if (i<epgmap->NumChannelIDs()-1) value=strcatrealloc(value,";");
                        }
                        epgmap->ChangeDays(days);
                        SetupStore(name,value);
                        free(name);
                        free(value);
                    }
                }
            }
        }
    }
}

// --------------------------------------------------------------------------------------------------------

cMenuSetupXmltv2vdrChannelMap::cMenuSetupXmltv2vdrChannelMap(cPluginXmltv2vdr *Plugin, cMenuSetupXmltv2vdr *Menu, int Index)
{
    baseplugin=Plugin;
    menu=Menu;
    hasmaps=false;
    flags=0;
    days=1;
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
    days=map->Days();
    daysmax=getdaysmax();
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

cOsdItem *cMenuSetupXmltv2vdrChannelMap::optionN(const char *s, int num)
{
    cString buffer = cString::sprintf("%s:\t%i", s, num);
    return new cOsdItem(buffer,osUnknown,false);
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
        Add(optionN(tr("days in advance"),1),true);
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
        Add(new cMenuEditIntItem(tr("days in advance"),&days,1,daysmax),true);
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
    Add(new cMyMenuEditBitItem(tr("review"),&flags,USE_REVIEW),true);
    Add(new cMyMenuEditBitItem(tr("video"),&flags,USE_VIDEO),true);
    Add(new cMyMenuEditBitItem(tr("audio"),&flags,USE_AUDIO),true);
    Add(new cMyMenuEditBitItem(tr("vps"),&flags,OPT_VPS),true);

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
                    days=1;
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
        map->ChangeDays(newmapping->Days());
        map->ReplaceChannels(newmapping->NumChannelIDs(),newmapping->ChannelIDs());
    }
}

void cMenuSetupXmltv2vdrChannelMap::Store(void)
{
    if (!channel) return;
    char *name=NULL;
    if (asprintf(&name,"channel.%s",channel)==-1) return;

    char *value=NULL;
    if (asprintf(&value,"%i;%i;",days,flags)==-1)
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
    map->ChangeDays(days);
    epgmappingreplace(map);

    free(name);
    free(value);

    if (!baseplugin) return;
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
                        if (epgsrc->DaysInAdvance()<days)
                        {
                            epgsrc->ChangeDaysInAdvance(days);
                            epgsrc->Store();
                        }
                        break;
                    }
                }
            }
        }
    }

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

