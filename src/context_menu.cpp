#include "window.hpp"
#include "file.hpp"
#include "utils.hpp"
#include "properties_dialog.hpp"

#include <fstream>
#include <algorithm>

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
		auto f_entry = dynamic_cast<file_entry*>(flowbox_files.get_selected_children()[0]);
		f_entry->label.start_editing();
		f_entry->label.set_position(f_entry->label.get_text().length());
	});
	section1->append("Delete", "file.delete");
	action_group->add_action("delete", [&](){
		auto children = flowbox_files.get_selected_children();
		for (const auto& child : children) {
			auto file = dynamic_cast<file_entry*>(child);
			std::filesystem::remove_all(file->path);
		}
	});

	section2->append("Cut", "file.cut");
	action_group->add_action("cut", [&](){
		std::printf("Clicked: cut\n");
		Glib::RefPtr<Gdk::Clipboard> clipboard = get_clipboard();

		std::string content = "cut";
		for (const auto &child : flowbox_files.get_selected_children()) {
			auto file = dynamic_cast<file_entry*>(child);
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
			auto file = dynamic_cast<file_entry*>(child);
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
		auto f_entry = dynamic_cast<file_entry*>(flowbox_files.get_selected_children()[0]);

		auto dialog = Gtk::make_managed<properties_dialog>(f_entry);
		dialog->set_transient_for(*this);
		dialog->show();
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

				std::istringstream stream(data);
				std::string line;

				std::string operation;
				std::getline(stream, operation);

				std::vector<std::filesystem::path> file_paths;
				while (std::getline(stream, line))
					file_paths.push_back(line.substr(7));

				if (operation == "copy") {
					file_op(file_paths, current_path, 'm');
				}
				else if (operation == "cut") {
					file_op(file_paths, current_path, 't');
					// TODO: Clean clipboard after this
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
			f_entry = dynamic_cast<file_entry*>(selected[0]);
		}
		else {
			std::filesystem::directory_entry entry(current_path);
			f_entry = Gtk::make_managed<file_entry>(this, entry);
		}

		auto dialog = Gtk::make_managed<properties_dialog>(f_entry);
		dialog->set_transient_for(*this);
		dialog->show();
	});
}

properties_dialog::properties_dialog(file_entry* f_entry) {
	set_default_size(300, 400);
	box_main = get_content_area();

	// Preview
	box_main->append(box_preview);
	box_preview.set_orientation(Gtk::Orientation::VERTICAL);
	box_preview.set_margin_top(32);
	box_preview.set_margin_bottom(22);
	box_preview.append(image_preview);
	box_preview.append(label_preview);

	// Icon
	image_preview.set_pixel_size(64);
	image_preview.set_from_icon_name(f_entry->file_icon);

	// Label
	label_preview.set_text(f_entry->file_name);
	label_preview.set_wrap_mode(Pango::WrapMode::WORD_CHAR);
	label_preview.set_ellipsize(Pango::EllipsizeMode::END);
	label_preview.set_max_width_chars(10);
	label_preview.set_lines(2);



	// General
	auto card_general = create_card();
	box_main->append(*card_general);
	centerbox_type = add_property_text(card_general, "Type");
	centerbox_size = add_property_text(card_general, "Size");
	if (f_entry->is_directory)
		centerbox_content = add_property_text(card_general, "Content");

	// Time
	auto card_time = create_card();
	box_main->append(*card_time);
	centerbox_created = add_property_text(card_time, "Created");
	centerbox_modified = add_property_text(card_time, "Modified");
	centerbox_accessed = add_property_text(card_time, "Accessed");

	// Permissions

	dispatcher.connect([&, f_entry]() {
		auto label_size = dynamic_cast<Gtk::Label*>(centerbox_size->get_end_widget());
		label_size->set_text(to_human_readable(f_entry->file_size));

		auto label_type = dynamic_cast<Gtk::Label*>(centerbox_type->get_end_widget());
		if (f_entry->is_directory) {
			auto label_content = dynamic_cast<Gtk::Label*>(centerbox_content->get_end_widget());
			label_content->set_text(std::to_string(f_entry->content_count));
			label_type->set_text("Directory");
		}
		else {
			label_type->set_text("File"); // TODO: Read actual file type
		}

		// TODO: Implement time stuff
		// TODO: Implement permission stuff
	});

	// TODO: Make this smoother/better
	// TODO: Make the thread stop when the dialog is closed
	std::thread([&, f_entry]() {
		f_entry->load_metadata();
		dispatcher.emit();
	}).detach();
}

Gtk::Box* properties_dialog::create_card() {
	Gtk::Box* card = Gtk::make_managed<Gtk::Box>();
	card->set_orientation(Gtk::Orientation::VERTICAL);
	card->set_size_request(280, -1);
	card->get_style_context()->add_class("card");
	card->set_hexpand(true);
	card->set_margin(10);
	card->set_halign(Gtk::Align::CENTER);
	card->set_valign(Gtk::Align::START);
	return card;
}

Gtk::CenterBox* properties_dialog::add_property(Gtk::Box* container, Gtk::Widget* name, Gtk::Widget* value) {
	Gtk::CenterBox* centerbox = Gtk::make_managed<Gtk::CenterBox>();
	centerbox->get_style_context()->add_class("property");
	centerbox->set_start_widget(*name);
	centerbox->set_end_widget(*value);
	container->append(*centerbox);
	return centerbox;
}

Gtk::CenterBox* properties_dialog::add_property_text(Gtk::Box* container, std::string title, std::string content) {
	Gtk::CenterBox* centerbox = Gtk::make_managed<Gtk::CenterBox>();
	Gtk::Label* label_title = Gtk::make_managed<Gtk::Label>(title);
	Gtk::Label* label_content = Gtk::make_managed<Gtk::Label>(content);
	centerbox->get_style_context()->add_class("property");
	centerbox->set_start_widget(*label_title);
	centerbox->set_end_widget(*label_content);
	container->append(*centerbox);
	return centerbox;
}