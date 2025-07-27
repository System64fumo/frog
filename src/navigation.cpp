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
			auto* custom_widget = dynamic_cast<file_entry*>(child);

			if (custom_widget->path == path_str) {
				flowbox_files.select_child(*custom_widget);
				auto vadjustment = scrolled_window_files.get_vadjustment();
				vadjustment->set_value(custom_widget->get_allocation().get_y());
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
	popovermenu_file_entry_menu.unparent();
	flowbox_files.remove_all();


	// File loading is split into 3 phases
	// This makes the UI responsive at the cost of my sanity (Especially on network mounts or slow disks)
	// Thank me later..
	stop_flag.store(false);
	async_task = std::async(std::launch::async, [&, fs_path]() {
		// Phase 1: Get all files in current dir and sort them
		generate_entry_autocomplete(fs_path.string());
		
		// Collect all entries first
		std::vector<std::filesystem::directory_entry> entries;
		for (const auto& entry : std::filesystem::directory_iterator(fs_path)) {
			if (stop_flag.load()) break;
			entries.push_back(entry);
		}
		
		// Sort entries according to the specified order
		std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
			bool a_is_dir = a.is_directory();
			bool b_is_dir = b.is_directory();
			bool a_is_hidden = a.path().filename().string()[0] == '.';
			bool b_is_hidden = b.path().filename().string()[0] == '.';
			
			// Directories come before files
			if (a_is_dir != b_is_dir) return a_is_dir > b_is_dir;
			
			// If both are directories or both are files
			if (a_is_dir) {
				// For directories, non-hidden come before hidden
				if (a_is_hidden != b_is_hidden) return a_is_hidden < b_is_hidden;
			} else {
				// For files, non-hidden come before hidden
				if (a_is_hidden != b_is_hidden) return a_is_hidden < b_is_hidden;
			}
			
			// If same type and same visibility, sort alphabetically
			return a.path().filename() < b.path().filename();
		});
		
		// Process sorted entries
		for (const auto& entry : entries) {
			if (stop_flag.load()) break;
			loading_entries_mutex.lock();
			Glib::MainContext::get_default()->invoke([&, entry]() {
				file_entry *f_entry = Gtk::make_managed<file_entry>(this, entry);
				flowbox_files.append(*f_entry);
				loading_entries_mutex.unlock();
				return false;
			});
		}
		
		auto children = flowbox_files.get_children();

		// Phase 3: Load thumbnails
		for (auto child : children) {
			if (stop_flag.load()) break;
			auto f_entry = dynamic_cast<file_entry*>(child);

			// TODO: This causes massive slowdowns
			// On the bright side it no longer leaks memory
			loading_entries_mutex.lock();
			Glib::MainContext::get_default()->invoke([&, f_entry]() {
				f_entry->load_thumbnail();
				loading_entries_mutex.unlock();
				return false;
			});
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
