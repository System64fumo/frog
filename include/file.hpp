#pragma once
#include <gtkmm/box.h>
#include <gtkmm/flowboxchild.h>
#include <gtkmm/entry.h>
#include <gtkmm/image.h>
#include <gtkmm/editablelabel.h>
#include <gtkmm/dragsource.h>
#include <filesystem>

class frog;

class file_entry : public Gtk::FlowBoxChild {
	public:
		file_entry(frog*, const std::filesystem::directory_entry&);
		~file_entry();
		frog* win;

		std::string path;
		bool is_directory;
		std::string file_name;
		std::string file_icon;
		int file_size;
		int content_count;
		Gtk::Box box_main;
		Gtk::Image image;
		Gtk::EditableLabel label;

		void load_thumbnail();
		void load_metadata();

	private:
		Glib::RefPtr<Gtk::DragSource> source;
		const std::filesystem::directory_entry entry;
		Glib::RefPtr<Gdk::Pixbuf> pixbuf;

		std::string extension;

		Glib::RefPtr<Gdk::Pixbuf> resize_thumbnail(const Glib::RefPtr<Gdk::Pixbuf>&);
		void setup_drop_target();
};
