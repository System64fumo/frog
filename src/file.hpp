#pragma once
#include <gtkmm/box.h>
#include <gtkmm/entry.h>
#include <gtkmm/image.h>
#include <gtkmm/editablelabel.h>
#include <filesystem>

class file_entry : public Gtk::Box {
	public:
		file_entry(const std::filesystem::directory_entry &entry);

		std::string path;
		bool is_directory;
		Gtk::Image image;
		Gtk::EditableLabel label;

		void load_thumbnail();

	private:
		Glib::RefPtr<Gdk::Pixbuf> pixbuf;

		int icon_size = 64;
		std::string file_name;
		int file_size;
		std::string file_icon;
		std::string extension;

		Glib::RefPtr<Gdk::Pixbuf> resize_thumbnail(Glib::RefPtr<Gdk::Pixbuf> pixbuf);
		void setup_drop_target();
};
