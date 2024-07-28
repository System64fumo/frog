#pragma once
#include <gtkmm/window.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/flowbox.h>
#include <glibmm/dispatcher.h>
#include <gtkmm/popovermenu.h>
#include <gtkmm/entry.h>
#include <gtkmm/gestureclick.h>
#include <giomm/menu.h>
#include <giomm/simpleactiongroup.h>
#include <atomic>
#include <future>
#include <mutex>
#include <queue>

#include "dir_watcher.hpp"

class frog : public Gtk::Window {
	public:
		frog();

	private:
		// TODO: This is starting to get rather long here.
		// Consider moving some parts of this to separate files.
		std::string current_path;
		std::vector<std::string> back_paths;
		std::vector<std::string> next_paths;

		std::atomic<bool> stop_flag;
		std::future<void> async_task;
		std::queue<Gtk::FlowBoxChild*> widget_queue;
		std::mutex queue_mutex;

		directory_watcher *watcher = nullptr;

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
		Glib::Dispatcher dispatcher_files;
		Glib::Dispatcher dispatcher_file_change;
		std::vector<Gtk::FlowBoxChild*> file_widgets;

		Glib::RefPtr<Gio::Menu> file_menu;
		Gtk::PopoverMenu popovermenu_context_menu;

		void on_entry_done();
		int sort_func(Gtk::FlowBoxChild *child1, Gtk::FlowBoxChild *child2);
		void on_dispatcher_files();
		void on_dispatcher_file_change();
		void on_filebox_child_activated(Gtk::FlowBoxChild* child);
		void on_places_child_activated(Gtk::FlowBoxChild* child);
		void on_right_clicked(const int &n_press, const double &x, const double &y, Gtk::FlowBoxChild *flowbox_child);
		void navbar_setup();
		void sidebar_setup();
		void context_menu_setup();
		void populate_files(const std::string &path);
};
