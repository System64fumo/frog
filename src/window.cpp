#include "main.hpp"
#include "window.hpp"
#include "place.hpp"
#include "file.hpp"
#include "css.hpp"
#include "config_parser.hpp"
#include "icons.hpp"
#include "disk.hpp"

#include <gtkmm/separator.h>
#include <gtkmm/droptarget.h>
#include <gdkmm/clipboard.h>
#include <glibmm/bytes.h>
#include <sys/statvfs.h>

frog::frog() {
	set_title("Frog");
	set_default_size(800, 400);
	set_child(box_main);
	show();

	navbar_setup();
	get_xdg_user_dirs();
	sidebar_setup();
	menu_file_setup();
	menu_dir_setup();
	load_icon_map();

	box_main.append(box_container);
	box_container.get_style_context()->add_class("file_container");
	box_container.set_hexpand(true);
	box_container.set_orientation(Gtk::Orientation::VERTICAL);

	box_container.append(entry_path);
	entry_path.get_style_context()->add_class("path_bar");
	entry_path.signal_activate().connect(sigc::mem_fun(*this, &frog::on_entry_done));

	// Somehow adding this box fixes critical errors when dragging to select several files/folders??
	// It also seems to fix drag and drop in general..
	// Why.. How..
	Gtk::Box *box = Gtk::make_managed<Gtk::Box>();
	box_container.append(scrolled_window_files);
	scrolled_window_files.set_child(*box);
	scrolled_window_files.set_vexpand(true);

	box->append(flowbox_files);

	Glib::RefPtr<Gtk::GestureClick> click_gesture = Gtk::GestureClick::create();
	Glib::RefPtr<Gtk::GestureClick> right_click_gesture = Gtk::GestureClick::create();
	click_gesture->set_button(GDK_BUTTON_PRIMARY);
	right_click_gesture->set_button(GDK_BUTTON_SECONDARY);
	click_gesture->signal_released().connect([&](const int &n_press, const double &x, const double &y) {
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

	dispatcher_files.connect(sigc::mem_fun(*this, &frog::on_dispatcher_files));
	dispatcher_file_change.connect(sigc::mem_fun(*this, &frog::on_dispatcher_file_change));

	flowbox_files.signal_child_activated().connect(sigc::mem_fun(*this, &frog::on_filebox_child_activated));
	flowbox_files.set_sort_func(sigc::mem_fun(*this, &frog::sort_func));
	flowbox_files.set_activate_on_single_click(false);
	flowbox_files.set_valign(Gtk::Align::START);
	flowbox_files.set_homogeneous(true);
	flowbox_files.set_max_children_per_line(128);
	flowbox_files.set_row_spacing(30);
	flowbox_files.set_column_spacing(30);

	flowbox_files.set_selection_mode(Gtk::SelectionMode::MULTIPLE);
	const GType ustring_type = Glib::Value<Glib::ustring>::value_type();
	auto target = Gtk::DropTarget::create(ustring_type, Gdk::DragAction::COPY);
	target->signal_drop().connect([](const Glib::ValueBase& value, double, double) {
		// TODO: Maybe i'm doing it wrong but this doesn't seem to read everything properly
		Glib::Value<Glib::ustring> ustring_value;
		ustring_value.init(value.gobj());
		std::printf("Got: %s\n", ustring_value.get().c_str());
		return true;
	}, false);
	flowbox_files.add_controller(target);


	//std::filesystem::current_path().string() // TODO: Add option to use this
	entry_path.set_text(getenv("HOME"));
	on_entry_done();

	// Load custom css
	std::string home_dir = getenv("HOME");
	std::string css_path = home_dir + "/.config/sys64/frog.css";
	css_loader css(css_path, this);
}

void frog::navbar_setup() {
	box_navigation.set_halign(Gtk::Align::CENTER);
	box_navigation.append(button_previous);
	box_navigation.get_style_context()->add_class("navbar");

	button_previous.get_style_context()->add_class("flat");
	button_previous.set_icon_name("go-previous-symbolic");
	button_previous.set_sensitive(false);
	button_previous.signal_clicked().connect(sigc::mem_fun(*this, &frog::navigate_hist_previous));

	box_navigation.append(button_next);
	button_next.get_style_context()->add_class("flat");
	button_next.set_icon_name("go-next-symbolic");
	button_next.set_sensitive(false);
	button_next.signal_clicked().connect(sigc::mem_fun(*this, &frog::navigate_hist_forward));

	box_navigation.append(button_up);
	button_up.get_style_context()->add_class("flat");
	button_up.set_icon_name("go-up-symbolic");
	button_up.set_sensitive(true);
	button_up.signal_clicked().connect(sigc::mem_fun(*this, &frog::navigate_up_dir));

	//box_navigation.append(button_search);
	//button_search.get_style_context()->add_class("flat");
	//button_search.set_icon_name("search-symbolic");
}

void frog::sidebar_setup() {
	box_main.append(box_sidebar);
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
	config_parser config(std::string(getenv("HOME")) + "/.config/sys64/frog.conf");
	std::vector<std::string> keys = config.get_keys("pinned");

	for (const auto &key : keys) {
		std::string cfg_pinned_place = config.get_value("pinned", key);
		place *place_entry = Gtk::make_managed<place>(key, cfg_pinned_place);
		flowbox_places.append(*place_entry);
	}

	Gtk::Separator *separator = Gtk::make_managed<Gtk::Separator>();
	flowbox_places.append(*separator);
	separator->get_parent()->set_sensitive(false);

	std::map<std::string, std::string> mounts = disk::get_mounts();
	std::vector<disk> disks;
	for (const auto& entry : std::filesystem::directory_iterator("/sys/block/")) {
		if (!entry.is_directory())
			continue;

		disk d(entry.path());
		for (const disk::partition& partition : d.partitions) {
			auto it = mounts.find(partition.name);
			if (it != mounts.end()) {
				std::string label = (!partition.label.empty()) ? partition.label : partition.name;
				struct statvfs stats;
				statvfs(it->second.c_str(), &stats);
				unsigned long total_bytes = stats.f_blocks * stats.f_frsize;
				unsigned long free_bytes = stats.f_bfree * stats.f_frsize;
				unsigned long used_bytes = total_bytes - free_bytes;
				double used = (double)used_bytes / (double)total_bytes;
				place *place_entry = Gtk::make_managed<place>(label, it->second, "drive-harddisk-symbolic", used);
				flowbox_places.append(*place_entry);
			}
		}
		disks.push_back(d);
	}
}

void frog::on_entry_done() {
	navigate_to_dir(entry_path.get_buffer()->get_text().raw());
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

	if (entry->is_directory) {
		std::string new_path = path + "/" + entry->label.get_text();
		next_paths.clear();
		navigate_to_dir(new_path);
	}
	else {
		std::string cmd = "xdg-open \"" + path + '/' + entry->label.get_text() + '"';
		system(cmd.c_str());
	}
}

void frog::on_places_child_activated(Gtk::FlowBoxChild *child) {
	place *place_entry = dynamic_cast<place*>(child->get_child());
	if (!place_entry)
		return;

	next_paths.clear();
	navigate_to_dir(place_entry->file_path);
}

void frog::on_dispatcher_files() {
	while (!widget_queue.empty()) {
		auto widget = widget_queue.front();
		widget_queue.pop();
		flowbox_files.append(*widget);
	}
}

void frog::on_dispatcher_file_change() {
	std::lock_guard<std::mutex> lock(watcher->queue_mutex);
	std::string event_type, event_name;

	event_type = watcher->event_type.front();
	watcher->event_type.pop();

	event_name = watcher->event_name.front();
	watcher->event_name.pop();

	// Temporary
	std::printf("File: %s %s\n", event_name.c_str(), event_type.c_str());

	// Handle file events
	// TODO: While moving/renaming files can be done this way
	// I would like to eventually just rename the label outright
	if (event_type == "created" || event_type == "moved_to") {
		std::filesystem::directory_entry entry(event_name);
		create_file_entry(entry);
		dispatcher_files.emit();
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
		std::string temp_path = current_path;
		current_path = "";
		navigate_to_dir(temp_path);
	}
}

void frog::create_file_entry(const std::filesystem::directory_entry &entry) {
	Gtk::FlowBoxChild *fbox_child = Gtk::make_managed<Gtk::FlowBoxChild>();
	fbox_child->set_size_request(96,96);
	file_entry *f_entry = Gtk::make_managed<file_entry>(entry);
	fbox_child->set_child(*f_entry);
	fbox_child->set_focusable(false); // Fixes focus issue when renaming

	Glib::RefPtr<Gtk::GestureClick> click_gesture = Gtk::GestureClick::create();
	click_gesture->set_button(GDK_BUTTON_SECONDARY);
	click_gesture->signal_pressed().connect(sigc::bind(sigc::mem_fun(*this, &frog::on_right_clicked), fbox_child));
	f_entry->add_controller(click_gesture);
	std::lock_guard<std::mutex> lock(queue_mutex);
	widget_queue.push(fbox_child);

	f_entry->get_style_context()->add_class("file_entry");
}
