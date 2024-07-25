#pragma once
#include <gtkmm.h>
#include <filesystem>

class file_entry : public Gtk::Box {
	public:
		file_entry(const std::filesystem::directory_entry &entry);

		bool is_directory;
		Gtk::Image image;
		Gtk::Label label;

	private:
		std::string file_name;
		int file_size;
		std::string file_icon;
};
