#pragma once
#include <gtkmm.h>

class place : public Gtk::Box {
	public:
		place(std::string label_str, std::string path_str);

		std::string file_path;

	private:
		Gtk::Image image;
		Gtk::Label label;
};
