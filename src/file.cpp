#include "file.hpp"
#include "icons.hpp"
#include "xdg_dirs.hpp"

#include <gtkmm/dragsource.h>
#include <gtkmm/stack.h>
#include <gtkmm/label.h>
#include <gtkmm/text.h>
#include <giomm/file.h>
#include <gtkmm/icontheme.h>

file_entry::file_entry(const std::filesystem::directory_entry &entry) {
	path = entry.path();
	file_name = entry.path().filename().string();
	set_orientation(Gtk::Orientation::VERTICAL);

	append(image);
	image.set_pixel_size(icon_size);
	image.set_from_icon_name("content-loading-symbolic");

	append(label);
	label.set_text(file_name);
	label.set_halign(Gtk::Align::CENTER);
	auto stack = dynamic_cast<Gtk::Stack*>(label.get_children()[0]);
	auto ulabel = dynamic_cast<Gtk::Label*>(stack->get_children()[0]);
	auto text = dynamic_cast<Gtk::Text*>(stack->get_children()[1]);
	stack->set_hhomogeneous(false);
	ulabel->set_justify(Gtk::Justification::CENTER);
	ulabel->set_ellipsize(Pango::EllipsizeMode::END);
	ulabel->set_max_width_chars(10);
	text->set_propagate_text_width(true);
	text->set_max_width_chars(10);

	// Handle renaming
	label.property_editing().signal_changed().connect([&, entry]() {
		if (label.get_text() != file_name.c_str()) {
			std::string cur_path = entry.path().parent_path();
			std::string new_name = label.get_text();
			std::filesystem::rename(path, cur_path + "/" + new_name);
		}
	});

	// Figure out the correct icon
	file_size = entry.is_regular_file() ? entry.file_size() : 0;
	is_directory=entry.is_directory();

	// Set up drag and drop
	auto source = Gtk::DragSource::create();
	source->set_actions(Gdk::DragAction::COPY);

	source->signal_prepare().connect([&](const double &x, const double &y) {
		// This is probably not the right way to do this?
		// It copies strings which is fine but we need to copy files
		Glib::Value<Glib::ustring> ustring_value;
		ustring_value.init(ustring_value.value_type());
		ustring_value.set(path);
		return Gdk::ContentProvider::create(ustring_value);
	}, false);
	add_controller(source);

	// Figure out what icon to use
	if (is_directory) {
		if (xdg_dirs[path] != "")
			file_icon = xdg_dirs[path];
		else
			file_icon = "default-folder";
		
	}
	else {
		size_t last_dot_pos = file_name.rfind('.');
		if (last_dot_pos != std::string::npos) {
			std::string extension = file_name.substr(last_dot_pos + 1);
			std::string extension_icon = icon_from_extension[extension];
			if (!extension_icon.empty())
				file_icon = icon_from_extension[extension];
			else
				file_icon = "application-blank";
		}
		else {
			// TODO: Re add magic file detection for fallback
			file_icon = "application-blank";
		}
	}

	// Load icons
	auto icon_theme = Gtk::IconTheme::get_for_display(Gdk::Display::get_default());
	auto icon_info = icon_theme->lookup_icon(file_icon, icon_size);
	auto file = icon_info->get_file();
	auto icon = Gdk::Texture::create_from_file(file);

	image.set(icon);
	source->set_icon(icon, icon_size / 2, icon_size / 2);
}
