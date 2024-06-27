#include "main.hpp"
#include "window.hpp"

int main(int argc, char* argv[]) {
	app = Gtk::Application::create("funky.sys64.frog");

	return app->make_window_and_run<frog>(argc, argv);
}
