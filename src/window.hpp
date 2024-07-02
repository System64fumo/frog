#pragma once
#include <gtkmm.h>
#include <atomic>
#include <future>

class frog : public Gtk::Window {
	public:
		frog();

	private:
		std::string current_path;
		std::vector<std::string> back_paths;
		std::vector<std::string> next_paths;

		std::atomic<bool> stop_flag;
		std::future<void> async_task;

		Gtk::Box box_main;
		Gtk::Box box_sidebar;
		Gtk::Box box_container;

		Gtk::Box box_navigation;
		Gtk::Button button_previous;
		Gtk::Button button_next;
		Gtk::Button button_up;
		Gtk::Button button_search;

		Gtk::ScrolledWindow scrolled_window_places;
		Gtk::FlowBox flowbox_places;

		Gtk::Entry entry_path;
		Gtk::ScrolledWindow scrolled_window_files;
		Gtk::FlowBox flowbox_files;

		Gtk::PopoverMenu popovermenu_context_menu;

		void on_entry_done();
		int sort_func(Gtk::FlowBoxChild *child1, Gtk::FlowBoxChild *child2);
		void on_child_activated(Gtk::FlowBoxChild* child);
		void on_right_clicked(const int &n_press, const double &x, const double &y, Gtk::FlowBoxChild *flowbox_child);
		void navbar_setup();
		void sidebar_setup();
		void context_menu_setup();
		void populate_files(const std::string &path);
};
