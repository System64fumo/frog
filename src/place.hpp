#pragma once
#include <gtkmm/box.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/progressbar.h>

class place : public Gtk::Box {
	public:
		place(std::string label_str, std::string path_str, std::string custom_icon = "", double fraction = 0);

		std::string file_path;

	private:
		Gtk::Image image;
		Gtk::Label label;
		Gtk::ProgressBar progressbar_size;
};
