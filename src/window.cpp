#include "main.hpp"
#include "window.hpp"
#include "file.hpp"
#include "css.hpp"

#include <iostream>
#include <filesystem>

frog::frog() {
	set_title("Frog");
	set_default_size(800, 400);
	set_child(box_main);
	show();

	// TODO: Add navigation buttons
	box_main.append(box_sidebar);
	box_sidebar.set_size_request(175, -1);
	box_sidebar.set_orientation(Gtk::Orientation::VERTICAL);
	box_sidebar.append(box_navigation);

	navbar_setup();
	sidebar_setup();

	box_container.append(entry_path);
	entry_path.get_style_context()->add_class("path_bar");
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
	flowbox_files.set_sort_func(sigc::mem_fun(*this, &frog::sort_func));
	flowbox_files.set_activate_on_single_click(false);
	flowbox_files.set_valign(Gtk::Align::START);
	flowbox_files.set_homogeneous(true);
	flowbox_files.set_max_children_per_line(128);
	flowbox_files.set_row_spacing(5);
	flowbox_files.set_column_spacing(5);

	//std::filesystem::current_path().string() // TODO: Add option to use this
	entry_path.set_text(getenv("HOME"));
	on_search_done();

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
	button_previous.signal_clicked().connect([&]() {
		std::string new_path = back_paths.back();
		entry_path.set_text(new_path);
		on_search_done();
		next_paths.push_back(back_paths.back());
		back_paths.pop_back();
		back_paths.pop_back();
		button_previous.set_sensitive((back_paths.size() != 1));
		button_next.set_sensitive(next_paths.size() != 0);
	});

	box_navigation.append(button_next);
	button_next.get_style_context()->add_class("flat");
	button_next.set_icon_name("go-next-symbolic");
	button_next.set_sensitive(false);
	button_next.signal_clicked().connect([&]() {
		std::string new_path = next_paths.back();
		next_paths.pop_back();
		entry_path.set_text(new_path);
		on_search_done();
		button_next.set_sensitive(next_paths.size() != 0);
	});

	// TODO: Add checks for up button sensitivity
	box_navigation.append(button_up);
	button_up.get_style_context()->add_class("flat");
	button_up.set_icon_name("go-up-symbolic");
	button_up.set_sensitive(true);
	button_up.signal_clicked().connect([&]() {
		std::filesystem::path path = current_path;
		std::filesystem::path parent_path = path.parent_path();
		entry_path.set_text(parent_path.string());
		on_search_done();
	});

	//box_navigation.append(button_search);
	//button_search.get_style_context()->add_class("flat");
	//button_search.set_icon_name("search-symbolic");

	box_main.append(box_container);
	box_container.get_style_context()->add_class("file_container");
	box_container.set_hexpand(true);
	box_container.set_orientation(Gtk::Orientation::VERTICAL);
}

void frog::sidebar_setup() {
	box_sidebar.get_style_context()->add_class("sidebar");
	box_sidebar.append(scrolled_window_places);
	scrolled_window_places.set_child(flowbox_places);
	scrolled_window_places.set_vexpand(true);
	flowbox_places.set_valign(Gtk::Align::START);
	flowbox_places.set_max_children_per_line(1);

	// Sample items
	// TODO: Actually read pinned items and add them here
	std::vector<std::string> pinned_places = {
		"Home", "Desktop", "Documents", "Downloads"
	};

	// TODO: Add on_child_activated stuff to this flowbox too;
	// TODO: Ideally replace this with it's own class
	for (const auto& text : pinned_places) {
		Gtk::Box *box = Gtk::make_managed<Gtk::Box>();
		Gtk::Label *label = Gtk::make_managed<Gtk::Label>(text);
		Gtk::Image *image = Gtk::make_managed<Gtk::Image>();

		box->get_style_context()->add_class("place");
		box->append(*image);
		box->append(*label);
		flowbox_places.append(*box);

		label->set_justify(Gtk::Justification::LEFT);
		image->set_pixel_size(16);
		image->set_from_icon_name("folder-symbolic");
		image->set_margin_end(5);
	}
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

	if (path_str == current_path)
		return;

	if (!std::filesystem::exists(path_str)) {
		entry_path.set_text(current_path);
		return;
	}

	entry_path.set_text(path_str);
	back_paths.push_back(current_path);
	current_path = path_str;
	populate_files(path_str);
	button_previous.set_sensitive(back_paths.size() != 1);
	button_next.set_sensitive(next_paths.size() != 0);
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

void frog::on_child_activated(Gtk::FlowBoxChild* child) {
	file_entry *entry = dynamic_cast<file_entry*>(child->get_child());
	std::string path = entry_path.get_buffer()->get_text().raw();

	if (entry->is_directory) {
		std::string new_path = path + "/" + entry->label.get_text();
		entry_path.set_text(new_path);
		next_paths.clear();
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
