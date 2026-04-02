/**************************************************************************/
/*  refactor_unique_name_dialog.h                                         */
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

#include "editor/scene/scene_tree_selector.h"

class EditorValidationPanel;

class RefactorUniqueNameDialog : public ConfirmationDialog {
	GDCLASS(RefactorUniqueNameDialog, ConfirmationDialog);
	enum {
		MSG_ID_SCRIPTS = 0,
	};

	enum PopupSettings {
		SHOW_POPUP = 0,
		REFACTOR_WITHOUT_CONFIRMATION = 1,
		NEVER_REFACTOR = 2,
	};

	struct RefactorData {
		StringName old_name;
		StringName new_name;
	};

	Vector<RefactorData> refactor_queue;
	Label *label = nullptr;
	EditorValidationPanel *validation_panel = nullptr;

	SceneTreeSelector *scene_tree_selector = nullptr;

	Node *get_scene_root() const;

	void _next();
	void _update_validation_panel();
	void _popup_refactor(const RefactorData &p_refactor_data, const HashSet<ObjectID> &p_nodes_to_consider);
	void _refactor_unique_name(const RefactorData &p_refactor_data, const HashSet<Node *> &p_nodes);
	HashSet<ObjectID> _get_nodes_to_consider(const StringName &p_old_name);

public:
	virtual void cancel_pressed() override;
	virtual void ok_pressed() override;

	void add_refactor(const StringName &p_old_name, const StringName &p_new_name);

	RefactorUniqueNameDialog();
};
