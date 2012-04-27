/*
 * parse.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef _PARSE_H
#define _PARSE_H

#include <vdr/epg.h>
#include <libxml/parser.h>
#include <time.h>

#include "maps.h"
#include "event.h"

class cEPGExecutor;
class cEPGSource;

class cParse
{
    enum
    {
        PARSE_NOERROR=0,
        PARSE_XMLTVERR,
        PARSE_NOMAPPING,
        PARSE_NOCHANNELID
    };

private:
    iconv_t cep2ascii;
    iconv_t cutf2ascii;
    const char *epdir;
    const char *epgfile;
    cEPGSource *source;
    cEPGMappings *maps;
    cXMLTVEvent xevent;
    time_t ConvertXMLTVTime2UnixTime(char *xmltvtime);
    bool FetchEvent(xmlNodePtr node);
public:
    cParse(const char *EPGFile, const char *EPDir, cEPGSource *Source, cEPGMappings *Maps);
    ~cParse();
    int Process(cEPGExecutor &myExecutor, char *buffer, int bufsize);
    static void RemoveNonAlphaNumeric(char *String);
    static bool FetchSeasonEpisode(iconv_t cEP2ASCII, iconv_t cUTF2ASCII, const char *EPDir,
                                   const char *Title, const char *ShortText, int &Season,
                                   int &Episode);
    static void InitLibXML();
    static void CleanupLibXML();
};

#endif
