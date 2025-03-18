#include "main.hpp"
#include "window.hpp"

int main(int argc, char* argv[]) {
	app = Gtk::Application::create(
		"funky.sys64.frog",
		Gio::Application::Flags::HANDLES_OPEN |
		Gio::Application::Flags::NON_UNIQUE);

	while (true) {
		switch (getopt(argc, argv, "dh")) {
			case 'd':
				desktop_mode = true;
				continue;

			case 'h':
			default :
				std::printf("usage:\n");
				std::printf("  frog [argument...]:\n\n");
				std::printf("arguments:\n");
				std::printf("  -d	Set desktop mode\n");
				std::printf("  -h	Show this help message\n");
				return 0;

			case -1:
				break;
			}

			break;
	}

	if (argc > 1) {
		char *path = argv[1];

		size_t len = strlen(path);
		if (len > 0 && path[len - 1] == '/') {
			path[len - 1] = '\0';
		}

		start_path = path;
	}

	app->signal_open().connect([&](const std::vector<Glib::RefPtr<Gio::File>>& files, const Glib::ustring&) {
		// For now only the last dir matters
		// TODO: Add support for opening multiple directories at once
		for (const auto& file : files)
			start_path = file->get_path();
		app->activate();
	});

	return app->make_window_and_run<frog>(0, nullptr);
}
