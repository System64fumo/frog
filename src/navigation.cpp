#include "window.hpp"
#include "place.hpp"

void frog::navigate_hist_previous() {
	std::string new_path = back_paths.back();
	navigate_to_dir(new_path);
	next_paths.push_back(back_paths.back());
	back_paths.pop_back();
	back_paths.pop_back();
	button_previous.set_sensitive((back_paths.size() != 1));
	button_next.set_sensitive(next_paths.size() != 0);
}

void frog::navigate_hist_forward() {
	std::string new_path = next_paths.back();
	next_paths.pop_back();
	navigate_to_dir(new_path);
	button_next.set_sensitive(next_paths.size() != 0);
}

void frog::navigate_up_dir() {
	std::filesystem::path path = current_path;
	std::filesystem::path parent_path = path.parent_path();
	navigate_to_dir(parent_path.string());
}

void frog::navigate_to_dir(const std::string &path) {
	std::filesystem::path fs_path = path;

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

	if (!std::filesystem::exists(path_str)) {
		entry_path.set_text(current_path);
		return;
	}

	back_paths.push_back(current_path);
	current_path = path_str;
	button_previous.set_sensitive(back_paths.size() != 1);
	button_next.set_sensitive(next_paths.size() != 0);

	entry_path.set_text(current_path);

	if (async_task.valid()) {
		stop_flag.store(true);
		async_task.wait();
	}

	// Cleanup
	std::queue<Gtk::FlowBoxChild*> empty;
	std::swap(widget_queue, empty);
	popovermenu_context_menu.unparent();
	flowbox_files.remove_all();

	stop_flag.store(false);
	async_task = std::async(std::launch::async, [this, path]() {
		for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(path)) {
			if (stop_flag.load())
				break;
			create_file_entry(entry);
			dispatcher_files.emit();
		}
	});

	if (watcher != nullptr) {
		if (watcher->path == path)
			return;
		else
			delete watcher;
	}

	watcher = new directory_watcher(&dispatcher_file_change);
	watcher->start_watching(path);

	// Select pinned entry from sidebar if exists
	bool path_found = false;
	for (auto child : flowbox_places.get_children()) {
		auto fbox_child = dynamic_cast<Gtk::FlowBoxChild*>(child);
		place *sidebar_place = dynamic_cast<place*>(fbox_child->get_child());

		if (!sidebar_place)
			continue;

		if (sidebar_place->file_path == path) {
			flowbox_places.select_child(*fbox_child);
			path_found = true;
			break;
		}
	}

	if (!path_found)
		flowbox_places.unselect_all();
}
