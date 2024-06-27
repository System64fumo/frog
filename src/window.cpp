#include "main.hpp"
#include "window.hpp"
#include "file.hpp"

#include <iostream>
#include <filesystem>

frog::frog() {
	set_title("Frog");
	set_default_size(800, 400);
	set_child(box_main);
	show();

	// TODO: Add navigation buttons
	//box_main.append(box_sidebar);
	//box_sidebar.set_size_request(175, -1);

	box_main.append(box_container);
	box_container.set_hexpand(true);
	box_container.set_orientation(Gtk::Orientation::VERTICAL);

	box_container.append(entry_path);
	entry_path.signal_activate().connect(sigc::mem_fun(*this, &frog::on_search_done));

	box_container.append(scrolled_window_files);
	scrolled_window_files.set_child(flowbox_files);
	scrolled_window_files.set_vexpand(true);

	Glib::RefPtr<Gtk::GestureClick> click_gesture = Gtk::GestureClick::create();
	click_gesture->set_button(GDK_BUTTON_PRIMARY);
	click_gesture->signal_pressed().connect([&](const int &n_press, const double &x, const double &y) {
		flowbox_files.unselect_all();
	});
	scrolled_window_files.add_controller(click_gesture);

	flowbox_files.signal_child_activated().connect(sigc::mem_fun(*this, &frog::on_child_activated));
	flowbox_files.set_activate_on_single_click(false);
	flowbox_files.set_valign(Gtk::Align::START);
	flowbox_files.set_homogeneous(true);
	flowbox_files.set_max_children_per_line(128);
	flowbox_files.set_row_spacing(5);
	flowbox_files.set_column_spacing(5);

	//std::filesystem::current_path().string() // TODO: Add option to use this
	entry_path.set_text(getenv("HOME"));
	on_search_done();

	// TODO: Add style support
}

void frog::on_search_done() {
	std::string path_str = entry_path.get_buffer()->get_text().raw();
	std::filesystem::path p = path_str;

	// Cleanup code that doesn't work that well
	p = p.lexically_normal();

	if (p.filename() == ".")
		p = p.parent_path();

	path_str = p.string();
	if (path_str.back() == '/' && path_str != "/")
		path_str.pop_back();

	if (path_str == previous_path)
		return;

	if (!std::filesystem::exists(path_str)) {
		entry_path.set_text(previous_path);
		return;
	}

	entry_path.set_text(path_str);
	previous_path = path_str;
	populate_files(path_str);
}

void frog::on_child_activated(Gtk::FlowBoxChild* child) {
	file_entry *entry = dynamic_cast<file_entry*>(child->get_child());
	std::string path = entry_path.get_buffer()->get_text().raw();

	if (entry->is_directory) {
		entry_path.set_text(path + "/" + entry->label.get_text());
		on_search_done();
	}
	else {
		std::string cmd = "xdg-open " + path + "/" + entry->label.get_text();
		system(cmd.c_str());
	}
}

void frog::populate_files(const std::string &path) {
	flowbox_files.remove_all();

	// TODO: Make this async
	for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(path)) {
		file_entry *f_entry = Gtk::make_managed<file_entry>(entry);
		flowbox_files.append(*f_entry);
	}
}
