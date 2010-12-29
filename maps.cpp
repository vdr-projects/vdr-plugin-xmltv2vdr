/*
 * maps.cpp: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "maps.h"
#include <limits.h>

cTEXTMapping::cTEXTMapping(const char *Name, const char *Value)
{
    name=strdup(Name);
    value=strdup(Value);
}

cTEXTMapping::~cTEXTMapping()
{
    if (name) free((void *) name);
    if (value) free((void *) value);
}

void cTEXTMapping::ChangeValue(const char *Value)
{
    if (value) free((void *) value);
    value=strdup(Value);
}

// --------------------------------------------------------------------------------------------------------

cEPGMapping::cEPGMapping(const char *ChannelName, const char *Flags_Days_and_Channels)
{
    channelname=strdup(ChannelName);

    dsyslog("xmltv2vdr: added mapping for '%s'",channelname);

    flags=USE_SHORTTEXT;
    days=1;
    channelids=NULL;
    numchannelids=0;

    if (Flags_Days_and_Channels)
    {
        char *flags_days_p=(char *) strdup(Flags_Days_and_Channels);
        if (!flags_days_p) return;

        char *flags_p=strchr(flags_days_p,';');
        if (flags_p)
        {
            *flags_p=0;
            flags_p++;
            days=atoi(flags_days_p);
            if (days<1) days=1;
            if (days>365) days=365;

            char *channels_p=strchr(flags_p,';');
            if (channels_p)
            {
                *channels_p=0;
                channels_p++;
                flags=atoi(flags_p);
                addchannels(channels_p);
            }
        }
        free(flags_days_p);
    }
}

cEPGMapping::~cEPGMapping()
{
    if (channelname) free((void *) channelname);
    if (channelids) free(channelids);
}

cEPGMapping::cEPGMapping(cEPGMapping&copy)
{
    channelname=strdup(copy.channelname);
    channelids=NULL;
    numchannelids=0;
    if (copy.numchannelids>0)
    {
        channelids=(tChannelID *) malloc((copy.numchannelids+1)*sizeof(tChannelID));
        if (!channelids) return;
        for (int i=0; i<copy.numchannelids; i++)
            channelids[i]=copy.channelids[i];
        numchannelids=copy.numchannelids;
    }
    flags=copy.flags;
    days=copy.days;
}

int cEPGMapping::compare(const void *a, const void *b)
{
    tChannelID *v1=(tChannelID *) a;
    tChannelID *v2=(tChannelID *) b;
    int num1=0,num2=0;
    if (*v1==tChannelID::InvalidID)
    {
        num1=INT_MAX;
    }
    else
    {
        cChannel *c1=Channels.GetByChannelID(*v1);
        if (c1) num1=c1->Number();
    }
    if (*v2==tChannelID::InvalidID)
    {
        num2=INT_MAX;
    }
    else
    {
        cChannel *c2=Channels.GetByChannelID(*v2);
        if (c2) num2=c2->Number();
    }
    if (num1>num2) return 1;
    else return -1;
}

void cEPGMapping::addchannels(const char *channels)
{
    char *tmp=(char *) strdup(channels);
    if (!tmp) return;

    char *token,*str1,*saveptr;
    str1=tmp;

    while (token=strtok_r(str1,";",&saveptr))
    {
        tChannelID ChannelID=tChannelID::FromString(token);
        if (!(ChannelID==tChannelID::InvalidID))
        {
            channelids=(tChannelID *) realloc(channelids,(numchannelids+1)*sizeof(tChannelID));
            if (!channelids)
            {
                free(tmp);
                return;
            }
            channelids[numchannelids]=ChannelID;
            numchannelids++;
        }
        str1=NULL;
    }
    free(tmp);
}

void cEPGMapping::AddChannel(int ChannelNumber)
{
    cChannel *chan=Channels.GetByNumber(ChannelNumber);
    if (chan)
    {
        bool found=false;
        for (int i=0; i<numchannelids; i++)
        {
            if (channelids[i]==chan->GetChannelID())
            {
                found=true;
                break;
            }
        }
        if (!found)
        {
            channelids=(tChannelID *) realloc(channelids,(numchannelids+1)*sizeof(tChannelID));
            if (!channelids) return;
            channelids[numchannelids]=chan->GetChannelID();
            numchannelids++;
            qsort(channelids,numchannelids,sizeof(tChannelID),compare);
        }
    }
}

void cEPGMapping::ReplaceChannels(int NumChannelIDs, tChannelID *ChannelIDs)
{
    if (NumChannelIDs<0) return;
    free(channelids);
    channelids=NULL;
    numchannelids=0;
    if (!NumChannelIDs) return;
    if (!ChannelIDs) return;

    for (int i=0; i<NumChannelIDs; i++)
    {
        channelids=(tChannelID *) realloc(channelids,(numchannelids+1)*sizeof(tChannelID));
        if (!channelids) return;
        channelids[numchannelids]=ChannelIDs[i];
        numchannelids++;
        qsort(channelids,numchannelids,sizeof(tChannelID),compare);
    }
}

void cEPGMapping::RemoveChannel(int ChannelNumber)
{
    if (!ChannelNumber) return;
    cChannel *chan=Channels.GetByNumber(ChannelNumber);
    if (!chan) return;

    bool found=false;
    int i;
    for (i=0; i<numchannelids; i++)
    {
        if (channelids[i]==chan->GetChannelID())
        {
            found=true;
            break;
        }
    }
    if (found)
    {
        channelids[i]=tChannelID::InvalidID;
        qsort(channelids,numchannelids,sizeof(tChannelID),compare);
        numchannelids--;
    }
}
