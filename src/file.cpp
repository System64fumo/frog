#include "file.hpp"

#include <iostream>
#include <magic.h>
#include <thread>

file_entry::file_entry(const std::filesystem::directory_entry &entry) {
	std::string name = entry.path().filename().string();
	int size = entry.is_regular_file() ? entry.file_size() : 0;

	set_orientation(Gtk::Orientation::VERTICAL);
	is_directory=entry.is_directory();

	append(image);
	image.set_pixel_size(64);

	if (name.size() >= 30)
		name = name.substr(0, 27) + "...";

	append(label);
	label.set_text(name);
	label.set_max_width_chars(10);
	label.set_wrap(true);
	label.set_wrap_mode(Pango::WrapMode::WORD_CHAR);

	if (is_directory)
		image.set_from_icon_name("default-folder");
	else {

		// Set placeholder image
		image.set_from_icon_name("content-loading-symbolic");

		// Figure out the correct icon
		std::thread file_thread([&, entry]() {
			// TODO: Improve mime type handling
			magic_t magic = magic_open(MAGIC_MIME_TYPE);
			magic_load(magic, nullptr);
			const char* mime_type = magic_file(magic, entry.path().c_str());
			std::string result(mime_type);
			magic_close(magic);
			std::replace(result.begin(), result.end(), '/', '-');

			// TODO: Handle previews
			// This does not always correspond to a right mimetype
			// Maybe use a manual pre defined list instead?
			image.set_from_icon_name(result);
		});

		// TODO: Detach this thread and use a callback for even faster processing
		file_thread.join();
	}
}
