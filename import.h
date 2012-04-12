/*
 * import.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef _IMPORT_H
#define _IMPORT_H

#include <vdr/epg.h>
#include <vdr/channels.h>
#include <sqlite3.h>

#include "event.h"
#include "source.h"
#include "maps.h"

enum
{
    IMPORT_ALL=0,
    IMPORT_DESCRIPTION,
    IMPORT_SHORTTEXT
};

class cEPGSource;
class cEPGExecutor;

class cImport
{
private:
    struct split
    {
        char *pointers[256];
        int count;
    };
    enum
    {
        IMPORT_NOERROR=0,
        IMPORT_NOSCHEDULE,
        IMPORT_NOCHANNEL,
        IMPORT_XMLTVERR,
        IMPORT_NOMAPPING,
        IMPORT_NOCHANNELID,
        IMPORT_EMPTYSCHEDULE
    };
    cEPGMappings *maps;
    cEPGSource *source;
    cTEXTMappings *texts;
    cCharSetConv *conv;
    bool pendingtransaction;
    const char *epgfile;
    char *RemoveLastCharFromDescription(char *description);
    char *Add2Description(char *description, const char *value);
    char *Add2Description(char *description, const char *name, const char *value);
    char *Add2Description(char *description, const char *name, int value);
    char *AddEOT2Description(char *description);
    struct split split(char *in, char delim);
    cEvent *GetEventBefore(cSchedule* schedule, time_t start);
    cEvent *SearchVDREvent(cSchedule* schedule, cXMLTVEvent *event);
    bool FetchXMLTVEvent(sqlite3_stmt *stmt, cXMLTVEvent *xevent);
    char *RemoveNonASCII(const char *src);
    cXMLTVEvent *PrepareAndReturn(sqlite3 *db, char *sql, sqlite3_stmt *stmt);
public:
    cImport(cEPGSource *Source, cEPGMappings *Maps, cTEXTMappings *Texts);
    ~cImport();
    int Process(cEPGExecutor &myExecutor);
    void Commit(sqlite3 *Db);
    bool WasChanged(cEvent *Event);
    bool PutEvent(cEPGSource *source, sqlite3 *db, cSchedule* schedule, cEvent *event,
                  cXMLTVEvent *xevent, int Flags, int Option=IMPORT_ALL);
    cXMLTVEvent *SearchXMLTVEvent(sqlite3 **Db, const char *ChannelID, const cEvent *Event);
    void UpdateXMLTVEvent(cEPGSource *Source, sqlite3 *Db, const cEvent *Event,
                          const char *SourceName, tEventID EventID, tEventID EITEventID,
                          const char *EITDescription=NULL);
    cXMLTVEvent *AddXMLTVEvent(const char *EPGFile, const char *ChannelID, const cEvent *Event,
                               const char *EITDescription);
};

#endif
