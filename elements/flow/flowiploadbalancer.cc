/*
 * flowiploadbalancer.{cc,hh} -- TCP & UDP load-balancer
 * Tom Barbette
 *
 * Copyright (c) 2019-2020 KTH Royal Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/glue.hh>
#include <click/args.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <click/flow/flow.hh>

#include "flowiploadbalancer.hh"


CLICK_DECLS

FlowIPLoadBalancer::FlowIPLoadBalancer() : _accept_nonsyn(true)
{
}

FlowIPLoadBalancer::~FlowIPLoadBalancer()
{
}

int
FlowIPLoadBalancer::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String mode= "rr";
    if (Args(conf, this, errh)
       .read_all("DST",Args::mandatory | Args::positional,DefaultArg<Vector<IPAddress>>(),_dsts)
       .read_mp("VIP", _vip)
       .read("MODE", mode)
       .complete() < 0)
        return -1;
    click_chatter("%p{element} has %d routes",this,_dsts.size());

    set_mode(mode);
    click_chatter("MODE set to %s", mode.c_str());
    return 0;
}

int FlowIPLoadBalancer::initialize(ErrorHandler *errh)
{
    return 0;
}


bool FlowIPLoadBalancer::new_flow(IPLBEntry* flowdata, Packet* p)
{
    if (isTCP(p) && !isSyn(p)) {
        nat_info_chatter("Non syn establishment!");
        if (!_accept_nonsyn)
            return false;
    }
    int server = pick_server(p);

    flowdata->chosen_server = server;

    nat_debug_chatter("New flow %d",server);

    return true;
}


void FlowIPLoadBalancer::push_flow(int, IPLBEntry* flowdata, PacketBatch* batch)
{
    auto fnt = [this,flowdata](Packet*&p) -> bool {
        WritablePacket* q =p->uniqueify();
        p = q;

        nat_debug_chatter("Packet for flow %d", flowdata->chosen_server);
        IPAddress srv = _dsts[flowdata->chosen_server];

        q->ip_header()->ip_dst = srv;
        p->set_dst_ip_anno(srv);
        return true;
    };
    EXECUTE_FOR_EACH_PACKET_UNTIL_DROP(fnt, batch);

    if (batch)
        checked_output_push_batch(0, batch);
}


FlowIPLoadBalancerReverse::FlowIPLoadBalancerReverse() : _lb(0)
{
}

FlowIPLoadBalancerReverse::~FlowIPLoadBalancerReverse()
{
}

int
FlowIPLoadBalancerReverse::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Element* e;
    if (Args(conf, this, errh)
       .read_mp("LB",e)
       .complete() < 0)
        return -1;

    _lb = reinterpret_cast<FlowIPLoadBalancer*>(e);
    _lb->add_remote_element(this);
    return 0;
}


int FlowIPLoadBalancerReverse::initialize(ErrorHandler *errh)
{
    return 0;
}

void FlowIPLoadBalancerReverse::push_batch(int, PacketBatch* batch)
{
    auto fnt = [this](Packet* &p) -> bool {
        WritablePacket* q =p->uniqueify();
        p = q;
        q->ip_header()->ip_src = _lb->_vip;
        return true;
    };

    EXECUTE_FOR_EACH_PACKET_UNTIL_DROP(fnt, batch);

    if (batch)
        checked_output_push_batch(0, batch);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FlowIPLoadBalancerReverse)
ELEMENT_MT_SAFE(FlowIPLoadBalancerReverse)
EXPORT_ELEMENT(FlowIPLoadBalancer)
ELEMENT_MT_SAFE(FlowIPLoadBalancer)
