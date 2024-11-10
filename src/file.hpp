#pragma once
#include <gtkmm/box.h>
#include <gtkmm/entry.h>
#include <gtkmm/image.h>
#include <gtkmm/editablelabel.h>
#include <gtkmm/dragsource.h>
#include <filesystem>

class file_entry : public Gtk::Box {
	public:
		file_entry(const std::filesystem::directory_entry&);
		~file_entry();

		std::string path;
		bool is_directory;
		std::string file_name;
		std::string file_icon;
		int file_size;
		const int icon_size = 64;
		Gtk::Image image;
		Gtk::EditableLabel label;

		void load_data();
		void load_thumbnail();

	private:
		const std::filesystem::directory_entry entry;
		Glib::RefPtr<Gtk::DragSource> source;
		Glib::RefPtr<Gdk::Pixbuf> pixbuf;

		std::string extension;

		Glib::RefPtr<Gdk::Pixbuf> resize_thumbnail(const Glib::RefPtr<Gdk::Pixbuf>&);
		void setup_drop_target();
};
