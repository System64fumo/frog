#include "main.hpp"
#include "window.hpp"
#include "place.hpp"
#include "file.hpp"
#include "icons.hpp"
#include "disk.hpp"
#include "utils.hpp"
#include "xdg_dirs.hpp"
#include "dir_watcher.hpp"
#include "config_parser.hpp"

#include <gtk4-layer-shell.h>
#include <gtkmm/eventcontrollerkey.h>
#include <gtkmm/separator.h>
#include <gtkmm/droptarget.h>
#include <gdkmm/clipboard.h>
#include <glibmm/vectorutils.h>
#include <gtkmm/cssprovider.h>

frog::frog() {
	set_title("Frog");
	set_default_size(800, 400);
	set_child(overlay_main);
	overlay_main.set_child(box_main);
	icon_theme = Gtk::IconTheme::get_for_display(Gdk::Display::get_default());

	get_xdg_user_dirs();
	load_icon_map();
	menu_file_setup();
	menu_dir_setup();

	// Desktop mode
	if (desktop_mode) {
		get_style_context()->add_class("desktop");

		gtk_layer_init_for_window(gobj());
		gtk_layer_set_namespace(gobj(), "desk_frog");
		gtk_layer_set_keyboard_mode(gobj(), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
		gtk_layer_set_layer(gobj(), GTK_LAYER_SHELL_LAYER_BACKGROUND);

		gtk_layer_set_anchor(gobj(), GTK_LAYER_SHELL_EDGE_LEFT, true);
		gtk_layer_set_anchor(gobj(), GTK_LAYER_SHELL_EDGE_RIGHT, true);
		gtk_layer_set_anchor(gobj(), GTK_LAYER_SHELL_EDGE_TOP, true);
		gtk_layer_set_anchor(gobj(), GTK_LAYER_SHELL_EDGE_BOTTOM, true);

		start_path = (std::string)getenv("HOME") + "/Desktop";
	}
	else {
		navbar_setup();
		sidebar_setup();

		entry_path.set_icon_from_icon_name("sidebar-expand", Gtk::Entry::IconPosition::PRIMARY);
		entry_path.set_icon_from_icon_name("document-swap-symbolic", Gtk::Entry::IconPosition::SECONDARY);
		entry_path.get_children()[2]->set_visible(false);

		box_status.append(entry_path);
		entry_path.get_style_context()->add_class("path_bar");
		entry_path.signal_activate().connect(sigc::mem_fun(*this, &frog::on_entry_done));
		entry_path.signal_changed().connect(sigc::mem_fun(*this, &frog::on_entry_changed));
		entry_path.set_hexpand(true);
		entry_path.set_activates_default(false);
		entry_path.signal_icon_press().connect([&](Gtk::Entry::IconPosition pos) {
			if (pos == Gtk::Entry::IconPosition::PRIMARY) {
				sidebar_should_hide = false;
				revealer_sidebar.set_reveal_child(true);
				box_overlay.set_visible(true);
			}
			else {
				std::printf("Expand file operations menu\n");
			}
		}, true);

		switch_layout();
	}

	show();

	box_main.append(box_status);
	box_main.get_style_context()->add_class("file_container");
	box_main.set_hexpand(true);
	box_main.set_orientation(Gtk::Orientation::VERTICAL);

	// Somehow adding this box fixes critical errors when dragging to select several files/folders??
	// It also seems to fix drag and drop in general..
	// Why.. How..
	Gtk::Box *box = Gtk::make_managed<Gtk::Box>();
	box_main.append(scrolled_window_files);
	scrolled_window_files.set_child(*box);
	scrolled_window_files.set_vexpand(true);

	box->append(flowbox_files);

	box_main.append(button_dummy);
	button_dummy.grab_focus();
	button_dummy.set_visible(false);

	auto event_controller_key = Gtk::EventControllerKey::create();
	event_controller_key->signal_key_pressed().connect(
		sigc::mem_fun(*this, &frog::on_key_press), true);
	add_controller(event_controller_key);

	Glib::RefPtr<Gtk::GestureClick> click_gesture = Gtk::GestureClick::create();
	Glib::RefPtr<Gtk::GestureClick> right_click_gesture = Gtk::GestureClick::create();
	click_gesture->set_button(GDK_BUTTON_PRIMARY);
	right_click_gesture->set_button(GDK_BUTTON_SECONDARY);
	click_gesture->signal_released().connect([&](const int &n_press, const double &x, const double &y) {
		// TODO: This does not unfocus the path entry
		// TODO: This still doesn't fully stop any active rename operations
		auto children = flowbox_files.get_selected_children();
		if (children.size() == 0)
			return;

		auto f_entry = dynamic_cast<file_entry*>(children[0]->get_child());
		f_entry->label.stop_editing(false);

		flowbox_files.unselect_all();
	});
	right_click_gesture->signal_pressed().connect([&](const int &n_press, const double &x, const double &y) {
		Gdk::Rectangle rect(x, y, 0, 0);
		popovermenu_context_menu.unparent();
		popovermenu_context_menu.set_parent(scrolled_window_files);
		popovermenu_context_menu.set_pointing_to(rect);
		popovermenu_context_menu.set_menu_model(menu_dir);
		popovermenu_context_menu.popup();
	});

	scrolled_window_files.add_controller(click_gesture);
	scrolled_window_files.add_controller(right_click_gesture);

	dispatcher_file_change.connect(sigc::mem_fun(*this, &frog::on_dispatcher_file_change));

	flowbox_files.signal_child_activated().connect(sigc::mem_fun(*this, &frog::on_filebox_child_activated));
	flowbox_files.set_sort_func(sigc::mem_fun(*this, &frog::sort_func));
	flowbox_files.set_activate_on_single_click(false);
	flowbox_files.set_valign(Gtk::Align::START); // TODO: Undo this eventually
	flowbox_files.set_homogeneous(true);
	flowbox_files.set_max_children_per_line(128);
	flowbox_files.set_row_spacing(30);
	flowbox_files.set_column_spacing(30);

	flowbox_files.set_selection_mode(Gtk::SelectionMode::MULTIPLE);
	auto target = Gtk::DropTarget::create(GDK_TYPE_FILE_LIST, Gdk::DragAction::MOVE);
	target->signal_drop().connect([&](const Glib::ValueBase& value, double, double) {
		Glib::Value<GSList*> gslist_value;
		gslist_value.init(value.gobj());
		auto files = Glib::SListHandler<Glib::RefPtr<Gio::File>>::slist_to_vector(gslist_value.get(), Glib::OwnershipType::OWNERSHIP_NONE);
		for (const auto& file : files) {
			Glib::RefPtr<Gio::File> destination = Gio::File::create_for_path(current_path.string() + "/" + file->get_basename());

			// TODO: Handle conflicts
			if (destination->query_exists()) {
				std::fprintf(stderr, "File already exists, Not moving file.\n");
				continue;
			}

			// TODO: Check for permissions (Both source and dest)
			std::printf("Moving file: %s\n", file->get_path().c_str());
			std::printf("To: %s\n", current_path.c_str());
			file->move(destination);
		}

		return true;
	}, false);
	scrolled_window_files.add_controller(target);

	// Open in the home directory by default
	if (start_path.empty())
		start_path = getenv("HOME");

	// Open in the current directory as a fallback
	if (start_path.empty())
		start_path = std::filesystem::current_path().string();

	entry_path.set_text(start_path);
	on_entry_done();

	const std::string& style_path = "/usr/share/sys64/frog/style.css";
	const std::string& style_path_usr = std::string(getenv("HOME")) + "/.config/sys64/frog/style.css";

	// Load base style
	if (std::filesystem::exists(style_path)) {
		auto css = Gtk::CssProvider::create();
		css->load_from_path(style_path);
		get_style_context()->add_provider_for_display(property_display(), css, GTK_STYLE_PROVIDER_PRIORITY_USER);
	}
	// Load user style
	if (std::filesystem::exists(style_path_usr)) {
		auto css = Gtk::CssProvider::create();
		css->load_from_path(style_path_usr);
		get_style_context()->add_provider_for_display(property_display(), css, GTK_STYLE_PROVIDER_PRIORITY_USER);
	}
}

void frog::navbar_setup() {
	box_navigation.set_halign(Gtk::Align::CENTER);
	box_navigation.append(button_previous);
	box_navigation.get_style_context()->add_class("navbar");

	button_previous.get_style_context()->add_class("flat");
	button_previous.set_icon_name("go-previous-symbolic");
	button_previous.set_sensitive(false);
	button_previous.set_focusable(false);
	button_previous.signal_clicked().connect(sigc::mem_fun(*this, &frog::navigate_hist_previous));

	box_navigation.append(button_next);
	button_next.get_style_context()->add_class("flat");
	button_next.set_icon_name("go-next-symbolic");
	button_next.set_sensitive(false);
	button_next.set_focusable(false);
	button_next.signal_clicked().connect(sigc::mem_fun(*this, &frog::navigate_hist_forward));

	box_navigation.append(button_up);
	button_up.get_style_context()->add_class("flat");
	button_up.set_icon_name("go-up-symbolic");
	button_up.set_sensitive(true);
	button_up.set_focusable(false);
	button_up.signal_clicked().connect(sigc::mem_fun(*this, &frog::navigate_up_dir));

	// TODO Enable this eventually
	/*box_navigation.append(button_search);
	button_search.get_style_context()->add_class("flat");
	button_search.set_icon_name("search-symbolic");
	button_search.set_focusable(false);*/
}

void frog::sidebar_setup() {
	overlay_main.add_overlay(box_overlay);
	box_overlay.get_style_context()->add_class("box_overlay");
	box_overlay.set_visible(false);
	Glib::RefPtr<Gtk::GestureClick> click_gesture = Gtk::GestureClick::create();
	click_gesture->set_button(GDK_BUTTON_PRIMARY);
	click_gesture->signal_pressed().connect([&](
		const int& n_press,
		const double& x,
		const double& y) {
			sidebar_should_hide = true;
			revealer_sidebar.set_reveal_child(false);
			box_overlay.set_visible(false);
	});
	box_overlay.add_controller(click_gesture);

	overlay_main.add_overlay(revealer_sidebar);
	revealer_sidebar.get_style_context()->add_class("revealer_sidebar");
	revealer_sidebar.set_child(box_sidebar);
	revealer_sidebar.set_reveal_child(true);
	revealer_sidebar.set_transition_type(Gtk::RevealerTransitionType::SLIDE_RIGHT);
	revealer_sidebar.set_halign(Gtk::Align::START);

	box_sidebar.set_size_request(175, -1);
	box_sidebar.set_orientation(Gtk::Orientation::VERTICAL);
	box_sidebar.append(box_navigation);
	box_sidebar.get_style_context()->add_class("sidebar");
	box_sidebar.append(scrolled_window_places);

	scrolled_window_places.set_child(flowbox_places);
	scrolled_window_places.set_vexpand(true);
	scrolled_window_places.set_hexpand_set(true);
	flowbox_places.set_valign(Gtk::Align::START);
	flowbox_places.set_max_children_per_line(1);

	// Pinned items
	flowbox_places.signal_child_activated().connect(sigc::mem_fun(*this, &frog::on_places_child_activated));
	config_parser config(std::string(getenv("HOME")) + "/.config/sys64/frog/config.conf");
	std::vector<std::string> keys = config.get_keys("pinned");

	for (const auto &key : keys) {
		std::string cfg_pinned_place = config.get_value("pinned", key);
		place *place_entry = Gtk::make_managed<place>(key, cfg_pinned_place);
		flowbox_places.append(*place_entry);
	}

	Gtk::Separator *separator = Gtk::make_managed<Gtk::Separator>();
	flowbox_places.append(*separator);
	separator->get_parent()->set_sensitive(false);

	// TODO: Remove all of the verbose output
	// And maybe consider adding tooltips to places
	disk_manager* dm = new disk_manager();
	auto disks = dm->get_disks();
	for (const auto& disk : disks) {
		for (const auto& partition : disk.partitions) {
			if (!partition.should_show)
				continue;

			const std::string icon = disk.removable ? "drive-removable-media-usb-symbolic" : "drive-harddisk-symbolic";
			place *place_entry = Gtk::make_managed<place>(partition.label, partition.mount_path, icon, partition);
			if (!partition.mount_path.empty()) {
				place_entry->set_tooltip_text(to_human_readable(partition.used_bytes) + "/" + to_human_readable(partition.total_bytes));
			}
			flowbox_places.append(*place_entry);
		}
	}
}

bool frog::on_key_press(const guint &keyval, const guint &keycode, const Gdk::ModifierType &state) {
	// TODO: Add auto navigation

	// Escape key
	if (keycode == 9) {
		button_dummy.grab_focus();
		entry_path.set_text(current_path.string());
	}

	// Slash key
	else if (keycode == 61) {
		entry_path.grab_focus();
		entry_path.set_text("/");
		entry_path.set_position(1);
	}

	// Other printable characters
	else if (g_unichar_isprint(gdk_keyval_to_unicode(keyval))) {
		entry_path.set_text(entry_path.get_text() + "/" + gdk_keyval_to_unicode(keyval));
		entry_path.grab_focus();
		entry_path.set_position(entry_path.get_text_length());
		return true;
	}


	return false;
}

void frog::on_entry_done() {
	navigate_to_dir(entry_path.get_buffer()->get_text().raw());
	entry_path.set_position(entry_path.get_text_length());
	button_dummy.grab_focus();
}

void frog::on_entry_changed() {
	// TODO: This is not perfect
	std::filesystem::directory_entry entry(entry_path.get_text().c_str());
	bool exists = entry.exists();
	bool is_dir = entry.is_directory();
	bool ends_with_slash = (entry_path.get_text()[entry_path.get_text_length() - 1] == '/');
	bool is_root = entry_path.get_text() == "/";

	if (exists && is_dir && (!ends_with_slash || is_root)) {
		generate_entry_autocomplete(entry_path.get_text());
	}
}

int frog::sort_func(Gtk::FlowBoxChild *child1, Gtk::FlowBoxChild *child2) {
	auto item1 = dynamic_cast<file_entry*>(child1->get_child());
	auto item2 = dynamic_cast<file_entry*>(child2->get_child());

	if (item1->is_directory != item2->is_directory)
		return item1->is_directory ? -1 : 1;

	if (item1->label.get_text()[0] == '.' && item2->label.get_text()[0] != '.')
		return 1;
	if (item1->label.get_text()[0] != '.' && item2->label.get_text()[0] == '.')
		return -1;

	return item1->label.get_text().compare(item2->label.get_text());
}

void frog::on_filebox_child_activated(Gtk::FlowBoxChild* child) {
	file_entry *entry = dynamic_cast<file_entry*>(child->get_child());
	std::string path = entry_path.get_buffer()->get_text().raw();

	// TODO: Can't open folders from desktop mode
	if (entry->is_directory && !desktop_mode) {
		next_paths.clear();
		navigate_to_dir(std::filesystem::path(path + "/" + entry->label.get_text()));
	}
	else {
		std::string cmd = "xdg-open \"" + path + '/' + entry->label.get_text() + '"';
		system(cmd.c_str());
	}
}

void frog::on_places_child_activated(Gtk::FlowBoxChild* child) {
	place *place_entry = dynamic_cast<place*>(child->get_child());
	if (!place_entry)
		return;

	// TODO: Add a polkit thing to mount the disk

	next_paths.clear();
	navigate_to_dir(place_entry->file_path);
}

void frog::on_dispatcher_file_change() {
	const std::string event_type = watcher->event_type.front();
	watcher->event_type.pop();

	const std::string event_name = watcher->event_name.front();
	watcher->event_name.pop();

	// Temporary
	std::printf("File: %s %s\n", event_name.c_str(), event_type.c_str());

	// Handle file events
	// TODO: While moving/renaming files can be done this way
	// I would like to eventually just rename the label outright
	if (event_type == "created" || event_type == "moved_to") {
		std::filesystem::directory_entry entry(event_name);
		create_file_entry(entry, true);
		return;
	}
	else if (event_type == "deleted" || event_type == "moved_from") {
		auto children = flowbox_files.get_children();
		for (auto child : children) {
			auto fbox_child = dynamic_cast<Gtk::FlowBoxChild*>(child);
			auto f_entry = dynamic_cast<file_entry*>(fbox_child->get_child());
			if (f_entry->path == event_name) {
				// Remove the popover if found
				std::vector<Gtk::Widget*> f_entry_children = fbox_child->get_children();
				if (f_entry_children.size() > 1)
					f_entry_children[1]->unparent();

				// Remove the file entry
				flowbox_files.remove(*fbox_child);
				return;
			}
		}
	}
	else if (event_type == "modified") {
		return; // For now this doesn't really matter
	}
	else {
		// Fallback solution
		navigate_to_dir(current_path);
		current_path = "";
	}
}

void frog::create_file_entry(const std::filesystem::directory_entry &entry, const bool& load_thumbnail) {
	file_entry *f_entry = Gtk::make_managed<file_entry>(this, entry);
	Gtk::FlowBoxChild *fbox_child = Gtk::make_managed<Gtk::FlowBoxChild>();

	fbox_child->set_size_request(96,110);
	fbox_child->set_child(*f_entry);
	fbox_child->set_focusable(false); // Fixes focus issue when renaming
	fbox_child->set_valign(Gtk::Align::START);

	Glib::RefPtr<Gtk::GestureClick> click_gesture = Gtk::GestureClick::create();
	click_gesture->set_button(GDK_BUTTON_SECONDARY);
	click_gesture->signal_pressed().connect(sigc::bind(sigc::mem_fun(*this, &frog::on_right_clicked), fbox_child));
	f_entry->add_controller(click_gesture);

	if (load_thumbnail)
		f_entry->load_thumbnail();

	Glib::signal_idle().connect_once([&, fbox_child] {
		flowbox_files.append(*fbox_child);
	});
}

void frog::snapshot_vfunc(const Glib::RefPtr<Gtk::Snapshot>& snapshot) {
	Glib::signal_idle().connect([&]() {
		switch_layout();
		return true;
	});

	// Render normally
	Gtk::Window::snapshot_vfunc(snapshot);
}

void frog::switch_layout() {
	// TODO: This could be better
	if (get_width() > 480) { // Desktop UI
		if (!revealer_sidebar.get_style_context()->has_class("mobile"))
			return;

		revealer_sidebar.set_reveal_child(true);
		box_main.set_margin_start(175);
		box_overlay.set_visible(false);
		sidebar_should_hide = true;
		entry_path.get_children()[0]->set_visible(false);
		revealer_sidebar.get_style_context()->remove_class("mobile");
		button_dummy.grab_focus();
	}
	else { // Mobile UI
		if (revealer_sidebar.get_style_context()->has_class("mobile"))
			return;

		if (sidebar_should_hide) {
			entry_path.get_children()[0]->set_visible(true);
			revealer_sidebar.set_reveal_child(false);
			box_main.set_margin_start(0);
			button_dummy.grab_focus();
		}
		revealer_sidebar.get_style_context()->add_class("mobile");
	}
}
