#pragma once
#include "disk.hpp"

#include <gtkmm/box.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/progressbar.h>

class place : public Gtk::Box {
	public:
		place(std::string label_str, std::string path_str, std::string custom_icon = "", const std::optional<disk_manager::partition>& part = std::nullopt);

		std::string file_path;
		bool is_disk;
		disk_manager::partition part;

	private:
		Gtk::Image image;
		Gtk::Label label;
		Gtk::ProgressBar progressbar_size;
};
