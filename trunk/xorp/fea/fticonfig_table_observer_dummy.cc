// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-

// Copyright (c) 2001-2003 International Computer Science Institute
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

#ident "$XORP: xorp/fea/fticonfig_table_observer_rtsock.cc,v 1.1 2003/05/02 23:21:37 pavlin Exp $"


#include "fea_module.h"
#include "libxorp/xorp.h"
#include "libxorp/xlog.h"
#include "libxorp/debug.h"

#include "fticonfig.hh"
#include "fticonfig_table_observer.hh"


//
// Observe whole-table information change about the unicast forwarding table.
//
// E.g., if the forwarding table has changed, then the information
// received by the observer would NOT specify the particular entry that
// has changed.
//
// The mechanism to set the information is dummy (for testing purpose).
//


FtiConfigTableObserverDummy::FtiConfigTableObserverDummy(FtiConfig& ftic)
    : FtiConfigTableObserver(ftic)
{
#if 0	// XXX: by default Dummy is never registering by itself
    register_ftic();
#endif
}

FtiConfigTableObserverDummy::~FtiConfigTableObserverDummy()
{
    stop();
}

int
FtiConfigTableObserverDummy::start()
{
    // TODO: XXX: PAVPAVPAV: implement it!
    
    return (XORP_OK);
}
    
int
FtiConfigTableObserverDummy::stop()
{
    // TODO: XXX: PAVPAVPAV: implement it!
    
    return (XORP_OK);
}
