#include "window.hpp"
#include "file.hpp"
#include "utils.hpp"

#include <fstream>
#include <algorithm>
#include <gtkmm/dialog.h>
#include <gtkmm/label.h>
#include <gtkmm/icontheme.h>
#include <gtkmm/centerbox.h>

void frog::menu_file_setup() {
	menu_file = Gio::Menu::create();
	auto section1 = Gio::Menu::create();
	auto section2 = Gio::Menu::create();
	auto section3 = Gio::Menu::create();

	menu_file->append_section(section1);
	menu_file->append_section(section2);
	menu_file->append_section(section3);

	auto action_group = Gio::SimpleActionGroup::create();
	insert_action_group("file", action_group);

	section1->append("Open With", "file.open");
	action_group->add_action("open", [](){
		std::printf("Clicked: open\n");
	});
	section1->append("Rename", "file.rename");
	action_group->add_action("rename", [&](){
		std::printf("Clicked: rename\n");
		auto selected = flowbox_files.get_selected_children()[0];
		auto f_entry = dynamic_cast<file_entry*>(selected->get_child());
		f_entry->label.start_editing();
		f_entry->label.set_position(f_entry->label.get_text().length());
	});
	section1->append("Delete", "file.delete");
	action_group->add_action("delete", [&](){
		auto children = flowbox_files.get_selected_children();
		for (const auto& child : children) {
			auto file = dynamic_cast<file_entry*>(child->get_child());
			std::filesystem::remove_all(file->path);
		}
	});

	section2->append("Cut", "file.cut");
	action_group->add_action("cut", [&](){
		std::printf("Clicked: cut\n");
		Glib::RefPtr<Gdk::Clipboard> clipboard = get_clipboard();

		std::string content = "cut";
		for (const auto &child : flowbox_files.get_selected_children()) {
			auto file = dynamic_cast<file_entry*>(child->get_child());
			content += "\nfile://" + file->path;
		}

		Glib::ustring mime_type = "x-special/gnome-copied-files";
		auto bytes = Glib::Bytes::create(content.data(), content.size());
		auto contentprovider = Gdk::ContentProvider::create(mime_type, bytes);
		clipboard->set_content(contentprovider);
	});
	section2->append("Copy", "file.copy");
	action_group->add_action("copy", [&](){
		std::printf("Clicked: copy\n");
		Glib::RefPtr<Gdk::Clipboard> clipboard = get_clipboard();

		std::string content = "copy";
		for (const auto &child : flowbox_files.get_selected_children()) {
			auto file = dynamic_cast<file_entry*>(child->get_child());
			content += "\nfile://" + file->path;
		}

		Glib::ustring mime_type = "x-special/gnome-copied-files";
		auto bytes = Glib::Bytes::create(content.data(), content.size());
		auto contentprovider = Gdk::ContentProvider::create(mime_type, bytes);
		clipboard->set_content(contentprovider);
	});

	section3->append("Properties", "file.properties");
	action_group->add_action("properties", [&](){
		std::printf("Clicked: properties\n");
		auto selected = flowbox_files.get_selected_children()[0];
		auto f_entry = dynamic_cast<file_entry*>(selected->get_child());
		create_properties_dialog(f_entry);
	});
}

void frog::menu_dir_setup() {
	menu_dir = Gio::Menu::create();
	auto section1 = Gio::Menu::create();
	auto section2 = Gio::Menu::create();
	auto section3 = Gio::Menu::create();

	auto action_group = Gio::SimpleActionGroup::create();
	insert_action_group("dir", action_group);

	menu_dir->append_section(section1);
	menu_dir->append_section(section2);
	menu_dir->append_section(section3);

	// TODO: Make all actions async
	section1->append("Create New Folder", "dir.newdir");
	action_group->add_action("newdir", [&](){
		std::string path = current_path.string() + "/New Folder";
		std::string desired_name = path;
		int suffix = 0;

		while (std::filesystem::exists(desired_name)) {
			suffix++;
			desired_name = path + "-" + std::to_string(suffix);
		}

		try {
			std::filesystem::create_directory(desired_name);
		}
		catch (const std::filesystem::filesystem_error& e) {
			std::fprintf(stderr, "Failed to create a new directory\n");
			std::fprintf(stderr, "Error: %s\n", e.what());
		}
	});

	section1->append("Create New File", "dir.newfile");
	action_group->add_action("newfile", [&](){
		const std::string file_path{current_path.string() + "/New file"};

		std::string desired_name = file_path;
		int suffix = 0;

		while (std::filesystem::exists(desired_name)) {
			suffix++;
			desired_name = file_path + "-" + std::to_string(suffix);
		}

		std::ofstream file(desired_name);
		if (!file) {
			std::fprintf(stderr, "Failed to create a new file\n");
		}
	});

	section2->append("Paste", "dir.paste");
	action_group->add_action("paste", [&](){
		std::printf("Clicked: paste\n");
		Glib::RefPtr<Gdk::Clipboard> clipboard = get_clipboard();
		std::vector<Glib::ustring> mime_types = {"x-special/gnome-copied-files"};
		clipboard->read_async(mime_types, Glib::PRIORITY_DEFAULT, [&, clipboard](Glib::RefPtr<Gio::AsyncResult>& result){
			Glib::ustring mime;
			Glib::RefPtr<Gio::InputStream> data = clipboard->read_finish(result, mime);

			// TODO: Figure out the size automatically
			gsize count = 1024;

			data->read_bytes_async(count, [&, data](Glib::RefPtr<Gio::AsyncResult>& result){
				auto bytes = data->read_bytes_finish(result);
				gsize bytes_size = bytes->get_size();
				auto byte_data = bytes->get_data(bytes_size);
				std::string data = std::string(reinterpret_cast<const char*>(byte_data), bytes_size);
				data = decode_url(data);
				std::printf("Data: %s\n", data.c_str());

				std::istringstream stream(data);
				std::string line;

				std::string operation;
				std::getline(stream, operation);

				std::vector<std::filesystem::path> file_paths;
				while (std::getline(stream, line))
					file_paths.push_back(line.substr(7));

				std::printf("Operation: %s\n", operation.c_str());
				if (operation == "copy") {
					file_op_copy(file_paths);
				}
				else if (operation == "cut") {
					for (const auto& path : file_paths) {
						// TODO: Run an optional validation check here to ensure the file was copied properly
						std::printf("Moving: %s to %s\n", path.string().c_str(), current_path.c_str());
						std::filesystem::rename(path, current_path / path.filename());
					}
				}

			});
		});
	});

	section3->append("Properties", "dir.properties");
	action_group->add_action("properties", [&](){
		std::printf("Clicked: properties\n");
		auto selected = flowbox_files.get_selected_children();
		file_entry* f_entry;
		if (selected.size() > 0) {
			f_entry = dynamic_cast<file_entry*>(selected[0]->get_child());
		}
		else {
			std::filesystem::directory_entry entry(current_path);
			f_entry = Gtk::make_managed<file_entry>(entry);
		}
		create_properties_dialog(f_entry);
	});
}

void frog::on_right_clicked(const int &n_press,
							const double &x,
							const double &y,
							Gtk::FlowBoxChild *flowbox_child) {

	// TODO: Handle multi selection right clicks
	// Check if the child is in the selection
	auto children = flowbox_files.get_selected_children();
	if (std::find(children.begin(), children.end(), flowbox_child) == children.end())
		flowbox_files.unselect_all();

	flowbox_files.select_child(*flowbox_child);

	Gdk::Rectangle rect(flowbox_child->get_width() / 2, flowbox_child->get_height() / 2, 0, 0);
	popovermenu_context_menu.unparent();
	popovermenu_context_menu.set_parent(*flowbox_child);
	popovermenu_context_menu.set_pointing_to(rect);
	popovermenu_context_menu.set_menu_model(menu_file);
	popovermenu_context_menu.popup();
}

void frog::create_properties_dialog(file_entry* f_entry) {
	// TODO: Lazy load metadata
	f_entry->load_metadata();

	Gtk::Dialog* dialog = Gtk::make_managed<Gtk::Dialog>();
	Gtk::Box* box_content = dialog->get_content_area();
	dialog->set_transient_for(*this);
	dialog->set_default_size(300, 400);

	// Preview
	Gtk::Box* box_preview = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
	Gtk::Label* label_file_name = Gtk::make_managed<Gtk::Label>(f_entry->file_name);
	label_file_name->set_wrap_mode(Pango::WrapMode::WORD_CHAR);
	label_file_name->set_ellipsize(Pango::EllipsizeMode::END);
	label_file_name->set_max_width_chars(10);
	label_file_name->set_lines(2);

	// Icon
	Gtk::Image* image_icon = Gtk::make_managed<Gtk::Image>();
	image_icon->set_pixel_size(64);
	auto icon_theme = Gtk::IconTheme::get_for_display(Gdk::Display::get_default());
	auto icon_info = icon_theme->lookup_icon(f_entry->file_icon, f_entry->icon_size);
	auto file = icon_info->get_file();
	auto icon = Gdk::Texture::create_from_file(file);
	image_icon->set(icon);

	box_preview->set_margin_top(32);
	box_preview->set_margin_bottom(32);
	box_preview->append(*image_icon);
	box_preview->append(*label_file_name);
	box_content->append(*box_preview);

	Gtk::Box* box_details = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
	box_details->set_size_request(280, -1);
	box_details->get_style_context()->add_class("card");
	box_details->set_hexpand(true);
	box_details->set_halign(Gtk::Align::CENTER);
	box_details->set_valign(Gtk::Align::START);

	// TODO: Add size conversion (Bytes, KBytes, MBytes, GBytes, Ect..)
	Gtk::CenterBox* centerbox_row_size = Gtk::make_managed<Gtk::CenterBox>();
	centerbox_row_size->get_style_context()->add_class("property");
	Gtk::Label* label_size_title = Gtk::make_managed<Gtk::Label>("Size");
	Gtk::Label* label_size_content = Gtk::make_managed<Gtk::Label>(to_human_readable(f_entry->file_size));
	
	centerbox_row_size->set_start_widget(*label_size_title);
	centerbox_row_size->set_end_widget(*label_size_content);

	box_details->append(*centerbox_row_size);

	box_content->append(*box_details);
	dialog->show();
}
