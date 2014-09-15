/*
** Copyright (C) 2014 Cisco and/or its affiliates. All rights reserved.
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
// ips_tag.cc author Russ Combs <rucombs@cisco.com>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "snort_types.h"
#include "protocols/packet.h"
#include "snort_debug.h"
#include "snort.h"
#include "profiler.h"
#include "sfhashfcn.h"
#include "detection/detection_defines.h"
#include "framework/ips_option.h"
#include "framework/parameter.h"
#include "framework/module.h"
#include "framework/range.h"

static const char* s_name = "window";

static THREAD_LOCAL ProfileStats tcpWinPerfStats;

class TcpWinOption : public IpsOption
{
public:
    TcpWinOption(const RangeCheck& c) :
        IpsOption(s_name)
    { config = c; };

    uint32_t hash() const;
    bool operator==(const IpsOption&) const;

    int eval(Cursor&, Packet*);

private:
    RangeCheck config;
};

//-------------------------------------------------------------------------
// class methods
//-------------------------------------------------------------------------

uint32_t TcpWinOption::hash() const
{
    uint32_t a,b,c;

    a = config.op;
    b = config.min;
    c = config.max;

    mix_str(a,b,c,get_name());
    final(a,b,c);

    return c;
}

bool TcpWinOption::operator==(const IpsOption& ips) const
{
    if ( strcmp(get_name(), ips.get_name()) )
        return false;

    TcpWinOption& rhs = (TcpWinOption&)ips;
    return ( config == rhs.config );
}

int TcpWinOption::eval(Cursor&, Packet *p)
{
    int rval = DETECTION_OPTION_NO_MATCH;
    PROFILE_VARS;

    if(!p->ptrs.tcph)
        return rval;

    MODULE_PROFILE_START(tcpWinPerfStats);

    if ( config.eval(p->ptrs.tcph->th_win) )
        rval = DETECTION_OPTION_MATCH;

    MODULE_PROFILE_END(tcpWinPerfStats);
    return rval;
}

//-------------------------------------------------------------------------
// module
//-------------------------------------------------------------------------

static const Parameter s_params[] =
{
    { "~range", Parameter::PT_STRING, nullptr, nullptr,
      "check if packet payload size is min<>max | <max | >min" },

    { nullptr, Parameter::PT_MAX, nullptr, nullptr, nullptr }
};

static const char* s_help =
    "rule option to check TCP window field";

class WindowModule : public Module
{
public:
    WindowModule() : Module(s_name, s_help, s_params) { };

    bool begin(const char*, int, SnortConfig*);
    bool set(const char*, Value&, SnortConfig*);

    ProfileStats* get_profile() const
    { return &tcpWinPerfStats; };

    RangeCheck data;
};

bool WindowModule::begin(const char*, int, SnortConfig*)
{
    data.init();
    return true;
}

bool WindowModule::set(const char*, Value& v, SnortConfig*)
{
    if ( !v.is("~range") )
        return false;

    return data.parse(v.get_string());
}

//-------------------------------------------------------------------------
// api methods
//-------------------------------------------------------------------------

static Module* mod_ctor()
{
    return new WindowModule;
}

static void mod_dtor(Module* m)
{
    delete m;
}

static IpsOption* window_ctor(Module* p, OptTreeNode*)
{
    WindowModule* m = (WindowModule*)p;
    return new TcpWinOption(m->data);
}

static void window_dtor(IpsOption* p)
{
    delete p;
}

static const IpsApi window_api =
{
    {
        PT_IPS_OPTION,
        s_name,
        s_help,
        IPSAPI_PLUGIN_V0,
        0,
        mod_ctor,
        mod_dtor
    },
    OPT_TYPE_DETECTION,
    1, PROTO_BIT__TCP,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    window_ctor,
    window_dtor,
    nullptr
};

#ifdef BUILDING_SO
SO_PUBLIC const BaseApi* snort_plugins[] =
{
    &window_api.base,
    nullptr
};
#else
const BaseApi* ips_window = &window_api.base;
#endif

