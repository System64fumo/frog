#include "file.hpp"

#include <iostream>
#include <magic.h>
#include <thread>

file_entry::file_entry(const std::filesystem::directory_entry &entry) {
	file_name = entry.path().filename().string();
	set_orientation(Gtk::Orientation::VERTICAL);

	append(image);
	image.set_pixel_size(64);
	image.set_from_icon_name("content-loading-symbolic");

	append(label);
	label.set_text(file_name);
	label.set_justify(Gtk::Justification::CENTER);
	label.set_ellipsize(Pango::EllipsizeMode::END);
	label.set_max_width_chars(0);
	dispatcher.connect(sigc::mem_fun(*this, &file_entry::on_dispatcher));

	// Figure out the correct icon
	std::thread file_thread([&, entry]() {
		file_size = entry.is_regular_file() ? entry.file_size() : 0;
		is_directory=entry.is_directory();

		if (is_directory) {
			file_icon = "default-folder";
			dispatcher.emit();
			return;
		}

		// TODO: Improve mime type handling
		magic_t magic = magic_open(MAGIC_MIME_TYPE);
		magic_load(magic, nullptr);
		const char* mime_type = magic_file(magic, entry.path().c_str());
		file_icon = std::string(mime_type);
		magic_close(magic);
		std::replace(file_icon.begin(), file_icon.end(), '/', '-');

		// TODO: Handle previews
		// This does not always correspond to a right mimetype
		// Maybe use a manual pre defined list instead?
		dispatcher.emit();
	});

	// TODO: Detach this thread and use a callback for even faster processing
	file_thread.join();
}

void file_entry::on_dispatcher() {
	image.set_from_icon_name(file_icon);
}
