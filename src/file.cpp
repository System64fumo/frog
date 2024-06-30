#include "file.hpp"

#include <iostream>
#include <magic.h>

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
	else {
		// TODO: Maybe run this in another thread too?
		// TODO: Improve mime type handling
		magic_t magic = magic_open(MAGIC_MIME_TYPE);
		magic_load(magic, nullptr);
		const char* mime_type = magic_file(magic, entry.path().c_str());
		std::string result(mime_type);
		magic_close(magic);
		std::replace(result.begin(), result.end(), '/', '-');
		// This does not always correspond to a right mimetype
		// Maybe use a manual pre defined list instead?

		image.set_from_icon_name(result);
	}
	// TODO: Handle previews
	// TODO: Handle mounts
}
