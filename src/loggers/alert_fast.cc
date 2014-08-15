/*
** Copyright (C) 2014 Cisco and/or its affiliates. All rights reserved.
** Copyright (C) 2002-2013 Sourcefire, Inc.
** Copyright (C) 1998-2002 Martin Roesch <roesch@sourcefire.com>
** Copyright (C) 2000,2001 Andrew R. Baker <andrewb@uab.edu>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License Version 2 as
** published by the Free Software Foundation.  You may not use, modify or
** distribute this program under any other version of the GNU General
** Public License.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/* alert_fast
 *
 * Purpose:  output plugin for fast alerting
 *
 * Arguments:  alert file
 *
 * Effect:
 *
 * Alerts are written to a file in the snort fast alert format
 *
 * Comments:   Allows use of fast alerts with other output plugin types
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <string>

#include "framework/logger.h"
#include "framework/module.h"
#include "event.h"
#include "protocols/packet.h"
#include "snort_debug.h"
#include "parser.h"
#include "util.h"
#include "mstring.h"
#include "packet_io/active.h"
#include "sf_textlog.h"
#include "log_text.h"
#include "sf_textlog.h"
#include "snort.h"
#include "packet_io/sfdaq.h"
#include "packet_io/intf.h"

/* full buf was chosen to allow printing max size packets
 * in hex/ascii mode:
 * each byte => 2 nibbles + space + ascii + overhead
 */
#define FULL_BUF (4*IP_MAXPACKET)
#define FAST_BUF (4*K_BYTES)

static THREAD_LOCAL TextLog* fast_log = nullptr;

using namespace std;

//-------------------------------------------------------------------------
// module stuff
//-------------------------------------------------------------------------

static const Parameter fast_params[] =
{
    { "file", Parameter::PT_STRING, nullptr, "stdout",
      "name of alert file" },

    { "packet", Parameter::PT_BOOL, nullptr, "true",
      "output packet dump with alert" },

    { "limit", Parameter::PT_INT, "0:", "0",
      "set limit (0 is unlimited)" },

    { "units", Parameter::PT_ENUM, "B | K | M | G", "B",
      "bytes | KB | MB | GB" },

    { nullptr, Parameter::PT_MAX, nullptr, nullptr, nullptr }
};

class FastModule : public Module
{
public:
    FastModule() : Module("alert_fast", fast_params) { };
    bool set(const char*, Value&, SnortConfig*);
    bool begin(const char*, int, SnortConfig*);
    bool end(const char*, int, SnortConfig*);

public:
    string file;
    unsigned long limit;
    unsigned units;
    bool packet;
};

bool FastModule::set(const char*, Value& v, SnortConfig*)
{
    if ( v.is("file") )
        file = v.get_string();

    else if ( v.is("packet") )
        packet = v.get_bool();

    else if ( v.is("limit") )
        limit = v.get_long();

    else if ( v.is("units") )
        units = v.get_long();

    else
        return false;

    return true;
}

bool FastModule::begin(const char*, int, SnortConfig*)
{
    file = "stdout";
    limit = 0;
    units = 0;
    packet = true;
    return true;
}

bool FastModule::end(const char*, int, SnortConfig*)
{
    while ( units-- )
        limit *= 1024;

    return true;
}

//-------------------------------------------------------------------------
// logger stuff
//-------------------------------------------------------------------------

class FastLogger : public Logger {
public:
    FastLogger(FastModule*);

    void open();
    void close();

    void alert(Packet*, const char* msg, Event*);

private:
    string file;
    unsigned long limit;
    bool packet;
};

FastLogger::FastLogger(FastModule* m)
{
    file = m->file;
    limit = m->limit;
    packet = m->packet;
}

void FastLogger::open()
{
    unsigned sz = packet ? FULL_BUF : FAST_BUF;
    fast_log = TextLog_Init(file.c_str(), sz, limit);
}

void FastLogger::close()
{
    if ( fast_log )
        TextLog_Term(fast_log);
}

void FastLogger::alert(Packet *p, const char *msg, Event *event)
{
    LogTimeStamp(fast_log, p);

    if( Active_PacketWasDropped() )
    {
        TextLog_Puts(fast_log, " [Drop]");
    }
    else if( Active_PacketWouldBeDropped() )
    {
        TextLog_Puts(fast_log, " [WDrop]");
    }

    {
#ifdef MARK_TAGGED
        char c=' ';
        if (p->packet_flags & PKT_REBUILT_STREAM)
            c = 'R';
        else if (p->packet_flags & PKT_REBUILT_FRAG)
            c = 'F';
        TextLog_Print(fast_log, " [**] %c ", c);
#else
        TextLog_Puts(fast_log, " [**] ");
#endif

        if(event != NULL)
        {
            TextLog_Print(fast_log, "[%lu:%lu:%lu] ",
                    (unsigned long) event->sig_info->generator,
                    (unsigned long) event->sig_info->id,
                    (unsigned long) event->sig_info->rev);
        }

        if (ScAlertInterface())
        {
            TextLog_Print(fast_log, " <%s> ", PRINT_INTERFACE(DAQ_GetInterfaceSpec()));
        }

        if (msg != NULL)
        {
            TextLog_Puts(fast_log, msg);
            TextLog_Puts(fast_log, " [**] ");
        }
        else
        {
            TextLog_Puts(fast_log, "[**] ");
        }
    }

    /* print the packet header to the alert file */
    if (p->ip_api.is_valid())
    {
        LogPriorityData(fast_log, event, 0);
        TextLog_Print(fast_log, "{%s} ", protocol_names[p->ip_api.proto()]);
        LogIpAddrs(fast_log, p);
    }

    if(packet)
    {
        if(p->ip_api.is_valid())
            LogIPPkt(fast_log, p->ip_api.proto(), p);
#ifndef NO_NON_ETHER_DECODER
        else if(p->proto_bits & PROTO_BIT__ARP)
            LogArpHeader(fast_log, p);
#endif
    }
    TextLog_NewLine(fast_log);
    TextLog_Flush(fast_log);
}

//-------------------------------------------------------------------------
// api stuff
//-------------------------------------------------------------------------

static Module* mod_ctor()
{ return new FastModule; }

static void mod_dtor(Module* m)
{ delete m; }

static Logger* fast_ctor(SnortConfig*, Module* mod)
{ return new FastLogger((FastModule*)mod); }

static void fast_dtor(Logger* p)
{ delete p; }

static LogApi fast_api
{
    {
        PT_LOGGER,
        "alert_fast",
        LOGAPI_PLUGIN_V0,
        0,
        mod_ctor,
        mod_dtor
    },
    OUTPUT_TYPE_FLAG__ALERT,
    fast_ctor,
    fast_dtor
};

#ifdef BUILDING_SO
SO_PUBLIC const BaseApi* snort_plugins[] =
{
    &fast_api.base,
    nullptr
};
#else
const BaseApi* alert_fast = &fast_api.base;
#endif

