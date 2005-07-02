// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
// vim:set sts=4 ts=8:

// Copyright (c) 2001-2005 International Computer Science Institute
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

#ident "$XORP: xorp/rtrmgr/conf_tree_node.cc,v 1.68 2005/07/02 00:08:33 pavlin Exp $"

//#define DEBUG_LOGGING
#include "rtrmgr_module.h"

#include "libxorp/xorp.h"
#include "libxorp/xlog.h"
#include "libxorp/debug.h"

#include "command_tree.hh"
#include "conf_tree_node.hh"
#include "module_command.hh"
#include "template_commands.hh"
#include "template_tree_node.hh"
#include "util.hh"


extern int booterror(const char *s) throw (ParseError);

bool 
CTN_Compare::operator() (ConfigTreeNode* a, ConfigTreeNode *b) {
    //if the nodes have the same template, then we sort according to
    //their own sort order, otherwise we sort them into template tree
    //order
    if (a->template_tree_node() == b->template_tree_node()) {
	TTSortOrder order = a->template_tree_node()->order();
	switch (order) {
	case ORDER_UNSORTED:
	    return false;
	case ORDER_SORTED_NUMERIC:
	    if (a->is_tag()) {
		XLOG_ASSERT(!a->const_children().empty());
		XLOG_ASSERT(!b->const_children().empty());
		const ConfigTreeNode *child_a = *(a->const_children().begin());
		const ConfigTreeNode *child_b = *(b->const_children().begin());
		return (atoi(child_a->segname().c_str()) 
			< atoi(child_b->segname().c_str()));
	    } else {
		return (atoi(a->segname().c_str()) 
			< atoi(b->segname().c_str()));
	    }
	case ORDER_SORTED_ALPHABETIC:
	    if (a->is_tag()) {
		XLOG_ASSERT(!a->const_children().empty());
		XLOG_ASSERT(!b->const_children().empty());
		const ConfigTreeNode *child_a = *(a->const_children().begin());
		const ConfigTreeNode *child_b = *(b->const_children().begin());
		return (child_a->str() < child_b->str());
	    } else {
		return (a->str() < b->str());
	    }
	}
    }

    //nodes have different templates, so sort by template file order
    return (a->child_number() < b->child_number());
}


bool 
CTN_CompareValue::operator() (ConfigTreeNode* a, ConfigTreeNode *b) {
    //if the nodes have the same template, then we sort according to
    //their own sort order, otherwise we leave them as they are
    if (a->template_tree_node() == b->template_tree_node()) {
	TTSortOrder order = a->template_tree_node()->order();
	switch (order) {
	case ORDER_UNSORTED:
	    //don't sort
	    return false;
	case ORDER_SORTED_NUMERIC:
	    if (a->is_tag()) {
		XLOG_ASSERT(!a->const_children().empty());
		XLOG_ASSERT(!b->const_children().empty());
		const ConfigTreeNode *child_a = *(a->const_children().begin());
		const ConfigTreeNode *child_b = *(b->const_children().begin());
		return (atoi(child_a->segname().c_str()) 
			< atoi(child_b->segname().c_str()));
	    } else {
		return (atoi(a->segname().c_str()) 
			< atoi(b->segname().c_str()));
	    }
	case ORDER_SORTED_ALPHABETIC:
	    if (a->is_tag()) {
		XLOG_ASSERT(!a->const_children().empty());
		XLOG_ASSERT(!b->const_children().empty());
		const ConfigTreeNode *child_a = *(a->const_children().begin());
		const ConfigTreeNode *child_b = *(b->const_children().begin());
		return (child_a->str() < child_b->str());
	    } else {
		return (a->str() < b->str());
	    }
	}
    }

    //nodes have different templates, so don't sort them
    return false;
}


ConfigTreeNode::ConfigTreeNode(bool verbose)
    : _template_tree_node(NULL),
      _deleted(false),
      _has_value(false),
      _operator(OP_NONE),
      _committed_operator(OP_NONE),
      _parent(NULL),
      _user_id(0),
      _committed_user_id(0),
      _committed_modification_time(TimeVal::ZERO()),
      _existence_committed(false),
      _value_committed(false),
      _on_parent_path(false),
      _verbose(verbose)
{
}

ConfigTreeNode::ConfigTreeNode(const string& nodename,
			       const string& path, 
			       const TemplateTreeNode* ttn,
			       ConfigTreeNode* parent,
			       uid_t user_id,
			       bool verbose)
    : _template_tree_node(ttn),
      _deleted(false),
      _has_value(false),
      _operator(OP_NONE),
      _committed_operator(OP_NONE),
      _segname(nodename),
      _path(path),
      _parent(parent),
      _user_id(user_id),
      _committed_user_id(0),
      _committed_modification_time(TimeVal::ZERO()),
      _existence_committed(false),
      _value_committed(false),
      _on_parent_path(false),
      _verbose(verbose)
{
    TimerList::system_gettimeofday(&_modification_time);
    parent->add_child(this);
}

ConfigTreeNode::ConfigTreeNode(const ConfigTreeNode& ctn)
    : _template_tree_node(ctn._template_tree_node),
      _deleted(ctn._deleted),
      _has_value(ctn._has_value),
      _value(ctn._value),
      _committed_value(ctn._committed_value),
      _operator(ctn._operator),
      _committed_operator(ctn._committed_operator),
      _segname(ctn._segname),
      _path(ctn._path),
      _parent(NULL),
      _user_id(ctn._user_id),
      _committed_user_id(ctn._committed_user_id),
      _modification_time(ctn._modification_time),
      _committed_modification_time(ctn._committed_modification_time),
      _existence_committed(ctn._existence_committed),
      _value_committed(ctn._value_committed),
      _on_parent_path(false),
      _verbose(ctn._verbose)
{
}

ConfigTreeNode::~ConfigTreeNode()
{
    while (!_children.empty()) {
	delete _children.front();
	_children.pop_front();
    }

    // Detect accidental reuse
    _template_tree_node = reinterpret_cast<TemplateTreeNode *>(0xbad);
    _parent = reinterpret_cast<ConfigTreeNode *>(0xbad);
}

bool
ConfigTreeNode::operator==(const ConfigTreeNode& them) const
{
    //
    // This comparison only cares about the declarative state of the node,
    // not about the ephemeral state such as when it was modified, deleted,
    // or who changed it last.
    //
    if (_segname != them.segname())
	return false;
    if (_parent != NULL && them.const_parent() != NULL) {
	if (is_tag() != them.is_tag())
	    return false;
	if (_has_value && (_value != them.value())) {
	    return false;
	}
	if (_operator != them.get_operator()) {
	    return false;
	}
    }
    if (is_leaf() != them.is_leaf())
	return false;
    if (_template_tree_node != them.template_tree_node())
	return false;

    debug_msg("Equal nodes:\n");
    debug_msg("Node1: %s\n", str().c_str());
    debug_msg("Node2: %s\n\n", them.str().c_str());

    return true;
}


void 
ConfigTreeNode::add_child(ConfigTreeNode* child)
{
    _children.push_back(child);
    list<ConfigTreeNode *> sorted_children = _children;
    sort_by_value(sorted_children);
    _children = sorted_children;
}

void 
ConfigTreeNode::remove_child(ConfigTreeNode* child)
{
    //
    // It's faster to implement this ourselves than use _children.remove()
    // because _children has no duplicates.
    //
    list<ConfigTreeNode*>::iterator iter;
    for (iter = _children.begin(); iter != _children.end(); ++iter) {
	if ((*iter) == child) {
	    _children.erase(iter);
	    return;
	}
    }

    XLOG_UNREACHABLE();
}

void
ConfigTreeNode::add_default_children()
{
    if (_template_tree_node == NULL)
	return;

    // Find all the template nodes for our children
    set<const TemplateTreeNode*> childrens_templates;
    list<ConfigTreeNode *>::const_iterator iter;
    for (iter = _children.begin(); iter != _children.end(); ++iter) {
	childrens_templates.insert((*iter)->template_tree_node());
    }

    //
    // Now go through all the children of our template node, and find
    // any that are leaf nodes that have default values, and that we
    // don't already have an instance of among our children.
    //
    list<TemplateTreeNode*>::const_iterator tci;
    for (tci = _template_tree_node->children().begin();
	 tci != _template_tree_node->children().end();
	 ++tci) {
	if ((*tci)->has_default()) {
	    if (childrens_templates.find(*tci) == childrens_templates.end()) {
		string name = (*tci)->segname();
		string path = _path + " " + name;
		ConfigTreeNode *new_node = create_node(name, path, *tci,
						       this, _user_id,
						       _verbose);
		new_node->set_value((*tci)->default_str(), _user_id);
		new_node->set_operator(OP_ASSIGN, _user_id);
	    }
	}
    }
}

void 
ConfigTreeNode::recursive_add_default_children()
{
    list<ConfigTreeNode *>::const_iterator iter;
    for (iter = _children.begin(); iter != _children.end(); ++iter) {
	(*iter)->recursive_add_default_children();
    }

    add_default_children();
}

void
ConfigTreeNode::set_value(const string &value, uid_t user_id)
{
    _value = value;
    _has_value = true;
    _value_committed = false;
    _user_id = user_id;
    TimerList::system_gettimeofday(&_modification_time);
}

void
ConfigTreeNode::set_operator(ConfigOperator op, uid_t user_id)
{
    if (_operator != op) {
	_operator = op;
	_value_committed = false;
	_user_id = user_id;
	TimerList::system_gettimeofday(&_modification_time);
    }
}

bool
ConfigTreeNode::merge_deltas(uid_t user_id,
			     const ConfigTreeNode& delta_node, 
			     bool provisional_change,
			     string& response)
{
    XLOG_ASSERT(_segname == delta_node.segname());

    if (_template_tree_node != NULL) {
	XLOG_ASSERT(type() == delta_node.type());

	if (delta_node.is_leaf()) {
	    if (_value != delta_node.value() 
		|| _operator != delta_node.get_operator()) {
		_has_value = true;
		if (provisional_change) {
		    _committed_value = _value;
		    _value = delta_node.value();
		    _committed_operator = _operator;
		    _operator = delta_node.get_operator();
		    _committed_user_id = _user_id;
		    _user_id = user_id;
		    _committed_modification_time = _modification_time;
		    TimerList::system_gettimeofday(&_modification_time);
		    _value_committed = false;
		} else {
		    _committed_value = delta_node.value();
		    _value = delta_node.value();
		    _committed_operator = delta_node.get_operator();
		    _operator = delta_node.get_operator();
		    _committed_user_id = delta_node.user_id();
		    _user_id = delta_node.user_id();
		    _committed_modification_time =
			delta_node.modification_time();
		    _modification_time = delta_node.modification_time();
		    _value_committed = true;
		}
#if 0
XXXXXXX to be copied to MasterConfigTreeNode
		_actions_pending = 0;
		_actions_succeeded = true;
		_cmd_that_failed = NULL;
#endif
	    }
	}
    }

    list<ConfigTreeNode*>::const_iterator iter;
    for (iter = delta_node.const_children().begin();
	 iter != delta_node.const_children().end();
	 ++iter) {
	ConfigTreeNode *delta_child = *iter;
	
	bool delta_child_done = false;
	list<ConfigTreeNode*>::iterator ci;
	for (ci = _children.begin(); ci != _children.end(); ++ci) {
	    ConfigTreeNode *my_child = *ci;
	    if (my_child->segname() == delta_child->segname()) {
		delta_child_done = true;
		bool success = my_child->merge_deltas(user_id, *delta_child,
						      provisional_change,
						      response);
		if (success == false) {
		    // If something failed, abort the merge
		    return false;
		}
		break;
	    }
	}
	if (delta_child_done == false) {
	    ConfigTreeNode* new_node;
	    new_node = create_node(delta_child->segname(),
				   delta_child->path(),
				   delta_child->template_tree_node(),
				   this,
				   user_id,
				   _verbose);
	    if (!provisional_change)
		new_node->set_existence_committed(true);
	    new_node->merge_deltas(user_id, *delta_child, provisional_change,
				   response);
	}
    }
    return true;
}

bool
ConfigTreeNode::merge_deletions(uid_t user_id,
				const ConfigTreeNode& deletion_node, 
				bool provisional_change,
				string& response)
{
    XLOG_ASSERT(_segname == deletion_node.segname());

    if (_template_tree_node != NULL) {
	XLOG_ASSERT(type() == deletion_node.type());
	if (deletion_node.const_children().empty()) {
	    if (provisional_change) {
		_deleted = true;
		_value_committed = false;
	    } else {
		delete_subtree_silently();
	    }
	    return true;
	}
    }
    
    list<ConfigTreeNode*>::const_iterator iter;
    for (iter = deletion_node.const_children().begin();
	 iter != deletion_node.const_children().end();
	 ++iter) {
	ConfigTreeNode *deletion_child = *iter;

	bool deletion_child_done = false;
	list<ConfigTreeNode*>::iterator ci;
	for (ci = _children.begin(); ci != _children.end(); ++ci) {
	    ConfigTreeNode *my_child = *ci;
	    if (my_child->segname() == deletion_child->segname()) {
		deletion_child_done = true;
		bool success = my_child->merge_deletions(user_id, 
							 *deletion_child,
							 provisional_change,
							 response);
		if (success == false) {
		    // If something failed, abort the merge
		    return false;
		}
		break;
	    }
	}
	if (deletion_child_done == false) {
	    response = "Failed to delete node:\n   " + deletion_node.path()
		+ "\nNode does not exist.\n";
	    return false;
	}
    }
    return true;
}


ConfigTreeNode*
ConfigTreeNode::find_config_module(const string& module_name)
{
    if (module_name.empty())
	return (NULL);

    // Check if this node matches
    if (_template_tree_node != NULL) {
	if (_template_tree_node->module_name() == module_name)
	    return (this);
    }

    // Check all children nodes
    list<ConfigTreeNode*>::const_iterator iter;
    for (iter = _children.begin(); iter != _children.end(); ++iter) {
	ConfigTreeNode* ctn = (*iter)->find_config_module(module_name);
	if (ctn != NULL)
	    return (ctn);
    }

    return (NULL);
}

bool
ConfigTreeNode::check_config_tree(string& result) const
{
    list<ConfigTreeNode *>::const_iterator iter;

    //
    // Check that the node is not deprecated
    //
    if (_template_tree_node != NULL) {
	if (_template_tree_node->is_deprecated()) {
	    result = c_format("Node \"%s\" is deprecated: %s\n",
			      _path.c_str(),
			      _template_tree_node->deprecated_reason().c_str());
	    return false;
	}
    }
    //
    // An early check that the child nodes are not deprecated.
    // Note: we need to check the child nodes before the check for
    // mandatory children so we will print a better error message
    // if, say, a mandatory child node has been renamed and the old
    // one has been deprecated.
    //
    for (iter = _children.begin(); iter != _children.end(); ++iter) {
	const TemplateTreeNode* ttn = (*iter)->template_tree_node();
	if (ttn == NULL)
	    continue;
	if (ttn->is_deprecated()) {
	    result = c_format("Node \"%s\" is deprecated: %s\n",
			      (*iter)->path().c_str(),
			      ttn->deprecated_reason().c_str());
	    return false;
	}
    }

    //
    // Check that all mandatory child nodes are configured in place
    //
    if (_template_tree_node != NULL) {
	list<string>::const_iterator li;
	for (li = _template_tree_node->mandatory_children().begin();
	     li != _template_tree_node->mandatory_children().end();
	     ++li) {
	    const string& mandatory_child = *li;
	    bool found = false;
	    for (iter = _children.begin(); iter != _children.end(); ++iter) {
		if ((*iter)->segname() == mandatory_child) {
		    found = true;
		    break;
		}
	    }
	    if (! found) {
		result = c_format("Node \"%s\" has no configured mandatory child node \"%s\"\n",
				  _path.c_str(), mandatory_child.c_str());
		return false;
	    }
	}
    }

    //
    // Recursively check all child nodes
    //
    for (iter = _children.begin(); iter != _children.end(); ++iter) {
	ConfigTreeNode* ctn = *iter;
	if (ctn->check_config_tree(result) != true)
	    return false;
    }

    return true;
}


string
ConfigTreeNode::discard_changes(int depth, int last_depth)
{
    string result;
    bool changes_made = false;

    debug_msg("DISCARD CHANGES node >%s<\n", _path.c_str());

    // The root node has a NULL template
    if (_template_tree_node != NULL) {
	if (_existence_committed == false) {
	    result =  show_subtree(depth, /* XXX */ depth * 2, true, false);
	    delete_subtree_silently();
	    return result;
	} else if (_value_committed == false) {
	    debug_msg("discarding changes from node %s\n",
		      _path.c_str());
	    _value_committed = true;
	    _user_id = _committed_user_id;
	    _value = _committed_value;
	    _operator = _committed_operator;
	    _modification_time = _committed_modification_time;
	    _deleted = false;
	    result = node_str();
	    if (is_leaf()) 
		result += "\n";
	    else
		result += " {\n";
	    changes_made = true;
	} 
    }

    list<ConfigTreeNode *>::iterator iter, prev_iter;
    iter = _children.begin();
    if (changes_made)
	last_depth = depth;
    while (iter != _children.end()) {
	// Be careful, or discard_changes can invalidate the iterator
	prev_iter = iter;
	++iter;
	debug_msg("  child: %s\n", (*prev_iter)->path().c_str());
	result += (*prev_iter)->discard_changes(depth + 1, last_depth);
    }

    return result;
}

int 
ConfigTreeNode::type() const
{
    return _template_tree_node->type();
}

bool 
ConfigTreeNode::is_tag() const
{
    /*only the root node does not have a template tree node, and it is
      not a tag*/
    if (_template_tree_node == NULL)
	return false;

    return _template_tree_node->is_tag();
}

bool 
ConfigTreeNode::is_leaf() const
{
    if (_has_value)
	return true;
    if (_template_tree_node == NULL)
	return false;
    if ((_template_tree_node->type() != NODE_VOID)
	&& (_parent != NULL)
	&& (!_parent->is_tag()))
	return true;
    return false;
}

unsigned int 
ConfigTreeNode::depth() const
{
    if (is_root_node())
	return 0;
    else
	return (1 + _parent->depth());
}

const string& 
ConfigTreeNode::value() const
{
    if (_has_value)
	return _value;

    if (is_tag()) {
	// We should never ask a tag for its value
	XLOG_UNREACHABLE();
    }

    return _segname;
}

ConfigOperator
ConfigTreeNode::get_operator() const
{
    if (is_tag()) {
	// We should never ask a tag for its operator
	XLOG_UNREACHABLE();
    }

    return _operator;
}

string 
ConfigTreeNode::show_subtree(int depth, int indent, bool do_indent,
			     bool annotate) const
{

    string s;
    string my_in;
    bool show_me = (depth > 0);
    bool is_a_tag = false;
    int new_indent;

    if (_deleted)
	return string("");

    if (_template_tree_node != NULL)
	is_a_tag = is_tag();

    for (int i = 0; i < indent; i++) 
	my_in += "  ";

    if (is_a_tag && show_me) {
	new_indent = indent;
	list<ConfigTreeNode*>::const_iterator iter;
	for (iter = _children.begin(); iter != _children.end(); ++iter) {
	    if ((*iter)->deleted()) {
		// skip deleted children
	    } else {
		if (annotate) {
		    if ((*iter)->existence_committed())
			s += "    ";
		    else
			s += ">   ";
		}
		s += my_in + _segname + " " +
		    (*iter)->show_subtree(depth, indent, false, annotate);
	    }
	}
    } else if (is_a_tag && (show_me == false)) {
	s = "ERROR";
    } else if ((is_a_tag == false) && show_me) {
	string s2;

	new_indent = indent + 2;

	// annotate modified config lines
	if (annotate) {
	    if ((_has_value && !_value_committed) || (!_existence_committed))
		s2 = ">   ";
	    else
		s2 = "    ";
	}

	if (do_indent) {
	    s = s2 + my_in + _segname;
	} else
	    s += _segname; 
	if ((type() != NODE_VOID) && (_has_value)) {
	    string value;
	    if ((type() == NODE_TEXT) /* && annotate */ ) {
	    	// XXX: re-escape quotes.
	    	string escaped("");

		for (string::const_iterator i = _value.begin();
		    i != _value.end(); ++i) {
		    
		    const char x = *i;
		    if (x == '"')
			escaped += "\\\"";
		    else
			escaped += x;
		}
		
		value = "\"" + escaped + "\"";
	    } else {
		value = _value;
	    }
	    if (_parent != NULL && _parent->is_tag()) {
		s += " " + value;
	    } else {
		if (_operator == OP_NONE) {
		    //no-op;
		} else if (_operator == OP_ASSIGN) {
		    s += show_operator() + " " + value;
		} else {
		    s += " " + show_operator() + " " + value;
		}
	    }
	}
	if (_children.size() == 0 
	    && (type() != NODE_VOID || _has_value)) {
	    //
	    // Normally if a node has no children, we don't want the
	    // braces, but certain grouping nodes, with type NODE_VOID
	    // (e.g. "protocols") won't parse correctly if we print
	    // them without the braces.
	    //
	    s += "\n";
	    return s;
	}
	s += " {\n";
	list<ConfigTreeNode*>::const_iterator iter;
	for (iter = _children.begin(); iter != _children.end(); ++iter) {
	    s += (*iter)->show_subtree(depth+1, new_indent, true, annotate);
	}
	s += s2 + my_in + "}\n";
    } else if ((is_a_tag == false) && (show_me == false)) {
	new_indent = indent;
	list<ConfigTreeNode*>::const_iterator iter;
	for (iter = _children.begin(); iter != _children.end(); ++iter) {
	    s += (*iter)->show_subtree(depth+1, new_indent, true, annotate);
	}
    }
    return s;
}

void
ConfigTreeNode::mark_subtree_as_committed()
{
    _existence_committed = true;
    _value_committed = true;

    list<ConfigTreeNode*>::iterator iter;
    iter = _children.begin();
    while (iter != _children.end()) {
	(*iter)->mark_subtree_as_committed();
	++iter;
    }
}

void
ConfigTreeNode::mark_subtree_as_uncommitted()
{
    _existence_committed = false;
    _value_committed = false;

    list<ConfigTreeNode*>::iterator iter;
    iter = _children.begin();
    while (iter != _children.end()) {
	(*iter)->mark_subtree_as_uncommitted();
	++iter;
    }
}

void
ConfigTreeNode::mark_subtree_for_deletion(uid_t user_id)
{
    // We delete all the children of this node, then delete the node itself
    debug_msg("Mark subtree for deletion: %s\n", _segname.c_str());

    if (_existence_committed == false) {
	// this node was never committed
	delete_subtree_silently();
	return;
    }

    //
    // Delete_subtree calls remove_child, so we just iterate until no
    // children are left.
    //
    debug_msg("Node has %u children\n", XORP_UINT_CAST(_children.size()));
    list<ConfigTreeNode*>::iterator iter, prev_iter;
    iter = _children.begin();
    while (iter != _children.end()) {
	//
	// Be careful to avoid the iterator becoming invalid when
	// calling mark_subtree_for_deletion on an uncommitted node.
	//
	prev_iter = iter;
	++iter;
	(*prev_iter)->mark_subtree_for_deletion(user_id);
    }

    _user_id = user_id;
    TimerList::system_gettimeofday(&_modification_time);
    _deleted = true;
    _value_committed = false;
#if 0
XXXXX to be copied to MasterConfigTreeNode??
    _actions_succeeded = true;
#endif
}

void
ConfigTreeNode::delete_subtree_silently()
{
    //
    // Delete_subtree calls remove_child, so we just iterate until no
    // children are left.
    //
    while (!_children.empty()) {
	_children.front()->delete_subtree_silently();
    }

    if (_parent != NULL)
	_parent->remove_child(this);

    if (! is_root_node())
	delete this;
}

void
ConfigTreeNode::clone_subtree(const ConfigTreeNode& orig_node)
{
    list<ConfigTreeNode*>::const_iterator iter;
    for (iter = orig_node.const_children().begin();
	 iter != orig_node.const_children().end();
	 ++iter) {
	ConfigTreeNode* new_node = create_node(**iter);
	new_node->set_parent(this);
	add_child(new_node);
	new_node->clone_subtree(**iter);
    }
}

string 
ConfigTreeNode::typestr() const
{
    return _template_tree_node->typestr();
}

string 
ConfigTreeNode::node_str() const
{
    string tmp;

    if (_template_tree_node == NULL) {
	tmp = "ROOT";
    } else {
	tmp = _path + " " + typestr();
	if (!_value.empty()) {
	    tmp += " = " + _value;
	}
    }
    return tmp;
}

//
// TODO: this method is not used. What is the difference between
// str() and node_str() ??
// 
string
ConfigTreeNode::str() const
{
    string s = _path;

    if (_has_value) {
	s+= ": " + _value;
    }
    return s;
}

string
ConfigTreeNode::subtree_str() const
{
    string s;

    s = c_format("%s\n", node_str().c_str());

    list<ConfigTreeNode*>::const_iterator iter;
    for (iter = _children.begin(); iter != _children.end(); ++iter) {
	s += (*iter)->subtree_str();
    }

    return s;
}

ConfigTreeNode* 
ConfigTreeNode::find_node(list<string>& path)
{

    // Are we looking for the root node?
    if (path.empty())
	return this;

    if (_template_tree_node != NULL) {
	if (path.front() == _segname) {
	    if (path.size() == 1) {
		return this;
	    } else {
		path.pop_front();
	    }
	} else {
	    // We must have screwed up
	    XLOG_UNREACHABLE();
	}
    }

    list<ConfigTreeNode *>::const_iterator iter;
    for (iter = _children.begin(); iter != _children.end(); ++iter) {
	if ((*iter)->segname() == path.front()) {
	    return ((*iter)->find_node(path));
	}
    }

    // None of the nodes match the path
    return NULL;
}


bool
ConfigTreeNode::retain_different_nodes(const ConfigTreeNode& them,
				       bool retain_value_changed)
{
    list<ConfigTreeNode*>::iterator my_iter;
    list<ConfigTreeNode*>::const_iterator their_iter;
    bool retained_children = false;

    XLOG_ASSERT(_segname == them.segname());

    for (my_iter = _children.begin(); my_iter != _children.end(); ) {
	bool retain_child = true;
	ConfigTreeNode *my_child = *my_iter;

	// Be careful not to invalidate the iterator when we remove children
	++my_iter;

	for (their_iter = them.const_children().begin();
	     their_iter != them.const_children().end();
	     ++their_iter) {
	    ConfigTreeNode* their_child = *their_iter;
	    // Are the nodes the same?
	    if ((*my_child) == (*their_child)) {
		if (!my_child->retain_different_nodes(*their_child,
						      retain_value_changed)) {
		    retain_child = false;
		}
		break;
	    }
	    // Are the nodes the same leaf node, but with a changed value
	    if (!retain_value_changed 
		&& my_child->is_leaf() && their_child->is_leaf() 
		&& (my_child->segname() == their_child->segname())) {
		retain_child = false;
		break;
	    }
	}
	if (retain_child == false) {
	    remove_child(my_child);
	    delete my_child;
	} else {
	    retained_children = true;
	}
    }
    return retained_children;
}

void
ConfigTreeNode::retain_common_nodes(const ConfigTreeNode& them)
{
    list<ConfigTreeNode*>::iterator my_iter;
    list<ConfigTreeNode*>::const_iterator their_iter;
    bool retained_children = false;

    XLOG_ASSERT(_segname == them.segname());

    for (my_iter = _children.begin(); my_iter != _children.end(); ) {
	bool retain_child = false;
	ConfigTreeNode *my_child = *my_iter;

	// Be careful not to invalidate the iterator when we remove children
	++my_iter;

	for (their_iter = them.const_children().begin();
	     their_iter != them.const_children().end();
	     ++their_iter) {
	    ConfigTreeNode* their_child = *their_iter;
	    if ((*my_child) == (*their_child)) {
		my_child->retain_common_nodes(*their_child);
		retain_child = true;
		retained_children = true;
		break;
	    }
	}
	if (retain_child == false) {
	    remove_child(my_child);
	    delete my_child;
	}
    }
    if (_parent != NULL && retained_children == false && is_tag()) {
	_parent->remove_child(this);
	delete this;
	return;
    }
}


bool
ConfigTreeNode::expand_variable(const string& varname, string& value) const
{

    VarType type = NONE;
    const ConfigTreeNode *varname_node;

    debug_msg("ConfigTreeNode::expand_variable at %s: >%s<\n",
	      _segname.c_str(), varname.c_str());

    varname_node = find_const_varname_node(varname, type);

    switch (type) {
    case NONE:
	return false;
    case NODE_VALUE:
	XLOG_ASSERT(varname_node != NULL);
	value = varname_node->value();
	return true;
    case NODE_OPERATOR:
	XLOG_ASSERT(varname_node != NULL);
	value = varname_node->show_operator();
	debug_msg("variable %s at %s, value is \"%s\"\n",  varname.c_str(), _segname.c_str(), value.c_str());
	return true;
    case NAMED:
    {
	list<string> var_parts;

	XLOG_ASSERT(varname_node != NULL);
	if (split_up_varname(varname, var_parts) == false) {
	    return false;
	}
	value = varname_node->named_value(var_parts.back());
	return true;
    }
    case TEMPLATE_DEFAULT:
    {
	const TemplateTreeNode* ttn;
	ttn = _template_tree_node->find_varname_node(varname);
	XLOG_ASSERT(ttn != NULL);
	XLOG_ASSERT(ttn->has_default());
	value = ttn->default_str();
	return true;
    }
    }
    XLOG_UNREACHABLE();
}

bool
ConfigTreeNode::expand_expression(const string& expression,
				  string& value) const
{
    // Expect string of form: "`" opchar + name + "`"
    // and the only definition of opchar supported is "~".
    //
    if (expression.size() < 4) {
	return false;
    }
    if (expression[0] != '`' ||
	expression[expression.size() - 1] != '`') {
	return false;
    }

    // Trim the back-quotes
    string tmp_expr = expression.substr(1, expression.size() - 2);

    //
    // XXX: for now the only expression we can expand is the "~" boolean
    // negation operator.
    //
    if (tmp_expr[0] != '~')
	return false;

    // Trim the operator
    tmp_expr = tmp_expr.substr(1, expression.size() - 1);

    // Expand the variable
    string tmp_value;
    if (expand_variable(tmp_expr, tmp_value) != true)
	return false;

    if (tmp_value == "false")
	value = "true";
    else if (tmp_value == "true")
	value = "false";
    else
	return false;

    return true;
}

const ConfigTreeNode* 
ConfigTreeNode::find_const_varname_node(const string& varname,
					VarType& type) const
{
    //
    // We need both const and non-const versions of find_varname_node,
    // but don't want to write it all twice.
    //
    return (const_cast<ConfigTreeNode*>(this))
	->find_varname_node(varname, type);
}

ConfigTreeNode*
ConfigTreeNode::find_varname_node(const string& varname, VarType& type)
{
    debug_msg("ConfigTreeNode::find_varname_node at %s: >%s<\n",
	      _segname.c_str(), varname.c_str());

    if (varname == "$(@)" || (varname == "$(" + _segname + ")") ) {
	XLOG_ASSERT(!_template_tree_node->is_tag());
	type = NODE_VALUE;
	debug_msg("varname node is >%s<\n", _segname.c_str());
	return this;
    }

    if (varname == "$(<>)") {
	XLOG_ASSERT(!_template_tree_node->is_tag());
	type = NODE_OPERATOR;
	debug_msg("varname node is >%s<\n", _segname.c_str());
	return this;
    }

    list<string> var_parts;
    if (split_up_varname(varname, var_parts) == false) {
	return NULL;
    }

    if (var_parts.back() == "DEFAULT") {
	// XXX: use the template tree to get the default value
	if (_template_tree_node->has_default()) {
	    type = TEMPLATE_DEFAULT;
	    return NULL;
	}
	type = NONE;
	return NULL;
    }

    ConfigTreeNode* found_node = NULL;
    if ((var_parts.front() == "@") || (_parent == NULL)) {
	_on_parent_path = true;
	found_node = find_child_varname_node(var_parts, type);
	_on_parent_path = false;
    } else if (var_parts.size() > 1) {
	// It's a parent node, or a child of a parent node
	found_node = find_parent_varname_node(var_parts, type);
    }
    if (found_node != NULL)
	return (found_node);

    // Test if we can match a template node
    if (_template_tree_node != NULL) {
	const TemplateTreeNode* ttn;
	ttn = _template_tree_node->find_varname_node(varname);
	if ((ttn != NULL) && ttn->has_default()) {
	    type = TEMPLATE_DEFAULT;
	    return NULL;
	}
    }

    //
    // XXX: if not referring to this node, then size of 0 or 1
    // is not valid syntax.
    //
    type = NONE;
    return NULL;
}

ConfigTreeNode*
ConfigTreeNode::find_parent_varname_node(const list<string>& var_parts, 
					 VarType& type)
{
    debug_msg("find parent at node %s\n", _segname.c_str());

    if (_parent == NULL) {
	debug_msg("reached root node\n");
	type = NONE;
	return NULL;
    }

    if (is_tag() || (this->type() == NODE_VOID)) {
	// When naming a parent node variable, you must start with a tag
	if (_segname == var_parts.front()) {
	    // We've found the right place to start
	    return find_child_varname_node(var_parts, type);
	}
    }
    _on_parent_path = true;
    ConfigTreeNode *parent, *found;
    parent = (ConfigTreeNode*)_parent;
    found = parent->find_parent_varname_node(var_parts, type);
    _on_parent_path = false;
    return found;
}

ConfigTreeNode*
ConfigTreeNode::find_child_varname_node(const list<string>& var_parts, 
					VarType& type)
{
    string s;

    s = "find child at node " + _segname + " varname: ";

    list<string>::const_iterator iter;
    for (iter = var_parts.begin(); iter != var_parts.end(); ++iter) {
	s += ">" + (*iter) + "< ";
    }
    s += "\n";
    debug_msg("%s", s.c_str());

    //
    // @ can only expand when we've already gone through this node
    // heading towards the parent.
    //
    if ((var_parts.front() == "@") && _on_parent_path == false) {
	type = NONE;
	debug_msg("no on parent path\n");
	return NULL;
    }

    if (_parent == NULL) {
	// We have reached the root node
	// The name should refer to a child of ours
	ConfigTreeNode *found_child, *child;
	list<ConfigTreeNode *>::iterator ci;
	for (ci = _children.begin(); ci != _children.end(); ++ci) {
	    child = (ConfigTreeNode*)(*ci);
	    found_child = child->find_child_varname_node(var_parts, type);
	    if (found_child != NULL)
		return found_child;
	}
	type = NONE;
	return NULL;
    }

    if ((var_parts.front() != "@")
	&& (var_parts.front() != _segname) 
	&& (var_parts.front() != "<>") 
	&& ((!_has_value) || (var_parts.front() != _value))) {
	// varname doesn't match us.
	type = NONE;
	debug_msg("varname doesn't match\n");
	return NULL;
    }

    // The name might refer to this node
    if (var_parts.size() == 1) {
	if ((var_parts.front() == "@") || (var_parts.front() == _segname)) {
	    type = NODE_VALUE;
	    debug_msg("varname V node is >%s<\n", _segname.c_str());
	    return this;
	}
    }

    // The name might refer to this node for an operator
    if (var_parts.size() == 1) {
	if ((var_parts.front() == "<>")) {
	    type = NODE_OPERATOR;
	    debug_msg("varname V node is >%s<\n", _segname.c_str());
	    return this;
	}
    }

    if (var_parts.size() == 2) {
	// The name might refer to a named variable on this node
	if (_variables.find(var_parts.back()) != _variables.end()) {
	    type = NAMED;
	    debug_msg("varname N node is >%s<\n", _segname.c_str());
	    return this;
	}
    }

    // The name might refer to a child of ours
    list<string> child_var_parts = var_parts;
    child_var_parts.pop_front();
    ConfigTreeNode *found_child, *child;
    list<ConfigTreeNode *>::iterator ci;
    for (ci = _children.begin(); ci != _children.end(); ++ci) {
	child = (ConfigTreeNode*)(*ci);
	found_child = child->find_child_varname_node(child_var_parts, type);
	if (found_child != NULL)
	    return found_child;
    }

    type = NONE;
    return NULL;
}

bool
ConfigTreeNode::split_up_varname(const string& varname, 
				       list<string>& var_parts) const
{
    debug_msg("split up varname >%s<\n", varname.c_str());

    if (varname.size() < 4 || varname[0] != '$'
	|| varname[1] != '(' || varname[varname.size() - 1] != ')') {
	XLOG_ERROR("Bad variable name: %s", varname.c_str());
	return false;
    }

    string trimmed = varname.substr(2, varname.size() - 3);
    var_parts = split(trimmed, '.');

    if (_verbose) {
	string debug_output;
	list<string>::iterator iter;
	for (iter = var_parts.begin(); iter != var_parts.end(); ++iter) {
	    debug_output += c_format(">%s<, ", iter->c_str());
	}
	debug_output += "\n";
	debug_msg("%s", debug_output.c_str());
    }
    return true;
}

string
ConfigTreeNode::join_up_varname(const list<string>& var_parts) const
{
    string s;

    s = "$(";
    list<string>::const_iterator iter;
    for (iter = var_parts.begin(); iter != var_parts.end(); ++iter) {
	if (iter != var_parts.begin()) {
	    s += ".";
	}
	s += *iter;
    }
    s += ")";
    return s;
}

void
ConfigTreeNode::expand_varname_to_matchlist(const vector<string>& parts,
						  size_t part,
						  list<string>& matches) const
{
    bool ok = false;

    // Don't expand unless the node has been committed
    if (! _existence_committed)
	return;

    if (is_root_node()) {
	// The root node
	ok = true;
	//
	// XXX: note that even if "part" is 0, this is compensated later by
	// the usage of "part + 1" later.
	//
	part--;		// Prevent increment of part later
    } else if ((!_parent->is_root_node()) && _parent->is_tag()) {
	// We're the varname after a tag
	if (parts[part] == "*") {
	    ok = true;
	} else {
	}
    } else {
	if (parts[part] != _segname) {
	    return;
	}
	ok = true;
    }

    if (! ok)
	return;

    if (part == parts.size() - 1) {
	// Everything that we were looking for matched
	XLOG_ASSERT(parts[part] == "*");
	matches.push_back(_segname);
	return;
    } 

    //
    // Search the children. If no more children, return the result so far
    //
    list<ConfigTreeNode*>::const_iterator iter;
    for (iter = _children.begin(); iter != _children.end(); ++iter) {
	ConfigTreeNode *child = (ConfigTreeNode*)(*iter);
	child->expand_varname_to_matchlist(parts, part + 1, matches);
    }
}

bool
ConfigTreeNode::set_variable(const string& varname, const string& value)
{
    //
    // The variable name should be in the form "nodename.varname".
    // Nodename itself can be complex, such as "vif.@" to indicate the
    // current vif.  The general idea is that we're setting a variable
    // on a config tree node. However the config tree node that we're
    // setting the variable on doesn't have to be "this" node - this
    // node only scopes the variable name.
    //

    // First search for an existing variable to set
    VarType type;
    ConfigTreeNode* node = find_varname_node(varname, type);
    string errmsg;
    if (node != NULL) {
	switch (type) {
	case NONE:
	    // This can't happen
	    XLOG_UNREACHABLE();
	    break;
	case NODE_VALUE: 
	    errmsg = c_format("Attempt to set variable \"%s\" "
			      "which is the name of a configuration node",
			      varname.c_str());
	    XLOG_ERROR("%s", errmsg.c_str());
	    return false;
	case NODE_OPERATOR: 
	    errmsg = c_format("Attempt to set variable operator \"%s\" "
			      "which is the name of a configuration node",
			      varname.c_str());
	    XLOG_ERROR("%s", errmsg.c_str());
	    return false;
	case NAMED:
	{
	    list<string> var_parts;

	    if (split_up_varname(varname, var_parts) == false) {
		return false;
	    }
	    node->set_named_value(var_parts.back(), value);
	    return true;
	}
	case TEMPLATE_DEFAULT: 
	    // XXX: Ignore, and attempt to set a named variable below.
	    break;
	}
    }

    //
    // We found no existing variable, see if there's a node on which we
    // can set a named variable.
    //
    list<string> var_parts;
    if (split_up_varname(varname, var_parts) == false) {
	return false;
    }
    list<string>::iterator iter;
    for (iter = var_parts.begin(); iter != var_parts.end(); ++iter) {
	debug_msg("VP: %s\n", iter->c_str());
    }
    if (var_parts.size() < 2) {
	errmsg = c_format("Attempt to set incomplete variable \"%s\"",
			  varname.c_str());
	XLOG_ERROR("%s", errmsg.c_str());
	return false;
    }
    var_parts.pop_back();
    if ((var_parts.front() == "@") || (_parent == NULL)) {
        _on_parent_path = true;
        node = find_child_varname_node(var_parts, type);
        _on_parent_path = false;
    } else {
        // It's a parent node, or a child of a parent node
        node = find_parent_varname_node(var_parts, type);
    }
    if (node != NULL) {
	switch (type) {
	case NONE:
	    // This can't happen
	    XLOG_UNREACHABLE();
	    break;
	case NODE_VALUE:
	    if (split_up_varname(varname, var_parts) == false) {
		return false;
	    }
	    node->set_named_value(var_parts.back(), value);
	    return true;
	case NODE_OPERATOR:
	    errmsg = c_format("Attempt to set operator \"%s\" "
			      "which is the child of a named variable",
			      varname.c_str());
	    XLOG_ERROR("%s", errmsg.c_str());
	    return false;
	case NAMED:
	    errmsg = c_format("Attempt to set variable \"%s\" "
			      "which is the child of a named variable",
			      varname.c_str());
	    XLOG_ERROR("%s", errmsg.c_str());
	    return false;
	case TEMPLATE_DEFAULT:
	    errmsg = c_format("Attempt to set variable \"%s\" "
			      "which is on a non-existent node",
			      varname.c_str());
	    XLOG_ERROR("%s", errmsg.c_str());
	    return false;
	}
    }

    errmsg = c_format("Attempt to set unknown variable \"%s\"",
		      varname.c_str());
    XLOG_ERROR("%s", errmsg.c_str());
    return false;
}

void 
ConfigTreeNode::set_named_value(const string& varname, const string& value)
{
    debug_msg("set_named_value %s=%s\n", varname.c_str(), value.c_str());

    _variables[varname] = value;
}


const string&
ConfigTreeNode::named_value(const string& varname) const
{
    map<string, string>::const_iterator iter;

    iter = _variables.find(varname);
    XLOG_ASSERT(iter != _variables.end());
    return iter->second;
}

int
ConfigTreeNode::child_number() const
{
    if (_template_tree_node) {
	return _template_tree_node->child_number();
    } else {
	return 0;
    }
}

void
ConfigTreeNode::sort_by_value(list <ConfigTreeNode*>& children) const
{
    children.sort(CTN_CompareValue());
} 


string
ConfigTreeNode::show_operator() const
{
    switch (_operator) {
    case OP_NONE:
	XLOG_UNREACHABLE();
    case OP_EQ:
	return string("==");
    case OP_LT:
	return string("<");
    case OP_LTE:
	return string("<=");
    case OP_GT:
	return string(">");
    case OP_GTE:
	return string(">=");
    case OP_ASSIGN:
	return string(":");
    case OP_ADD:
	return string("add");
    case OP_SUB:
	return string("sub");
    }
    XLOG_UNREACHABLE();
}
