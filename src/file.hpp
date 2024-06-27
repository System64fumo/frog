#pragma once
#include <gtkmm.h>
#include <filesystem>

class file_entry : public Gtk::Box {
	public:
		file_entry(const std::filesystem::directory_entry &entry);

		bool is_directory;
		Gtk::Image image;
		Gtk::Label label;
};
