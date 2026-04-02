/**************************************************************************/
/*  scene_tree_selector.cpp                                               */
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

#include "scene_tree_selector.h"

#include "core/object/callable_mp.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "scene/gui/box_container.h"
#include "scene/gui/check_box.h"
#include "scene/gui/check_button.h"

void SceneTreeSelector::_add_item(Node *p_parent, Node *p_node, int p_index) {
	TreeItem *parent_item = nullptr;
	if (p_parent && node_item_map.has(p_parent->get_instance_id())) {
		Object *obj = ObjectDB::get_instance(node_item_map.get(p_parent->get_instance_id()));
		ERR_FAIL_NULL_MSG(obj, "TreeItem not found for p_parent node: " + p_parent->get_name());
		parent_item = Object::cast_to<TreeItem>(obj);
	}
	TreeItem *item = scene_tree->create_item(parent_item, p_index);
	node_item_map.insert(p_node->get_instance_id(), item->get_instance_id());
	item_node_map.insert(item->get_instance_id(), p_node->get_instance_id());

	if (selectable_nodes.has(p_node->get_instance_id())) {
		item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
		item->set_selectable(0, true);
		item->set_editable(0, true);
	} else {
		item->set_cell_mode(0, TreeItem::CELL_MODE_STRING);
		item->set_selectable(0, false);
		item->set_editable(0, false);
	}
	item->set_text(0, p_node->get_name());
	item->set_icon(0, EditorNode::get_singleton()->get_object_icon(p_node));

	Ref<Script> scr = p_node->get_script();
	if (scr.is_valid()) {
		item->add_button(0, get_editor_theme_icon(SNAME("Script")));
		item->set_button_disabled(0, item->get_button_count(0) - 1, true);
	}

	if (marked_nodes.has(p_node->get_instance_id())) {
		item->set_custom_color(0, get_theme_color(SNAME("accent_color"), EditorStringName(Editor)));
	} else if (!selectable_nodes.has(p_node->get_instance_id())) {
		item->set_custom_color(0, get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
	}
}

void SceneTreeSelector::_on_item_edited() {
	TreeItem *edited_item = scene_tree->get_edited();
	int edited_column = scene_tree->get_edited_column();

	if (!edited_item) {
		return;
	}

	if (edited_item->get_cell_mode(edited_column) == TreeItem::CELL_MODE_CHECK) {
		bool is_checked = edited_item->is_checked(edited_column);
		ObjectID id = item_node_map.get(edited_item->get_instance_id());
		if (is_checked) {
			selected_nodes.insert(id);
		} else {
			selected_nodes.erase(id);
		}
		emit_signal("selection_changed");
	}
}

void SceneTreeSelector::_on_select_all_toggled(bool p_pressed) {
	selected_nodes.clear();
	for (const KeyValue<ObjectID, ObjectID> &E : node_item_map) {
		if (!selectable_nodes.has(E.key)) {
			continue;
		}

		Object *obj = ObjectDB::get_instance(E.value);
		ERR_FAIL_NULL_MSG(obj, "TreeItem not found for node with instance ID: " + itos(E.key));
		TreeItem *item = Object::cast_to<TreeItem>(obj);
		if (!item) {
			continue;
		}

		item->set_checked(0, p_pressed);
		if (p_pressed) {
			selected_nodes.insert(E.key);
		}
	}

	emit_signal("selection_changed");
}

void SceneTreeSelector::_on_show_all_toggled(bool p_pressed) {
	show_all_nodes = p_pressed;

	item_node_map.clear();
	node_item_map.clear();
	scene_tree->clear();

	AHashMap<Node *, bool> memoized_filter_result;
	_update_subtree(scene_root, memoized_filter_result);
	_reapply_selection(selected_nodes);
}

void SceneTreeSelector::_bind_methods() {
	ADD_SIGNAL(MethodInfo("selection_changed"));
}

void SceneTreeSelector::_update_subtree(Node *p_node, AHashMap<Node *, bool> &memoized_filter_result) {
	if (p_node == scene_root) {
		_add_item(nullptr, p_node, 0);
	} else if (_matches_filter(p_node, memoized_filter_result)) {
		_add_item(p_node->get_parent(), p_node, p_node->get_index(false));
	}

	for (int i = 0; i < p_node->get_child_count(false); i++) {
		Node *child = p_node->get_child(i, false);
		_update_subtree(child, memoized_filter_result);
	}
}

bool SceneTreeSelector::_matches_filter(Node *p_node, AHashMap<Node *, bool> &memoized_filter_result) {
	if (show_all_nodes) {
		return true;
	}

	if (memoized_filter_result.has(p_node)) {
		return memoized_filter_result.get(p_node);
	}

	ObjectID id = p_node->get_instance_id();
	if (selectable_nodes.has(id) || marked_nodes.has(id)) {
		memoized_filter_result.insert(p_node, true);
		return true;
	}

	for (int i = 0; i < p_node->get_child_count(false); i++) {
		Node *child = p_node->get_child(i, false);
		if (_matches_filter(child, memoized_filter_result)) {
			memoized_filter_result.insert(p_node, true);
			return true;
		}
	}

	memoized_filter_result.insert(p_node, false);
	return false;
}

void SceneTreeSelector::_reapply_selection(const HashSet<ObjectID> &p_selected_nodes) {
	for (const ObjectID &id : p_selected_nodes) {
		if (!node_item_map.has(id)) {
			continue;
		}
		Object *obj = ObjectDB::get_instance(node_item_map.get(id));
		ERR_FAIL_NULL_MSG(obj, "TreeItem not found for node with instance ID: " + itos(id));
		TreeItem *item = Object::cast_to<TreeItem>(obj);
		if (!item) {
			continue;
		}
		item->set_checked(0, true);
	}
}

void SceneTreeSelector::clear() {
	scene_tree->clear();
	item_node_map.clear();
	node_item_map.clear();
	selected_nodes.clear();
	selectable_nodes.clear();
	marked_nodes.clear();
}

void SceneTreeSelector::create(Node *p_root, const HashSet<ObjectID> &p_selectable_nodes, const HashSet<ObjectID> &p_marked_nodes) {
	ERR_FAIL_NULL(p_root);
	clear();
	scene_root = p_root;
	selectable_nodes = p_selectable_nodes;
	marked_nodes = p_marked_nodes;
	show_all_checkbox->set_pressed_no_signal(show_all_nodes);

	AHashMap<Node *, bool> memoized_filter_result;
	_update_subtree(scene_root, memoized_filter_result);

	select_all_checkbox->set_pressed_no_signal(true);
	_on_select_all_toggled(true);
}

HashSet<Node *> SceneTreeSelector::get_selected_nodes() const {
	HashSet<Node *> result;
	for (const ObjectID &id : selected_nodes) {
		Object *obj = ObjectDB::get_instance(id);
		if (!obj) {
			continue;
		}
		Node *node = Object::cast_to<Node>(obj);
		if (!node) {
			continue;
		}
		result.insert(node);
	}
	return result;
}

SceneTreeSelector::SceneTreeSelector() {
	HBoxContainer *button_container = memnew(HBoxContainer);
	add_child(button_container);

	select_all_checkbox = memnew(CheckBox);
	select_all_checkbox->set_text(TTR("Select All"));
	select_all_checkbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	button_container->add_child(select_all_checkbox);
	select_all_checkbox->connect(SceneStringName(toggled), callable_mp(this, &SceneTreeSelector::_on_select_all_toggled));

	show_all_checkbox = memnew(CheckButton);
	show_all_checkbox->set_text(TTR("Show All"));
	show_all_checkbox->set_h_size_flags(Control::SIZE_SHRINK_END);
	button_container->add_child(show_all_checkbox);
	show_all_checkbox->connect(SceneStringName(toggled), callable_mp(this, &SceneTreeSelector::_on_show_all_toggled));

	scene_tree = memnew(Tree);
	scene_tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	scene_tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	scene_tree->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	scene_tree->set_allow_reselect(true);
	scene_tree->add_theme_constant_override("button_margin", 0);
	scene_tree->add_theme_constant_override("icon_max_width", get_theme_constant(SNAME("class_icon_size"), EditorStringName(Editor)));
	add_child(scene_tree);

	scene_tree->connect("item_edited", callable_mp(this, &SceneTreeSelector::_on_item_edited));
}
