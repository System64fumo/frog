#include "window.hpp"
#include "place.hpp"
#include "file.hpp"
#include "dir_watcher.hpp"

#include <gtkmm/adjustment.h>

void frog::navigate_hist_previous() {
	navigate_to_dir(back_paths.back());
	next_paths.push_back(back_paths.back());
	back_paths.pop_back();
	back_paths.pop_back();
	button_previous.set_sensitive((back_paths.size() != 1));
	button_next.set_sensitive(next_paths.size() != 0);
}

void frog::navigate_hist_forward() {
	navigate_to_dir(next_paths.back());
	next_paths.pop_back();
	button_next.set_sensitive(next_paths.size() != 0);
}

void frog::navigate_up_dir() {
	navigate_to_dir(current_path.parent_path());
}

void frog::navigate_to_dir(std::filesystem::path fs_path) {
	if (fs_path.filename() == ".") {
		fs_path = fs_path.parent_path();
		entry_path.set_text(fs_path.string());
	}

	fs_path = fs_path.lexically_normal();

	std::string path_str = fs_path.string();
	if (path_str.back() == '/' && path_str != "/") {
		path_str.pop_back();
		path_str.pop_back();
	}

	if (path_str == current_path)
		return;

	std::filesystem::directory_entry entry(path_str);

	if (!entry.exists()) {
		entry_path.set_text(current_path.string());
		return;
	}
	// TODO: This should also work for non current directory files
	else if (!entry.is_directory()) {
		for (auto* child : flowbox_files.get_children()) {
			auto fbox_child = dynamic_cast<Gtk::FlowBoxChild*>(child);
			auto* custom_widget = dynamic_cast<file_entry*>(fbox_child->get_child());

			if (custom_widget->path == path_str) {
				flowbox_files.select_child(*fbox_child);
				auto vadjustment = scrolled_window_files.get_vadjustment();
				vadjustment->set_value(fbox_child->get_allocation().get_y());
				entry_path.set_text(current_path.string());
				return;
			}
		}
	}

	back_paths.push_back(current_path);
	current_path = path_str;
	button_previous.set_sensitive(back_paths.size() != 1);
	button_next.set_sensitive(next_paths.size() != 0);

	entry_path.set_text(current_path.string());

	if (async_task.valid()) {
		stop_flag.store(true);
		async_task.wait();
	}

	// Cleanup
	popovermenu_context_menu.unparent();
	flowbox_files.remove_all();


	// File loading is split into 3 phases
	// This makes the UI responsive at the cost of my sanity (Especially on network mounts or slow disks)
	// Thank me later..
	stop_flag.store(false);
	async_task = std::async(std::launch::async, [&, fs_path]() {
		// Phase 1: Get all files in current dir
		generate_entry_autocomplete(fs_path.string());
		for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(fs_path)) {
			if (stop_flag.load())
				break;

			create_file_entry(entry, false);
		}
		auto children = flowbox_files.get_children();

		// TODO: Re enable this..
		// Phase 2: Load data
		/*for (auto child : children) {
			if (stop_flag.load())
				break;
			auto fbox_child = dynamic_cast<Gtk::FlowBoxChild*>(child);
			auto f_entry = dynamic_cast<file_entry*>(fbox_child->get_child());
			f_entry->load_data();
		}*/

		// Phase 3: Load thumbnails
		for (auto child : children) {
			if (stop_flag.load())
				break;
			auto fbox_child = dynamic_cast<Gtk::FlowBoxChild*>(child);
			auto f_entry = dynamic_cast<file_entry*>(fbox_child->get_child());
			f_entry->load_thumbnail();
		}
	});

	if (watcher != nullptr) {
		if (watcher->path == current_path)
			return;
		else
			delete watcher;
	}

	watcher = new directory_watcher(dispatcher_file_change);
	watcher->start_watching(current_path);

	// Select pinned entry from sidebar if exists
	bool path_found = false;
	for (auto child : flowbox_places.get_children()) {
		auto fbox_child = dynamic_cast<Gtk::FlowBoxChild*>(child);
		place *sidebar_place = dynamic_cast<place*>(fbox_child->get_child());

		if (!sidebar_place)
			continue;

		if (sidebar_place->file_path == current_path) {
			flowbox_places.select_child(*fbox_child);
			path_found = true;
			break;
		}
	}

	if (!path_found)
		flowbox_places.unselect_all();
}

void frog::generate_entry_autocomplete(const std::string& path) {
	entry_completion = Gtk::EntryCompletion::create();
	completion_model = Gtk::ListStore::create(columns);
	entry_completion->set_model(completion_model);
	entry_completion->set_text_column(columns.name);
	entry_path.set_completion(entry_completion);
	for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(path)) {
		auto row = *(completion_model->append());
		row[columns.name] = entry.path().string();
	}
}
