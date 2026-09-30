/**************************************************************************/
/*  refactor_unique_name_dialog.cpp                                       */
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

#include "refactor_unique_name_dialog.h"

#include "core/io/resource_saver.h"
#include "core/object/callable_mp.h"
#include "core/object/script_language.h"
#include "editor/docks/scene_tree_dock.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/gui/editor_validation_panel.h"
#include "editor/script/script_editor_plugin.h"
#include "editor/settings/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/main/scene_tree.h"

#include "modules/gdscript/gdscript.h"

static bool _is_identifier_char(char32_t p_char) {
	return (p_char >= 'a' && p_char <= 'z') ||
			(p_char >= 'A' && p_char <= 'Z') ||
			(p_char >= '0' && p_char <= '9') ||
			p_char == '_';
}

static HashSet<Node *> _resolve_nodes(const HashSet<ObjectID> &p_node_ids) {
	HashSet<Node *> result;
	for (const ObjectID &id : p_node_ids) {
		Node *node = Object::cast_to<Node>(ObjectDB::get_instance(id));
		if (node) {
			result.insert(node);
		}
	}
	return result;
}

// Same lookup as the "View Owners" dialog, using the dependencies cached by the file system.
static void _find_script_users(EditorFileSystemDirectory *p_dir, const HashSet<String> &p_script_paths, const String &p_excluded_path, HashMap<String, Vector<String>> &r_users) {
	if (!p_dir) {
		return;
	}

	for (int i = 0; i < p_dir->get_subdir_count(); i++) {
		_find_script_users(p_dir->get_subdir(i), p_script_paths, p_excluded_path, r_users);
	}

	for (int i = 0; i < p_dir->get_file_count(); i++) {
		const String file_path = p_dir->get_file_path(i);
		if (file_path == p_excluded_path) {
			continue;
		}
		for (const String &dep : p_dir->get_file_deps(i)) {
			if (p_script_paths.has(dep)) {
				r_users[dep].push_back(file_path);
			}
		}
	}
}

static bool _is_whole_word_match(const String &p_text, int p_pos, int p_len) {
	const bool left_boundary = p_pos == 0 || !_is_identifier_char(p_text[p_pos - 1]);
	const int end = p_pos + p_len;
	const bool right_boundary = end >= p_text.length() || p_text[end] == '/' || !_is_identifier_char(p_text[end]);
	return left_boundary && right_boundary;
}

static bool _contains_whole_word(const String &p_text, const String &p_token) {
	if (p_token.is_empty()) {
		return false;
	}

	int from = 0;
	while (true) {
		const int pos = p_text.find(p_token, from);
		if (pos == -1) {
			return false;
		}
		if (_is_whole_word_match(p_text, pos, p_token.length())) {
			return true;
		}
		from = pos + p_token.length();
	}
}

static String _replace_whole_word(const String &p_text, const String &p_old_token, const String &p_new_token, bool &r_replaced) {
	r_replaced = false;
	if (p_old_token.is_empty()) {
		return p_text;
	}

	String result;
	int from = 0;
	while (true) {
		const int pos = p_text.find(p_old_token, from);
		if (pos == -1) {
			result += p_text.substr(from);
			break;
		}
		result += p_text.substr(from, pos - from);
		if (_is_whole_word_match(p_text, pos, p_old_token.length())) {
			result += p_new_token;
			r_replaced = true;
		} else {
			result += p_old_token;
		}
		from = pos + p_old_token.length();
	}

	return result;
}

RefactorUniqueNameDialog::RefactorUniqueNameDialog() {
	set_title(TTRC("Refactor Unique Name"));
	set_ok_button_text(TTRC("Update selected scripts"));
	// Stays open while confirming the update of scripts used elsewhere.
	set_hide_on_ok(false);

	VBoxContainer *vbox = memnew(VBoxContainer);
	vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	vbox->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(vbox);

	label = memnew(Label);
	label->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	vbox->add_child(label);

	validation_panel = memnew(EditorValidationPanel);
	validation_panel->set_v_size_flags(Control::SIZE_FILL);
	validation_panel->add_line(MSG_ID_SCRIPTS);
	validation_panel->add_line(MSG_ID_SHARED_SCRIPTS);
	validation_panel->set_update_callback(callable_mp(this, &RefactorUniqueNameDialog::_update_validation_panel));

	scene_tree_selector = memnew(SceneTreeSelector);
	scene_tree_selector->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	scene_tree_selector->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	scene_tree_selector->connect("selection_changed", callable_mp(validation_panel, &EditorValidationPanel::update));

	vbox->add_child(scene_tree_selector);
	vbox->add_child(validation_panel);

	shared_scripts_confirmation = memnew(ConfirmationDialog);
	shared_scripts_confirmation->set_title(TTRC("Scripts Used Elsewhere"));
	shared_scripts_confirmation->set_ok_button_text(TTRC("Update Anyway"));
	shared_scripts_confirmation->connect(SceneStringName(confirmed), callable_mp(this, &RefactorUniqueNameDialog::_apply_refactor));
	add_child(shared_scripts_confirmation);
}

void RefactorUniqueNameDialog::add_refactor(const StringName &p_old_name, const StringName &p_new_name) {
	const int refactor_setting = int(EDITOR_GET("docks/scene_tree/unique_name_refactor"));
	if (refactor_setting == RefactorUniqueNameDialog::NEVER_REFACTOR) {
		return;
	}

	refactor_queue.push_back(RefactorData{ p_old_name, p_new_name });
	if (!is_visible()) {
		_next();
	}
}

void RefactorUniqueNameDialog::_next() {
	if (refactor_queue.is_empty()) {
		return;
	}

	const RefactorData &refactor_data = refactor_queue[0];
	const HashSet<ObjectID> nodes_to_consider = _get_nodes_to_consider(refactor_data.old_name);

	if (nodes_to_consider.is_empty()) {
		refactor_queue.remove_at(0);
		return _next();
	}

	// A script also used by other files may refer to a different node there, in which case updating it would break it.
	HashSet<String> script_paths;
	for (Node *n : _resolve_nodes(nodes_to_consider)) {
		Ref<Script> script = n->get_script();
		script_paths.insert(script->get_path());
	}
	shared_scripts.clear();
	_find_script_users(EditorFileSystem::get_singleton()->get_filesystem(), script_paths, get_scene_root()->get_scene_file_path(), shared_scripts);

	const int refactor_setting = int(EDITOR_GET("docks/scene_tree/unique_name_refactor"));
	// Let the user decide about shared scripts, even if confirmation is disabled.
	if (refactor_setting == RefactorUniqueNameDialog::REFACTOR_WITHOUT_CONFIRMATION && shared_scripts.is_empty()) {
		_refactor_unique_name(refactor_data, _resolve_nodes(nodes_to_consider));
		refactor_queue.remove_at(0);
		return _next();
	}

	_popup_refactor(refactor_data, nodes_to_consider);
}

void RefactorUniqueNameDialog::_update_validation_panel() {
	if (refactor_queue.is_empty()) {
		validation_panel->set_message(MSG_ID_SCRIPTS, "", EditorValidationPanel::MSG_INFO);
		return;
	}

	HashSet<Node *> selected_nodes = scene_tree_selector->get_selected_nodes();
	HashSet<String> selected_script_paths;

	for (Node *node : selected_nodes) {
		Ref<Script> script = node->get_script();
		if (!script.is_valid()) {
			continue;
		}

		const String path = script->get_path();
		if (path.is_empty()) {
			continue;
		}

		selected_script_paths.insert(path);
	}

	if (selected_script_paths.is_empty()) {
		validation_panel->set_message(MSG_ID_SCRIPTS, TTRC("No script files selected."), EditorValidationPanel::MSG_OK, false);
		return;
	}

	Vector<String> script_paths;
	for (const String &path : selected_script_paths) {
		script_paths.push_back(path);
	}
	script_paths.sort();

	String message = vformat(TTRC("Script files to update (%d):"), script_paths.size());
	for (int i = 0; i < script_paths.size(); i++) {
		message += String(U"\n•  ") + script_paths[i];
	}

	validation_panel->set_message(MSG_ID_SCRIPTS, message, EditorValidationPanel::MSG_OK, false);

	const String shared_text = _get_shared_scripts_text(selected_nodes);
	if (!shared_text.is_empty()) {
		validation_panel->set_message(MSG_ID_SHARED_SCRIPTS, TTRC("Also used by other files, where they will probably no longer work:") + shared_text, EditorValidationPanel::MSG_WARNING, false);
	}
}

String RefactorUniqueNameDialog::_get_shared_scripts_text(const HashSet<Node *> &p_nodes) const {
	Vector<String> script_paths;
	for (Node *node : p_nodes) {
		Ref<Script> script = node->get_script();
		if (script.is_valid() && shared_scripts.has(script->get_path()) && !script_paths.has(script->get_path())) {
			script_paths.push_back(script->get_path());
		}
	}
	script_paths.sort();

	String text;
	for (const String &path : script_paths) {
		text += vformat(String(U"\n•  %s (%s)"), path, String(", ").join(shared_scripts[path]));
	}
	return text;
}

void RefactorUniqueNameDialog::ok_pressed() {
	const String shared_text = _get_shared_scripts_text(scene_tree_selector->get_selected_nodes());
	if (!shared_text.is_empty()) {
		// Canceling returns to this dialog, which stays open.
		shared_scripts_confirmation->set_text(TTR("These scripts are also used by other files, where they will probably no longer work once updated:") + shared_text + "\n\n" + TTR("Update them anyway?"));
		shared_scripts_confirmation->popup_centered();
		return;
	}
	_apply_refactor();
}

void RefactorUniqueNameDialog::_apply_refactor() {
	hide();
	if (!refactor_queue.is_empty()) {
		const RefactorData &refactor_data = refactor_queue[0];
		_refactor_unique_name(refactor_data, scene_tree_selector->get_selected_nodes());
		refactor_queue.remove_at(0);
	}
	callable_mp(this, &RefactorUniqueNameDialog::_next).call_deferred();
}

void RefactorUniqueNameDialog::_popup_refactor(const RefactorData &p_refactor_data, const HashSet<ObjectID> &p_nodes_to_consider) {
	label->set_text(vformat(TTRC("Node with unique name \"%s\" was renamed \"%s\" \nSelect script(s) to update"),
			p_refactor_data.old_name,
			p_refactor_data.new_name));

	Node *renamed_node = get_scene_root()->get_node_or_null("%" + String(p_refactor_data.new_name));
	HashSet<ObjectID> marked;
	if (renamed_node) {
		marked.insert(renamed_node->get_instance_id());
	}

	scene_tree_selector->create(get_scene_root(), p_nodes_to_consider, marked);
	validation_panel->update();
	popup_centered_clamped(Size2(350, 700) * EDSCALE);
}

void RefactorUniqueNameDialog::cancel_pressed() {
	if (!refactor_queue.is_empty()) {
		refactor_queue.remove_at(0);
	}
	callable_mp(this, &RefactorUniqueNameDialog::_next).call_deferred();
}

Node *RefactorUniqueNameDialog::get_scene_root() const {
	ERR_FAIL_COND_V(!is_inside_tree(), nullptr);

	return get_tree()->get_edited_scene_root();
}

void RefactorUniqueNameDialog::_refactor_unique_name(const RefactorData &p_refactor_data, const HashSet<Node *> &p_nodes) {
	if (p_nodes.is_empty()) {
		return;
	}

	const String old_str = "%" + String(p_refactor_data.old_name);
	const String new_str = "%" + String(p_refactor_data.new_name);

	for (Node *n : p_nodes) {
		Ref<Script> script = n->get_script();
		if (script.is_null()) {
			continue;
		}

		// Like "Find in Files", edit the script through its editor when it is open, so that
		// the change can be undone there and is not overwritten by the editor's text on save.
		TextEditorBase *teb = Object::cast_to<TextEditorBase>(ScriptEditor::get_singleton()->get_script_container()->get_resource_editor(script));
		if (teb) {
			CodeTextEditor *code_text_editor = teb->get_code_editor();
			CodeEdit *code_edit = code_text_editor->get_text_editor();
			bool replaced = false;
			const String source = _replace_whole_word(code_edit->get_text(), old_str, new_str, replaced);
			if (replaced) {
				code_edit->begin_complex_operation();
				Variant nav_state = code_text_editor->get_navigation_state();
				code_edit->set_text(source);
				code_edit->emit_signal(SceneStringName(text_changed));
				code_text_editor->set_edit_state(nav_state);
				code_edit->end_complex_operation();
			}
		} else {
			bool replaced = false;
			const String source = _replace_whole_word(script->get_source_code(), old_str, new_str, replaced);
			if (replaced) {
				script->set_source_code(source);
				ResourceSaver::save(script, script->get_path());
				script->reload();
			}
		}
	}

	SceneTreeDock *scene_tree_dock = SceneTreeDock::get_singleton();
	if (scene_tree_dock->is_visible_in_tree()) {
		scene_tree_dock->get_tree_editor()->get_scene_tree()->grab_focus();
	}
}

HashSet<ObjectID> RefactorUniqueNameDialog::_get_nodes_to_consider(const StringName &p_old_name) {
	ERR_FAIL_NULL_V(get_scene_root(), HashSet<ObjectID>());

	ScriptEditor::get_singleton()->get_script_container()->apply_scripts();

	HashSet<ObjectID> nodes_to_consider;
	LocalVector<Node *> stack;
	const String old_name_token = "%" + String(p_old_name);

	Node *scene_root = get_scene_root();
	stack.push_back(scene_root);
	while (!stack.is_empty()) {
		Node *current = stack[stack.size() - 1];
		stack.remove_at(stack.size() - 1);

		//if a node with the old unique name exists as child of the current node,
		//then the current node is a scene root with an other, similarly named unique node, we don't need to consider it for refactoring
		if (!current->get_node_or_null("%" + String(p_old_name))) {
			Ref<Script> script = current->get_script();
			// Built-in scripts are not supported, as they are only saved along with the scene that contains them.
			if (script.is_valid() && !script->is_built_in()) {
				ScriptLanguage *gdscript_language = script.ptr()->get_language();
				if (gdscript_language && gdscript_language->get_name() == "GDScript" && _contains_whole_word(script.ptr()->get_source_code(), old_name_token)) {
					nodes_to_consider.insert(current->get_instance_id());
				}
			}
		}

		for (int i = 0; i < current->get_child_count(); i++) {
			Node *child = current->get_child(i);
			if (child->get_owner() == scene_root) {
				stack.push_back(current->get_child(i));
			}
		}
	}

	return nodes_to_consider;
}
