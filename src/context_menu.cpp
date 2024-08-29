#include "window.hpp"
#include "file.hpp"

#include <fstream>

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
			std::filesystem::remove(file->path);
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
	action_group->add_action("properties", [](){
		std::printf("Clicked: properties\n");
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

	section1->append("Create New Folder", "dir.newdir");
	action_group->add_action("newdir", [&](){
		const std::filesystem::path dir_path{current_path + "/New Folder"};
		try {
			std::filesystem::create_directory(dir_path);
		}
		catch (const std::filesystem::filesystem_error& e) {
			std::fprintf(stderr, "Failed to create a new directory\n");
			std::fprintf(stderr, "Error: %s\n", e.what());
		}
	});

	section1->append("Create New File", "dir.newfile");
	action_group->add_action("newfile", [&](){
		const std::string file_path{current_path + "/New file"};
		std::ofstream file(file_path);
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
				std::printf("Data: %s\n", data.c_str());
			});
		});
	});

	section3->append("Properties", "dir.properties");
	action_group->add_action("properties", [](){
		std::printf("Clicked: properties\n");
	});
}

void frog::on_right_clicked(const int &n_press,
							const double &x,
							const double &y,
							Gtk::FlowBoxChild *flowbox_child) {

	flowbox_files.select_child(*flowbox_child);

	Gdk::Rectangle rect(flowbox_child->get_width() / 2, flowbox_child->get_height() / 2, 0, 0);
	popovermenu_context_menu.unparent();
	popovermenu_context_menu.set_parent(*flowbox_child);
	popovermenu_context_menu.set_pointing_to(rect);
	popovermenu_context_menu.set_menu_model(menu_file);
	popovermenu_context_menu.popup();
}
