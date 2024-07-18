#include "main.hpp"
#include "window.hpp"
#include "place.hpp"
#include "file.hpp"
#include "css.hpp"
#include "config_parser.hpp"

#include <iostream>
#include <filesystem>

frog::frog() {
	set_title("Frog");
	set_default_size(800, 400);
	set_child(box_main);
	show();

	navbar_setup();
	sidebar_setup();

	box_main.append(box_container);
	box_container.get_style_context()->add_class("file_container");
	box_container.set_hexpand(true);
	box_container.set_orientation(Gtk::Orientation::VERTICAL);

	box_container.append(entry_path);
	entry_path.get_style_context()->add_class("path_bar");
	entry_path.signal_activate().connect(sigc::mem_fun(*this, &frog::on_entry_done));

	box_container.append(scrolled_window_files);
	scrolled_window_files.set_child(flowbox_files);
	scrolled_window_files.set_vexpand(true);

	Glib::RefPtr<Gtk::GestureClick> click_gesture = Gtk::GestureClick::create();
	click_gesture->set_button(GDK_BUTTON_PRIMARY);
	click_gesture->signal_pressed().connect([&](const int &n_press, const double &x, const double &y) {
		flowbox_files.unselect_all();
	});
	scrolled_window_files.add_controller(click_gesture);

	dispatcher_files.connect(sigc::mem_fun(*this, &frog::on_dispatcher_files));
	flowbox_files.signal_child_activated().connect(sigc::mem_fun(*this, &frog::on_filebox_child_activated));
	flowbox_files.set_sort_func(sigc::mem_fun(*this, &frog::sort_func));
	flowbox_files.set_activate_on_single_click(false);
	flowbox_files.set_valign(Gtk::Align::START);
	flowbox_files.set_homogeneous(true);
	flowbox_files.set_max_children_per_line(128);
	flowbox_files.set_row_spacing(30);
	flowbox_files.set_column_spacing(30);

	context_menu_setup();

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
	button_previous.signal_clicked().connect([&]() {
		std::string new_path = back_paths.back();
		populate_files(new_path);
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
		populate_files(new_path);
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
		populate_files(parent_path.string());
	});

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
	flowbox_places.set_valign(Gtk::Align::START);
	flowbox_places.set_max_children_per_line(1);

	// Pinned items
	config_parser config(std::string(getenv("HOME")) + "/.config/sys64/frog.conf");
	std::vector<std::string> keys = config.get_keys("pinned");

	for (const auto &key : keys) {
		std::string cfg_pinned_place = config.get_value("pinned", key);
		place *place_entry = Gtk::make_managed<place>(key, cfg_pinned_place);
		flowbox_places.append(*place_entry);
	}
	
	flowbox_places.signal_child_activated().connect(sigc::mem_fun(*this, &frog::on_places_child_activated));
}

void frog::context_menu_setup() {
	// TODO: Separate this into file and folder menus.
	file_menu = Gio::Menu::create();
	auto section1 = Gio::Menu::create();
	auto section2 = Gio::Menu::create();
	auto section3 = Gio::Menu::create();

	file_menu->append_section(section1);
	file_menu->append_section(section2);
	file_menu->append_section(section3);

	auto action_group = Gio::SimpleActionGroup::create();
	insert_action_group("file", action_group);

	section1->append("Open With", "file.open");
	action_group->add_action("open", [](){
		std::cout << "Clicked: open" << std::endl;
	});
	section1->append("Rename", "file.rename");
	action_group->add_action("rename", [](){
		std::cout << "Clicked: rename" << std::endl;
	});
	section1->append("Delete", "file.delete");
	action_group->add_action("delete", [](){
		std::cout << "Clicked: delete" << std::endl;
	});

	section2->append("Cut", "file.cut");
	action_group->add_action("cut", [](){
		std::cout << "Clicked: cut" << std::endl;
	});
	section2->append("Copy", "file.copy");
	action_group->add_action("copy", [](){
		std::cout << "Clicked: copy" << std::endl;
	});
	section2->append("Paste", "file.paste");
	action_group->add_action("paste", [](){
		std::cout << "Clicked: paste" << std::endl;
	});

	section3->append("Properties", "file.properties");
	action_group->add_action("properties", [](){
		std::cout << "Clicked: properties" << std::endl;
	});
}

void frog::on_entry_done() {
	populate_files(entry_path.get_buffer()->get_text().raw());
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
		populate_files(new_path);
	}
	else {
		std::string cmd = "xdg-open \"" + path + '/' + entry->label.get_text() + '"';
		system(cmd.c_str());
	}
}

void frog::on_places_child_activated(Gtk::FlowBoxChild *child) {
	place *place_entry = dynamic_cast<place*>(child->get_child());
	next_paths.clear();
	populate_files(place_entry->file_path);
}

void frog::on_right_clicked(const int &n_press,
							const double &x,
							const double &y,
							Gtk::FlowBoxChild *flowbox_child) {

	file_entry *entry = dynamic_cast<file_entry*>(flowbox_child->get_child());

	// For now only file right click menus are supported
	if (entry->is_directory)
		return;
	else
		popovermenu_context_menu.set_menu_model(file_menu);

	// This looks horrible, I know..
	flowbox_files.select_child(*flowbox_child);
	popovermenu_context_menu.unparent();
	popovermenu_context_menu.set_parent(*flowbox_child);
	popovermenu_context_menu.popup();
}

void frog::populate_files(const std::string &path) {
	std::filesystem::path fs_path = path;

	if (fs_path.filename() == ".") {
		fs_path = fs_path.parent_path();
		entry_path.set_text(fs_path.string());
	}

	fs_path = fs_path.lexically_normal();

	std::string path_str = fs_path.string();
	if (path_str.back() == '/' && path_str != "/") {
		path_str.pop_back();
		path_str.pop_back();
	}

	if (path_str == current_path)
		return;

	if (!std::filesystem::exists(path_str)) {
		entry_path.set_text(current_path);
		return;
	}

	back_paths.push_back(current_path);
	current_path = path_str;
	button_previous.set_sensitive(back_paths.size() != 1);
	button_next.set_sensitive(next_paths.size() != 0);

	entry_path.set_text(path);
	flowbox_files.remove_all();

	if (async_task.valid()) {
		stop_flag.store(true);
		async_task.wait();
	}

	stop_flag.store(false);
	async_task = std::async(std::launch::async, [this, path]() {
		for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(path)) {
			if (stop_flag.load())
				break;

			Gtk::FlowBoxChild *fbox_child = Gtk::make_managed<Gtk::FlowBoxChild>();
			fbox_child->set_size_request(96,96);
			file_entry *f_entry = Gtk::make_managed<file_entry>(entry);
			fbox_child->set_child(*f_entry);

			Glib::RefPtr<Gtk::GestureClick> click_gesture = Gtk::GestureClick::create();
			click_gesture->set_button(GDK_BUTTON_SECONDARY);
			click_gesture->signal_pressed().connect(sigc::bind(sigc::mem_fun(*this, &frog::on_right_clicked), fbox_child));
			f_entry->add_controller(click_gesture);
			std::lock_guard<std::mutex> lock(queue_mutex);
			widget_queue.push(fbox_child);

			dispatcher_files.emit();
		}
	});

	if (watcher != nullptr)
		delete watcher;

	watcher = new directory_watcher();
	watcher->start_watching(path);
}

void frog::on_dispatcher_files() {
	while (!widget_queue.empty()) {
		auto widget = widget_queue.front();
		widget_queue.pop();
		flowbox_files.append(*widget);
	}
}
