#pragma once
#include <gtkmm/window.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/flowbox.h>
#include <gtkmm/revealer.h>
#include <glibmm/dispatcher.h>
#include <gtkmm/popovermenu.h>
#include <gtkmm/overlay.h>
#include <gtkmm/entry.h>
#include <gtkmm/liststore.h>
#include <gtkmm/gestureclick.h>
#include <giomm/menu.h>
#include <giomm/simpleactiongroup.h>
#include <filesystem>
#include <atomic>
#include <future>
#include <mutex>
#include <queue>

#include "dir_watcher.hpp"
#include "xdg_dirs.hpp"
#include "file.hpp"

class frog : public Gtk::Window {
	public:
		frog();

	private:
		// TODO: This is starting to get rather long here.
		// Consider moving some parts of this to separate files.
		std::filesystem::path current_path;
		std::vector<std::filesystem::path> back_paths;
		std::vector<std::filesystem::path> next_paths;

		std::atomic<bool> stop_flag;
		std::future<void> async_task;
		std::queue<Gtk::FlowBoxChild*> widget_queue;
		std::mutex queue_mutex;
		bool sidebar_should_hide = true;

		directory_watcher *watcher = nullptr;

		Gtk::Overlay overlay_main;
		Gtk::Box box_overlay;
		Gtk::Box box_main;
		Gtk::Revealer revealer_sidebar;
		Gtk::Box box_sidebar;
		Gtk::Box box_top;

		Gtk::Box box_navigation;
		Gtk::Button button_previous;
		Gtk::Button button_next;
		Gtk::Button button_up;
		Gtk::Button button_search;
		Gtk::Button button_expand;

		Gtk::ScrolledWindow scrolled_window_places;
		Gtk::FlowBox flowbox_places;

		Gtk::Entry entry_path;
		Glib::RefPtr<Gtk::EntryCompletion> entry_completion;
		Glib::RefPtr<Gtk::ListStore> completion_model;
		struct ModelColumns : public Gtk::TreeModel::ColumnRecord {
			ModelColumns() { add(name); }
			Gtk::TreeModelColumn<Glib::ustring> name;
		};
		ModelColumns columns;

		Gtk::ScrolledWindow scrolled_window_files;
		Gtk::FlowBox flowbox_files;
		Glib::Dispatcher dispatcher_files;
		Glib::Dispatcher dispatcher_file_change;
		std::vector<Gtk::FlowBoxChild*> file_widgets;
		Gtk::Button button_dummy;

		Glib::RefPtr<Gio::Menu> menu_file;
		Glib::RefPtr<Gio::Menu> menu_dir;
		Gtk::PopoverMenu popovermenu_context_menu;

		void on_entry_done();
		void on_entry_changed();
		void generate_entry_autocomplete(const std::string& path);

		int sort_func(Gtk::FlowBoxChild *child1, Gtk::FlowBoxChild *child2);
		void on_dispatcher_files();
		void on_dispatcher_file_change();
		void on_filebox_child_activated(Gtk::FlowBoxChild* child);
		void on_places_child_activated(Gtk::FlowBoxChild* child);
		void on_right_clicked(const int &n_press, const double &x, const double &y, Gtk::FlowBoxChild *flowbox_child);
		bool on_key_press(const guint &keyval, const guint &keycode, const Gdk::ModifierType &state);
		void switch_layout();

		void navbar_setup();
		void sidebar_setup();
		void menu_file_setup();
		void menu_dir_setup();
		void create_properties_dialog(file_entry*);
		void create_file_entry(const std::filesystem::directory_entry &entry);

		void navigate_hist_previous();
		void navigate_hist_forward();
		void navigate_up_dir();
		void navigate_to_dir(std::filesystem::path);

	protected:
		void snapshot_vfunc(const Glib::RefPtr<Gtk::Snapshot>&);
};
