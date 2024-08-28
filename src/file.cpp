#include "file.hpp"
#include "icons.hpp"
#include "xdg_dirs.hpp"

#include <iostream>
#include <gtkmm/stack.h>
#include <gtkmm/label.h>

file_entry::file_entry(const std::filesystem::directory_entry &entry) {
	path = entry.path();
	file_name = entry.path().filename().string();
	set_orientation(Gtk::Orientation::VERTICAL);

	append(image);
	image.set_pixel_size(64);
	image.set_from_icon_name("content-loading-symbolic");

	append(label);
	label.set_text(file_name);
	label.set_halign(Gtk::Align::CENTER);
	auto stack = dynamic_cast<Gtk::Stack*>(label.get_children()[0]);
	auto ulabel = dynamic_cast<Gtk::Label*>(stack->get_children()[0]);
	ulabel->set_justify(Gtk::Justification::CENTER);
	ulabel->set_ellipsize(Pango::EllipsizeMode::END);
	ulabel->set_max_width_chars(0);

	// Handle renaming
	label.signal_state_flags_changed().connect([&, entry](Gtk::StateFlags state_flags) {
		if ((int)state_flags == 24704) {
			if (label.get_text() != file_name.c_str()) {
				std::string cur_path = entry.path().parent_path();
				std::string new_name = label.get_text();
				std::filesystem::rename(path, cur_path + "/" + new_name);
			}
		}
	});

	// Figure out the correct icon
	file_size = entry.is_regular_file() ? entry.file_size() : 0;
	is_directory=entry.is_directory();

	if (is_directory) {
		if (xdg_dirs[path] != "")
			image.set_from_icon_name(xdg_dirs[path]);
		else
			image.set_from_icon_name("default-folder");
		return;
	}

	size_t last_dot_pos = file_name.rfind('.');
	if (last_dot_pos != std::string::npos) {
		std::string extension = file_name.substr(last_dot_pos + 1);
		std::string extension_icon = icon_from_extension[extension];
		if (!extension_icon.empty())
			image.set_from_icon_name(icon_from_extension[extension]);
		else
			image.set_from_icon_name("application-blank");
	}
	else {
		// TODO: Re add magic file detection for fallback
		image.set_from_icon_name("application-blank");
	}
}
