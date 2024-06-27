#pragma once
#include <gtkmm.h>

class frog : public Gtk::Window {
	public:
		frog();

	private:
		std::string previous_path;

		Gtk::Box box_main;
		Gtk::Box box_sidebar;
		Gtk::Box box_container;
		Gtk::Entry entry_path;
		Gtk::ScrolledWindow scrolled_window_files;
		Gtk::FlowBox flowbox_files;

		void on_search_done();
		void on_child_activated(Gtk::FlowBoxChild* child);
		void populate_files(const std::string &path);
};
