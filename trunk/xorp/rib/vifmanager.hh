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

// $XORP: xorp/rib/vifmanager.hh,v 1.12 2003/06/05 02:39:36 pavlin Exp $

#ifndef __RIB_VIFMANAGER_HH__
#define __RIB_VIFMANAGER_HH__

#include <map>

#include "libxorp/timer.hh"

#include "libproto/proto_state.hh"

#include "xrl/interfaces/fea_ifmgr_xif.hh"


#define IF_EVENT_CREATED 1
#define IF_EVENT_DELETED 2
#define IF_EVENT_CHANGED 3

class EventLoop;
class RibManager;
class Vif;
class XrlRouter;

/**
 * @short VifManager keeps track of the VIFs currently enabled in the FEA.
 *
 * The RIB process has a single VifManager instance, which registers
 * with the FEA process to discover the VIFs on this router and their
 * IP addresses and prefixes. When the VIFs or their configuration in
 * the FEA change, the VifManager will be notified, and it will update
 * the RIBs appropriately. The RIBs need to know about VIFs and VIF
 * addresses to decide which routes have nexthops that are on directly
 * connected subnets, and which are nexthops that need to be resolved
 * using other routes to figure out where next to send the packet.
 * Only routes with nexthops that are on directly connected subnets
 * can be sent to the FEA. 
 */
class VifManager : public ProtoState {
public:
    /**
     * VifManager constructor
     *
     * @param xrl_router this process's XRL router.
     * @param eventloop this process's EventLoop.
     * @param rib_manager this class contains the actual RIBs for IPv4
     * and IPv4, unicast and multicast.
     */
    VifManager(XrlRouter& xrl_router, EventLoop& eventloop,
	       RibManager* rib_manager);

    /**
     * VifManager destructor
     */
    ~VifManager();

    /**
     * Start operation.
     * 
     * Start the process of registering with the FEA, etc.
     * 
     * @return XORP_OK on success, otherwise XORP_ERROR.
     */
    int	start();

    /**
     * Stop operation.
     * 
     * Gracefully stop operation.
     * 
     * @return XORP_OK on success, otherwise XORP_ERROR.
     */
    int	stop();

    /**
     * Set test-mode - don't try to communicate with the FEA.
     */
    void no_fea() { _no_fea = true; }

    /**
     * The state of the VifManager. It it hasn't yet successfully
     * registered with the FEA, it will be in INITIALIZING state. If
     * it has permanently failed to register with the FEA it will be
     * in FAILED state. Otherwise it should be in READY state.
     */
    enum State { INITIALIZING, READY, FAILED };

    /**
     * Get the state of the VifManager.
     * 
     * @return the state of the VifManager. 
     * @see VifManager::State.
     */
    State state() const { return _state; }

    /**
     * Update the status of a physical interface.
     * 
     * This method is called when receiving an XRL from the FEA
     * indicating that a physical interface on the router has been added,
     * deleted, or reconfigured.
     * 
     * @param ifname the name of the physical interface that changed.
     * @param event the event that occured. Should be one of the following:
     * IF_EVENT_CREATED, IF_EVENT_DELETED, or IF_EVENT_CHANGED.
     */
    void interface_update(const string& ifname, const uint32_t& event);

    /**
     * Update the status of a virtual interface.
     * 
     * This method is called when receiving an XRL from the FEA
     * indicating that a virtual interface on the router has been added,
     * deleted, or reconfigured.
     * 
     * @param ifname the name of the physical interface on which the
     * virtual interface resides.
     * @param vifname the name of the virtual interface that changed.
     * @param event the event that occured. Should be one of the following:
     * IF_EVENT_CREATED, IF_EVENT_DELETED, or IF_EVENT_CHANGED.
     */
    void vif_update(const string& ifname, const string& vifname,
		    const uint32_t& event);

    /**
     * Update the IPv4 address of a virtual interface.
     * 
     * This method is called when receiving an XRL from the FEA
     * indicating that a virtual interface has undergone an address
     * change. An IPv4 address (and associated prefix length) has
     * been added, deleted, or reconfigured on this VIF.
     * 
     * @param ifname the name of the interface containing the VIF.
     * @param vifname the name of the VIF on which the address change occured.
     * @param addr the address that was added or deleted.
     * @param event the event that occured. Should be one of the following:
     * IF_EVENT_CREATED or IF_EVENT_DELETED.
     */
    void vifaddr4_update(const string& ifname,
			 const string& vifname,
			 const IPv4& addr,
			 const uint32_t& event);

    /**
     * Update the IPv6 address of a virtual interface.
     * 
     * This method is called when receiving an XRL from the FEA
     * indicating that a virtual interface has undergone an address
     * change. An IPv6 address (and associated prefix length) has
     * been added, deleted, or reconfigured on this VIF.
     *
     * @param ifname the name of the interface containing the VIF.
     * @param vifname the name of the VIF on which the address change occured.
     * @param addr the address that was added or deleted.
     * @param event the event that occured. Should be one of the following:
     * IF_EVENT_CREATED or IF_EVENT_DELETED.
     */
    void vifaddr6_update(const string& ifname,
			 const string& vifname,
			 const IPv6& addr,
			 const uint32_t& event);

private:
    void update_state();
    void set_vif_state();
    
    void clean_out_old_state();
    void xrl_result_unregister_client(const XrlError& e);
    void register_if_spy();
    void xrl_result_register_client(const XrlError& e);
    void xrl_result_get_configured_interface_names(const XrlError& e,
						   const XrlAtomList* alist);
    void xrl_result_get_configured_vif_names(const XrlError& e,
					     const XrlAtomList* alist,
					     string ifname);
    void xrl_result_get_configured_vif_flags(const XrlError& e,
					     const bool* enabled,
					     const bool* broadcast,
					     const bool* loopback,
					     const bool* point_to_point,
					     const bool* multicast,
					     string ifname,
					     string vifname);
    void xrl_result_get_configured_vif_addresses4(const XrlError& e,
						  const XrlAtomList* alist,
						  string ifname,
						  string vifname);
    void xrl_result_get_configured_vif_addresses6(const XrlError& e,
						  const XrlAtomList* alist,
						  string ifname,
						  string vifname);
    void interface_deleted(const string& ifname);
    void vif_deleted(const string& ifname, const string& vifname);
    void vif_created(const string& ifname, const string& vifname);
    void vifaddr4_created(const string& ifname, const string& vifname,
			  const IPv4& addr);
    void vifaddr6_created(const string& ifname, const string& vifname,
			  const IPv6& addr);
    void xrl_result_get_configured_address_flags4(const XrlError& e,
						  const bool* enabled,
						  const bool* broadcast,
						  const bool* loopback,
						  const bool* point_to_point,
						  const bool* multicast,
						  string ifname,
						  string vifname,
						  IPv4 addr);
    void xrl_result_get_configured_address_flags6(const XrlError& e,
						  const bool* enabled,
						  const bool* loopback,
						  const bool* point_to_point,
						  const bool* multicast,
						  string ifname,
						  string vifname,
						  IPv6 addr);
    void xrl_result_get_configured_prefix4(const XrlError& e,
					   const uint32_t* prefix_len,
					   string ifname, string vifname,
					   IPv4 addr);
    void xrl_result_get_configured_prefix6(const XrlError& e,
					   const uint32_t* prefix_len,
					   string ifname, string vifname,
					   IPv6 addr);
    void xrl_result_get_configured_broadcast4(const XrlError& e,
					      const IPv4* broadcast,
					      string ifname, string vifname,
					      IPv4 addr);
    void xrl_result_get_configured_endpoint4(const XrlError& e,
					     const IPv4* endpoint,
					     string ifname, string vifname,
					     IPv4 addr);
    void xrl_result_get_configured_endpoint6(const XrlError& e,
					     const IPv6* endpoint,
					     string ifname, string vifname,
					     IPv6 addr);
    void vifaddr4_deleted(const string& ifname, const string& vifname,
			  const IPv4& addr);
    void vifaddr6_deleted(const string& ifname, const string& vifname,
			  const IPv6& addr);

    XrlRouter&		_xrl_router;
    EventLoop&		_eventloop;
    RibManager*		_rib_manager;
    XrlIfmgrV0p1Client	_ifmgr_client;
    
    bool		_no_fea;
    XorpTimer		_register_retry_timer;
    State		_state;
    
    // The following variables keep track of how many answers we're
    // still expecting from various pipelined queries to the FEA.
    size_t		_interfaces_remaining;
    size_t		_vifs_remaining;
    size_t		_addrs_remaining;
    
    // The maps with the interfaces and vifs
    map<string, Vif* >	_vifs_by_name;
    multimap<string, Vif* > _vifs_by_interface;
    
    map<string, Vif* >	_saved_vifs_by_name;	// The local copy
    
    string _fea_target_name;	// The FEA target name
};

#endif // __RIB_VIFMANAGER_HH__
