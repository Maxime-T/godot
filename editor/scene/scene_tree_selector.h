/**************************************************************************/
/*  scene_tree_selector.h                                                 */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "editor/scene/scene_tree_editor.h"

class CheckButton;

class SceneTreeSelector : public VBoxContainer {
	GDCLASS(SceneTreeSelector, VBoxContainer);

	Tree *scene_tree = nullptr;
	CheckBox *select_all_checkbox = nullptr;
	CheckButton *show_all_checkbox = nullptr;
	Node *scene_root = nullptr;
	AHashMap<ObjectID, ObjectID> item_node_map;
	AHashMap<ObjectID, ObjectID> node_item_map;

	HashSet<ObjectID> selected_nodes;
	HashSet<ObjectID> selectable_nodes;
	HashSet<ObjectID> marked_nodes;
	bool show_all_nodes = false;

	void _add_item(Node *p_parent, Node *p_node, int p_index);

	void _on_item_edited();
	void _on_select_all_toggled(bool p_pressed);
	void _on_show_all_toggled(bool p_pressed);

	void _update_subtree(Node *p_node, AHashMap<Node *, bool> &memoized_filter_result);
	bool _matches_filter(Node *p_node, AHashMap<Node *, bool> &memoized_filter_result);
	void _reapply_selection(const HashSet<ObjectID> &p_selected_nodes);

protected:
	static void _bind_methods();

public:
	void clear();
	void create(Node *p_root, const HashSet<ObjectID> &p_selectable_nodes, const HashSet<ObjectID> &p_marked_nodes = HashSet<ObjectID>());
	HashSet<Node *> get_selected_nodes() const;
	SceneTreeSelector();
};
