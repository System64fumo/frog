#include "file.hpp"

#include <iostream>

file_entry::file_entry(const std::filesystem::directory_entry &entry) {
	std::string name = entry.path().filename().string();
	int size = entry.is_regular_file() ? entry.file_size() : 0;

	set_orientation(Gtk::Orientation::VERTICAL);
	is_directory=entry.is_directory();
	set_size_request(96,96);

	append(image);
	image.set_pixel_size(64);

	append(label);
	label.set_text(name);
	label.set_max_width_chars(10);
	label.set_wrap(true);
	label.set_wrap_mode(Pango::WrapMode::WORD_CHAR);

	if (is_directory)
		image.set_from_icon_name("default-folder");
	else
		image.set_from_icon_name("application-blank");
	// TODO: Handle different file mime types
	// TODO: Handle previews
	// TODO: Handle mounts
}
