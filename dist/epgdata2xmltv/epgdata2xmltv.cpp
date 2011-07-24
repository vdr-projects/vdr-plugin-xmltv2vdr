/*
 * epgdata2xmltv.cpp: a grabber for the xmltv2vdr plugin
 *
 */

#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <locale.h>
#include <zip.h>
#include "epgdata2xmltv.h"
#include "epgdata2xmltv_xsl.h"

#include <fcntl.h>

#define xstr(s) str(s)
#define str(s) #s

int SysLogLevel=1;

void syslog_with_tid(const char *format, ...)
{
    va_list ap;
    char fmt[255];
    snprintf(fmt, sizeof(fmt), "%s", format);
    va_start(ap, format);
    vfprintf(stderr,fmt,ap);
    va_end(ap);
    fprintf(stderr,"\n");
    fflush(stderr);
}

cepgdata2xmltv::cepgdata2xmltv ()
{
    data.size = 0;
    data.fd=-1;
    pxsltStylesheet = NULL;
    sxmlDoc=NULL;
}

cepgdata2xmltv::~cepgdata2xmltv ()
{
    if (pxsltStylesheet)
    {
        xsltFreeStylesheet(pxsltStylesheet);
        xsltCleanupGlobals();
        xmlCleanupParser();
    }
}

void cepgdata2xmltv::LoadXSLT()
{
    if (pxsltStylesheet) return;
    xmlSubstituteEntitiesDefault (1);
    xmlLoadExtDtdDefaultValue = 1;
    exsltRegisterAll();

    if ((sxmlDoc = xmlReadMemory (xsl, sizeof(xsl), NULL,NULL,0)) != NULL)
    {
        pxsltStylesheet=xsltParseStylesheetDoc(sxmlDoc);
        if (!pxsltStylesheet)
        {
            esyslog("can't parse stylesheet");
            xmlFreeDoc (sxmlDoc);
            sxmlDoc=NULL;
        }
    }
}

static size_t WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *dataptr)
{
    struct data *data = (struct data *) dataptr;
    size_t realsize = size * nmemb;
    if (data->fd!=-1)
    {
        write(data->fd,ptr,realsize);
        data->size+=realsize;
    }
    return realsize;
}


int cepgdata2xmltv::DownloadData(const char *url)
{
    CURL *curl_handle;
    data.size=0;

    int ret;
    ret=curl_global_init(CURL_GLOBAL_NOTHING);
    if (ret) return ret;

    curl_handle = curl_easy_init();
    if (!curl_handle) return -40; // unused curl error
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);  // Specify URL to get
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 0);  // don't follow redirects
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);  // Send all data to this function
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) &data);  // Pass our 'data' struct to the callback function
    curl_easy_setopt(curl_handle, CURLOPT_MAXFILESIZE, 20971520);  // Set maximum file size to get (bytes)
    curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1);  // No progress meter
    curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);  // No signaling
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 240);  // Set timeout to 240 seconds
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, EPGDATA2XMLTV_USERAGENT);  // Some servers don't like requests that are made without a user-agent field

    ret=curl_easy_perform(curl_handle);
    if (!ret)
    {
        long code;
        curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &code);
        if (code==407)
        {
            ret=10;
        }
        else
        {
            if (code!=200) ret=22;
        }
    }

    if ((data.size<120) && (!ret)) ret=22;

    curl_easy_cleanup(curl_handle);  // Cleanup curl stuff
    curl_global_cleanup();
    return -ret;
}

int cepgdata2xmltv::Fetch(const char *dest, const char *pin, int day)
{
    char *url = NULL;
    char *filename = NULL;

    if (asprintf (&filename, "%i&pin=%s",day,pin)==-1)
    {
        esyslog("failed to allocate string");
        return 1;
    }
    if (asprintf (&url, EPGDATA2XMLTV_URL, filename) == -1)
    {
        esyslog("failed to allocate string");
        free(filename);
        return 1;
    }
    if (filename) free(filename);

    data.fd=open(dest,O_CREAT|O_TRUNC|O_WRONLY,0664);
    if (data.fd==-1)
    {
        esyslog("failed to open %s",dest);
        return 1;
    }

    int ret=DownloadData(url);
    close(data.fd);
    if (ret) unlink(dest);
    data.fd=-1;

    free (url);
    // -40 fatal curl error
    // -10 wrong proxy auth
    // -7 couldn't connect
    // -6 couldn't resolve host (proxy)
    // -22 not found
    if (ret==-40)
    {
        esyslog("fatal curl error");
        return 1;
    }
    if (ret==-28)
    {
        esyslog("timeout");
        return 1;
    }
    if (ret==-10)
    {
        esyslog("wrong proxy auth");
        return 1;
    }
    if (ret==-7)
    {
        esyslog("failed to connect");
        return 2;
    }
    if (ret==-6)
    {
        esyslog("failed to resolve host");
        return 2;
    }
    if (ret==-22)
    {
        esyslog("wrong pin");
        return 1;

    }
    return 0;
}

char *cepgdata2xmltv::strreplace(char *s, const char *s1, const char *s2)
{
    char *p = strstr(s, s1);
    if (p)
    {
        int of = p - s;
        int l  = strlen(s);
        int l1 = strlen(s1);
        int l2 = strlen(s2);
        if (l2 > l1)
            s = (char *)realloc(s, l + l2 - l1 + 1);
        char *sof = s + of;
        if (l2 != l1)
            memmove(sof + l2, sof + l1, l - of - l1 + 1);
        strncpy(sof, s2, l2);
    }
    return s;
}

int cepgdata2xmltv::Process(int argc, char *argv[])
{
    FILE *f=fopen("/var/lib/epgsources/epgdata2xmltv","r");
    if (!f)
    {
        esyslog("failed to open epgdata2xmltv config");
        return 1;
    }
    char *line=NULL,*lptr=NULL;
    size_t size;
    getline(&line,&size,f);
    getline(&line,&size,f);
    char *sc=strchr(line,';');
    if (sc)
    {
        *sc=0;
        sc++;
    }
    else
    {
        sc=line;
    }
    int daysmax=atoi(sc);
    if (daysmax<0) daysmax=1;
    int daysinadvance=atoi(argv[1]);
    if (daysinadvance<0) daysinadvance=1;
    if (daysinadvance>daysmax) daysinadvance=daysmax;

    bool head=false;
    char *xmlmem=NULL;

    time_t t=time(NULL);

    for (int day=0; day<=daysinadvance; day++)
    {
        time_t td=t+(day*86400);
        struct tm *tm;
        tm=localtime(&td);
        char vgl[10];
        sprintf(vgl,"%04i%02i%02i",tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday);

        char *dest=NULL;
        if (asprintf(&dest,"/tmp/epgdata_%02i.zip",day)==-1)
        {
            esyslog("failed to allocate string");
            continue;
        }

        bool ok=false;
        do
        {
            bool offline=true;
            struct stat statbuf;
            if (stat(dest,&statbuf)==-1)
            {
                if (Fetch(dest,argv[2],day))
                {
                    ok=true;
                    break;
                }
                offline=false;
            }

            struct zip *zip=zip_open(dest,0,NULL);
            if (!zip)
            {
                if (offline)
                {
                    if (unlink(dest)==-1)
                    {
                        esyslog("cannot unlink %s",dest);
                        ok=true;
                        break;
                    }
                    continue;
                }
                esyslog("failed to open %s",dest);
                ok=true;
                break;
            }

            int i=zip_name_locate(zip,"qy.dtd",ZIP_FL_NOCASE);
            if (i==-1)
            {
                if (offline)
                {
                    if (unlink(dest)==-1)
                    {
                        esyslog("cannot unlink %s",dest);
                        ok=true;
                        break;
                    }
                    continue;
                }
                esyslog("failed read qy.dtd in %s",dest);
                ok=true;
                break;
            }

            struct zip_file *zfile=zip_fopen_index(zip,i,0);
            if (!zfile)
            {
                if (offline)
                {
                    if (unlink(dest)==-1)
                    {
                        esyslog("cannot unlink %s",dest);
                        ok=true;
                        break;
                    }
                    continue;
                }
                esyslog("failed to read qy.dtd from %s",dest);
                ok=true;
                break;
            }
            struct zip_stat sb;
            memset(&sb,0,sizeof(sb));
            if (zip_stat_index(zip,i,ZIP_FL_UNCHANGED,&sb)==-1)
            {
                if (offline)
                {
                    if (unlink(dest)==-1)
                    {
                        zip_fclose(zfile);
                        esyslog("cannot unlink %s",dest);
                        ok=true;
                        break;
                    }
                    continue;
                }
                zip_fclose(zfile);
                esyslog("failed to stat qy.dtd in %s",dest);
                ok=true;
                break;
            }
            if (sizeof(sb.size>4)) sb.size &= 0x00FFFFFF; // just to be sure
            xmlmem=(char *) malloc(sb.size+1);
            int size=zip_fread(zfile,xmlmem,sb.size);
            if (size!=sb.size)
            {
                zip_fclose(zfile);
                esyslog("failed to read qy.dtd from %s",dest);
                ok=true;
                break;
            }
            xmlmem[size]=0;
            zip_fclose(zfile);
            xmlmem=strreplace(xmlmem,"?>\n","?>\n<!DOCTYPE pack [\n");

            int entries=zip_get_num_files(zip);
            for (int i=0; i<entries; i++)
            {
                const char *name=zip_get_name(zip,i,0);
                if (strstr(name,"xml"))
                {
                    // check date of xml
                    if (strstr(name,vgl))
                    {
                        struct zip_file *zfile=zip_fopen_index(zip,i,0);
                        if (!zfile)
                        {
                            if (offline)
                            {
                                if (unlink(dest)==-1)
                                {
                                    esyslog("cannot unlink %s",dest);
                                    ok=true;
                                    break;
                                }
                                continue;
                            }
                            esyslog("failed to read %s from %s",name,dest);
                            ok=true;
                            break;
                        }
                        struct zip_stat sb;
                        memset(&sb,0,sizeof(sb));
                        if (zip_stat_index(zip,i,ZIP_FL_UNCHANGED,&sb)==-1)
                        {
                            if (offline)
                            {
                                if (unlink(dest)==-1)
                                {
                                    esyslog("cannot unlink %s",dest);
                                    ok=true;
                                    break;
                                }
                                continue;
                            }
                            esyslog("failed to stat %s in %s",name,dest);
                            ok=true;
                            break;
                        }
                        if (sizeof(sb.size>4)) sb.size &= 0x00FFFFFF; // just to be sure
                        int lpos=strlen(xmlmem);
                        char *nptr=(char *) realloc(xmlmem,lpos+sb.size+1);
                        if (nptr)
                        {
                            xmlmem=nptr;
                        }
                        else
                        {
                            zip_fclose(zfile);
                            free(xmlmem);
                            xmlmem=NULL;
                            esyslog("out of memory");
                            ok=true;
                            break;
                        }
                        int size=zip_fread(zfile,xmlmem+lpos,sb.size);
                        if (size!=sb.size)
                        {
                            zip_fclose(zfile);
                            free(xmlmem);
                            xmlmem=NULL;
                            esyslog("failed to read %s from %s",name,dest);
                            ok=true;
                            break;
                        }
                        zip_fclose(zfile);
                        xmlmem[lpos+size]=0;
                        xmlmem[lpos++]=']';
                        xmlmem[lpos++]='>';
                        xmlmem[lpos++]='\n';
                        while (xmlmem[lpos]!='?')
                        {
                            xmlmem[lpos]=' ';
                            lpos++;
                        }
                        xmlmem[lpos++]=' ';
                        xmlmem[lpos++]=' ';
                        while (xmlmem[lpos]!='>')
                        {
                            xmlmem[lpos]=' ';
                            lpos++;
                        }
                        xmlmem[lpos++]=' ';
                        ok=true;
                    }
                }
            }
            zip_close(zip);
            if (!ok)
            {
                if (offline)
                {
                    if (unlink(dest)==-1)
                    {
                        ok=true;
                        break;
                    }
                    continue;
                }
                else
                {
                    esyslog("found no valid data in %s",dest);
                    if (xmlmem) free(xmlmem);
                    xmlmem=NULL;
                    ok=true;
                    break;
                }
            }
        }
        while (ok==false);
        free(dest);

        if (!line)
        {
            line=(char *) malloc(81);
            size=80;
        }
        if (!xmlmem) continue;
        long offset=ftell(f);

        xmlDocPtr pxmlDoc;
        if (!pxsltStylesheet) LoadXSLT();
        if ((pxmlDoc=xmlParseMemory(xmlmem,strlen(xmlmem)))==NULL)
        {
            esyslog("failed parsing xml");
            free(xmlmem);
            continue;
        }

        for (;;)
        {
            lptr=line+1;
            line[0]=' ';
            if (getline(&lptr,&size,f)==-1) break;
            char *channel=line;
            char *sc=strchr(channel,';');
            if (sc) *sc=0;

            bool use=false;
            for (int i=3; i<argc; i++)
            {
                if (!strcasecmp(lptr,argv[i]))
                {
                    use=true;
                    break;
                }
            }

            if (use)
            {
                if (!head)
                {
                    printf("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
                    printf("<tv generator-info-name=\"epgdata2xmltv\">\n");

                    for (int i=3; i<argc; i++)
                    {
                        printf("<channel id=\"%s\">\n",argv[i]);
                        printf("<display-name lang=\"de\">%s</display-name>\n",argv[i]);
                        printf("</channel>\n");
                    }
                    head=true;
                }

                int num=atoi(sc+1);
                if (num>0)
                {
                    char *channelnum=strdup(sc+1);
                    char *lf=strchr(channelnum,10);
                    if (lf) *lf=0;
                    channel[0]='"';
                    *sc++='"';
                    *sc=0;
                    const char *params[5] =
                    {
                        "channelid", channel, "channelnum",channelnum,NULL
                    };
                    Translate(pxmlDoc,params);
                    if (channelnum) free(channelnum);
                }
            }
        }
        xmlFreeDoc (pxmlDoc);
        fseek(f,offset,SEEK_SET);
        if (xmlmem) free(xmlmem);
        xmlmem=NULL;
    }
    if (line) free(line);
    fclose(f);

    if (head) printf("</tv>\n");
    return head ? 0 : 1;
}

bool cepgdata2xmltv::Translate(xmlDocPtr pxmlDoc, const char **params)
{
    xmlDocPtr res=NULL;
    if ((res = xsltApplyStylesheet (pxsltStylesheet, pxmlDoc, (const char **)params)) == NULL)
    {
        dsyslog("error applying xslt stylesheet");
        return false;
    }
    else
    {
        xsltSaveResultToFile(stdout,res,pxsltStylesheet);
        xmlFreeDoc (res);
    }
    return true;
}

int main(int argc, char *argv[])
{
    if (argc<4) return 132;
    cepgdata2xmltv *epgdata2xmltv = new cepgdata2xmltv;
    int ret=epgdata2xmltv->Process(argc, argv);
    delete epgdata2xmltv;
    return ret;
}

