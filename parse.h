/*
 * parse.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef _PARSE_H
#define _PARSE_H

#include <vdr/tools.h>
#include <vdr/epg.h>
#include <libxml/parser.h>
#include <time.h>

#include "maps.h"

class cEPGExecutor;
class cEPGSource;

class cXMLTVEvent
{
private:
    char *title;
    char *shorttext;
    char *description;
    char *country;
    char *system;
    char *review;
    char *rating;
    char *origtitle;
    int year;
    time_t starttime;
    int duration;
    time_t vps;
    int season;
    int episode;
    tEventID eventid;
    cStringList credits;
    cStringList categories;
#if VDRVERSNUM >= 10711
    uchar parentalRating;
    uchar contents[MaxEventContents];
#endif
public:
    cXMLTVEvent();
    ~cXMLTVEvent();
    bool RemoveLastCharFromDescription();
    bool Add2Description(const char *Name, const char *Value);
    bool Add2Description(const char *Name, int Value);
    bool Add2Description(const char *Value);
    void Clear();
    void SetTitle(const char *Title);
    void SetOrigTitle(const char *OrigTitle);
    void SetShortText(const char *ShortText);
    void SetDescription(const char *Description);
    void AddCredits(const char *CreditType, const char *Credit, const char *Addendum=NULL);
    void AddCategory(const char *Category);
    void SetCountry(const char *Country);
    void SetReview(const char *Review);
    void SetRating(const char *System, const char *Rating);
    void SetSeason(int Season)
    {
        season=Season;
    }
    void SetEpisode(int Episode)
    {
        episode=Episode;
    }
    void SetYear(int Year)
    {
        year=Year;
    }
    void SetStartTime(time_t StartTime)
    {
        starttime=StartTime;
    }
    void SetDuration(int Duration)
    {
        duration=Duration;
    }
    void SetVps(time_t Vps)
    {
        vps=Vps;
    }
    void SetEventID(tEventID EventID)
    {
        eventid=EventID;
    }
    time_t Vps(void) const
    {
        return vps;
    }
    int Duration() const
    {
        return duration;
    }
    time_t StartTime() const
    {
        return starttime;
    }
    const char *Title(void) const
    {
        return title;
    }
    const char *ShortText(void) const
    {
        return shorttext;
    }
    const char *Description(void) const
    {
        return description;
    }
    const char *Country(void) const
    {
        return country;
    }
    int Year() const
    {
        return year;
    }
    const char *Review(void) const
    {
        return review;
    }
    const char *Rating(void) const
    {
        return rating;
    }
    const char *RatingSystem(void) const
    {
        return system;
    }
    const char *OrigTitle(void) const
    {
        return origtitle;
    }
    tEventID EventID(void) const
    {
        return eventid;
    }
    cStringList *Credits(void)
    {
        return &credits;
    }
    cStringList *Categories(void)
    {
        return &categories;
    }
    int Season(void)
    {
        return season;
    }
    int Episode(void)
    {
        return episode;
    }
};

class cParse
{
    struct split
    {
        char *pointers[256];
        int count;
    };

    enum
    {
        PARSE_NOERROR=0,
        PARSE_NOSCHEDULE,
        PARSE_NOCHANNEL,
        PARSE_XMLTVERR,
        PARSE_NOMAPPING,
        PARSE_NOCHANNELID,
        PARSE_EMPTYSCHEDULE
    };

private:
    cEPGSource *source;
    cEPGMappings *maps;
    cTEXTMappings *texts;
    cXMLTVEvent xevent;
    cCharSetConv *conv;
    char *epdir;
    char cbuf[80];
    char *RemoveNonASCII(const char *src);
    struct split split(char *in, char delim);
    u_long DoSum(u_long sum, const char *buf, int nBytes);
    cEvent *SearchEvent(cSchedule* schedule, cXMLTVEvent *xevent);
    time_t ConvertXMLTVTime2UnixTime(char *xmltvtime);
    bool FetchEvent(xmlNodePtr node);
    void FetchSeasonEpisode(cEvent *event);
    cEPGMapping *EPGMapping(const char *ChannelName);
    cTEXTMapping *TEXTMapping(const char *Name);
    bool PutEvent(cSchedule* schedule,cEvent *event,cXMLTVEvent *xevent, cEPGMapping *map);
    cEvent *GetEventBefore(cSchedule *schedule, time_t start);
public:
    cParse(cEPGSource *Source, cEPGMappings *Maps, cTEXTMappings *Texts);
    ~cParse();
    int Process(cEPGExecutor &myExecutor, char *buffer, int bufsize);
    static void InitLibXML();
    static void CleanupLibXML();
};

#endif
