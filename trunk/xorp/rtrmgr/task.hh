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

// $XORP: xorp/rtrmgr/task.hh,v 1.24 2004/05/18 00:05:13 pavlin Exp $

#ifndef __RTRMGR_TASK_HH__
#define __RTRMGR_TASK_HH__

#include <map>
#include <vector>
#include "rtrmgr_module.h"
#include "libxorp/xorp.h"
#include "libxorp/status_codes.h"
#include "libxipc/xrl_router.hh"
#include "unexpanded_xrl.hh"

class ConfigTree;
class ModuleCommand;
class ModuleManager;
class Task;
class TaskManager;
class XorpClient;

class Validation {
public:
    typedef XorpCallback1<void, bool>::RefPtr CallBack;

    Validation(const string& module_name, bool verbose)
	: _module_name(module_name), _verbose(verbose) {};
    virtual ~Validation() {};

    virtual void validate(CallBack cb) = 0;

protected:
    const string _module_name;
    bool	_verbose;	 // Set to true if output is verbose
};

class DelayValidation : public Validation {
public:
    DelayValidation(const string& module_name, EventLoop& eventloop,
		    uint32_t ms, bool verbose);

    void validate(CallBack cb);

private:
    void timer_expired();

    EventLoop&	_eventloop;
    CallBack	_cb;
    uint32_t	_delay_in_ms;
    XorpTimer	_timer;
};

class XrlStatusValidation : public Validation {
public:
    XrlStatusValidation(const string& module_name, const XrlAction& xrl_action,
			TaskManager& taskmgr);
    virtual ~XrlStatusValidation() {}

    void validate(CallBack cb);

protected:
    void dummy_response();
    virtual void xrl_done(const XrlError& e, XrlArgs* xrl_args);
    virtual void handle_status_response(ProcessStatus status,
					const string& reason) = 0;
    EventLoop& eventloop();

    const XrlAction&	_xrl_action;
    TaskManager&	_task_manager;
    CallBack		_cb;
    XorpTimer		_retry_timer;
    uint32_t		_retries;
};

class StatusStartupValidation : public XrlStatusValidation {
public:
    StatusStartupValidation(const string& module_name,
			    const XrlAction& xrl_action,
			    TaskManager& taskmgr);

private:
    void handle_status_response(ProcessStatus status, const string& reason);
};

class StatusReadyValidation : public XrlStatusValidation {
public:
    StatusReadyValidation(const string& module_name,
			  const XrlAction& xrl_action,
			  TaskManager& taskmgr);

private:
    void handle_status_response(ProcessStatus status, const string& reason);
};

class StatusConfigMeValidation : public XrlStatusValidation {
public:
    StatusConfigMeValidation(const string& module_name,
			     const XrlAction& xrl_action,
			     TaskManager& taskmgr);

private:
    void handle_status_response(ProcessStatus status, const string& reason);
};

class StatusShutdownValidation : public XrlStatusValidation {
public:
    StatusShutdownValidation(const string& module_name,
			     const XrlAction& xrl_action,
			     TaskManager& taskmgr);

private:
    void xrl_done(const XrlError& e, XrlArgs* xrl_args);
    void handle_status_response(ProcessStatus status, const string& reason);
};

class Startup {
public:
    typedef XorpCallback1<void, bool>::RefPtr CallBack;
    Startup(const string& module_name, bool verbose);
    virtual ~Startup() {}

    virtual void startup(CallBack cb) = 0;

protected:
    const string _module_name;
    bool	_verbose;	 // Set to true if output is verbose
};

class XrlStartup : public Startup {
public:
    XrlStartup(const string& module_name, const XrlAction& xrl_action,
	       TaskManager& taskmgr);
    virtual ~XrlStartup() {}

    void startup(CallBack cb);
    EventLoop& eventloop() const;

private:
    void dummy_response();
    void startup_done(const XrlError& err, XrlArgs* xrl_args);

    const XrlAction&	_xrl_action;
    TaskManager&	_task_manager;
    CallBack		_cb;
    XorpTimer		_dummy_timer;
};

class Shutdown {
public:
    typedef XorpCallback1<void, bool>::RefPtr CallBack;
    Shutdown(const string& module_name, bool verbose);
    virtual ~Shutdown() {}

    virtual void shutdown(CallBack cb) = 0;

protected:
    const string _module_name;
    bool	_verbose;	 // Set to true if output is verbose
};

class XrlShutdown : public Shutdown {
public:
    XrlShutdown(const string& module_name, const XrlAction& xrl_action,
		TaskManager& taskmgr);
    virtual ~XrlShutdown() {}

    void shutdown(CallBack cb);
    EventLoop& eventloop() const;

private:
    void dummy_response();
    void shutdown_done(const XrlError& err, XrlArgs* xrl_args);

    const XrlAction&	_xrl_action;
    TaskManager&	_task_manager;
    CallBack		_cb;
    XorpTimer		_dummy_timer;
};

class TaskXrlItem {
public:
    TaskXrlItem(const UnexpandedXrl& uxrl, const XrlRouter::XrlCallback& cb,
		Task& task);
    TaskXrlItem::TaskXrlItem(const TaskXrlItem& them);

    bool execute(string& errmsg);
    void execute_done(const XrlError& err, XrlArgs* xrl_args);
    void resend();
    void unschedule();

private:
    UnexpandedXrl	_unexpanded_xrl;
    XrlRouter::XrlCallback _xrl_callback;
    Task&		_task;
    XorpTimer		_resend_timer;
    uint32_t		_resend_counter;
    bool		_verbose;	 // Set to true if output is verbose
};

class Task {
public:
    typedef XorpCallback2<void, bool, string>::RefPtr CallBack;

    Task(const string& name, TaskManager& taskmgr);
    ~Task();

    void start_module(const string& mod_name, Validation* startup_validation,
		      Validation* config_validation, Startup* startup);
    void shutdown_module(const string& mod_name, Validation* validation,
			 Shutdown* shutdown);
    void add_xrl(const UnexpandedXrl& xrl, XrlRouter::XrlCallback& cb);
    void set_ready_validation(Validation* validation);
    Validation* ready_validation() const { return _ready_validation; }
    bool will_shutdown_module() const { return _stop_module; }
    void run(CallBack cb);
    void xrl_done(bool success, bool fatal, string errmsg); 
    bool do_exec() const;
    XorpClient& xorp_client() const;

    const string& name() const { return _name; }
    EventLoop& eventloop() const;

    bool verbose() const { return _verbose; }

protected:
    void step1_start();
    void step1_done(bool success);

    void step2_wait();
    void step2_done(bool success);

    void step2_2_wait();
    void step2_2_done(bool success);

    void step2_3_wait();
    void step2_3_done(bool success);

    void step3_config();
    void step3_done(bool success);

    void step4_wait();
    void step4_done(bool success);

    void step5_stop();
    void step5_done(bool success);

    void step6_wait();
    void step6_done(bool success);

    void step7_wait();
    void step7_kill();

    void step8_report();
    void task_fail(string errmsg, bool fatal);

private:
    string	_name;		// The name of the task
    TaskManager& _taskmgr;
    string	_module_name;	// The name of the module to start and stop
    bool	_start_module;
    bool	_stop_module;
    Validation*	_startup_validation; // The validation mechanism for the
                                     // module startup
    Validation* _config_validation;  // The validation mechanism for the
				     // module configuration
    Validation*	_ready_validation; // The validation mechanism for the module 
                                   // reconfiguration
    Validation*	_shutdown_validation;  // The validation mechanism for the 
                                       // module shutdown
    Startup*	_startup_method;
    Shutdown*	_shutdown_method;
    list<TaskXrlItem> _xrls;
    bool	_config_done;	// True if we changed the module's config
    CallBack	_task_complete_cb; // The task completion callback
    XorpTimer	_wait_timer;
    bool	_verbose;	 // Set to true if output is verbose
};

class TaskManager {
    typedef XorpCallback2<void, bool, string>::RefPtr CallBack;

public:
    TaskManager::TaskManager(ConfigTree& config_tree, ModuleManager& mmgr,
			     XorpClient& xclient, bool global_do_exec,
			     bool verbose);
    ~TaskManager();

    void set_do_exec(bool do_exec);
    void reset();
    int add_module(const ModuleCommand& mod_cmd);
    void add_xrl(const string& module_name, const UnexpandedXrl& xrl, 
		 XrlRouter::XrlCallback& cb);
    void shutdown_module(const string& module_name);
    void run(CallBack cb);
    XorpClient& xorp_client() const { return _xorp_client; }
    ModuleManager& module_manager() const { return _module_manager; }
    ConfigTree& config_tree() const { return _config_tree; }
    bool do_exec() const { return _current_do_exec; }
    bool verbose() const { return _verbose; }
    EventLoop& eventloop() const;

    /**
     * @short kill_process is used to kill a fatally wounded process
     *
     * kill_process is used to kill a fatally wounded process. This
     * does not politely ask the process to die, because if we get
     * here we can't communicate with the process using XRLs, so we
     * just kill it outright.
     * 
     * @param module_name the module name of the process to be killed.  
     */
    void kill_process(const string& module_name);

    /**
     * @short shell_execute is used to start external processes.
     *
     * shell_execute is used to start external processes, running them
     * in a shell.  It is NOT used to start regular XORP processes,
     * but rather for background maintenance tasks.
     *
     * @param userid the UID of the user to run the task as.
     * @param argv the command and arguements to run
     * @param callback callback to call when the child process terminates
     */
    int shell_execute(uid_t userid, const vector<string>& argv, 
		      TaskManager::CallBack cb);

private:
    void reorder_tasks();
    void run_task();
    void task_done(bool success, string errmsg);
    void fail_tasklist_initialization(const string& errmsg);
    Task& find_task(const string& module_name);
    void null_callback();

    ConfigTree&		_config_tree;
    ModuleManager&	_module_manager;
    XorpClient&		_xorp_client;
    bool		_global_do_exec; // Set to false if we're never going
					// to execute anything because we're
					// in a debug mode
    bool		_current_do_exec;
    bool		_verbose;	// Set to true if output is verbose

    // _tasks provides fast access to a Task by name
    map<string, Task*> _tasks;

    // _tasklist maintains the execution order
    list<Task*> _tasklist;

    // _shutdown_order maintains the shutdown ordering
    list<Task*> _shutdown_order;

    map<string, const ModuleCommand*> _module_commands;

    CallBack _completion_cb;
};

#endif // __RTRMGR_TASK_HH__
