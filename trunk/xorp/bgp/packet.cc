// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-

// Copyright (c) 2001,2002 International Computer Science Institute
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software")
// to deal in the Software without restriction, subject to the conditions
// listed in the XORP LICENSE file. These conditions include: you must
// preserve this copyright notice, and you cannot mention the copyright
// holders in advertising related to the Software without their permission.
// The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
// notice is a summary of the XORP LICENSE file; the license in that file is
// legally binding.

#ident "$XORP: xorp/bgp/packet.cc,v 1.3 2003/01/16 23:18:57 pavlin Exp $"

// #define DEBUG_LOGGING
// #define DEBUG_PRINT_FUNCTION_NAME

#include "bgp_module.h"
#include "packet.hh"

/* **************** BGPPacket *********************** */

BGPPacket::BGPPacket()
{
    debug_msg("BGPPacket constructor called\n");
    memset(_Marker, 255, sizeof(_Marker));
}

BGPPacket::~BGPPacket()
{
    debug_msg("BGPPacket destructor called\n");
}

void
BGPPacket::set_marker(uint8_t * m)
{
    debug_msg("Packet marker set\n");
    memcpy(_Marker, m, MARKER_SIZE);
}

const uint8_t *
BGPPacket::flatten(struct iovec *iov, const int cnt, int& len) const
{
    len = 0;
    for (int i = 0; i < cnt; i++) {
	debug_msg("length = %d\n",  (iov + i)->iov_len);
	len += (iov + i)->iov_len;
    }

    uint8_t *ptr = new uint8_t[len];

    uint8_t *base = ptr;
    for (int i = 0; i < cnt; i++) {
	memcpy(base, (iov + i)->iov_base, (iov + i)->iov_len);
	base += (iov + i)->iov_len;
    }

    return ptr;
}


