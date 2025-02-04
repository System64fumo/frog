#include "window.hpp"
#include "utils.hpp"
#include <fstream>
#include <set>

std::string frog::decode_url(const std::string& encoded) {
	std::string decoded;
	size_t length = encoded.length();
	for (size_t i = 0; i < length; ++i) {
		if (encoded[i] == '%' && i + 2 < length) {
			std::string hexCode = encoded.substr(i + 1, 2);
			char decodedChar = static_cast<char>(std::stoi(hexCode, nullptr, 16));
			decoded += decodedChar;
			i += 2;
		}
		else if (encoded[i] == '+') {
			decoded += ' ';
		}
		else {
			decoded += encoded[i];
		}
	}
	return decoded;
}

uint calculate_size(const std::filesystem::path& path, std::map<std::filesystem::path, uint>& file_sizes) {
	uint size = 0;

	if (std::filesystem::is_directory(path)) {
		for (const auto& entry : std::filesystem::recursive_directory_iterator(path, std::filesystem::directory_options::skip_permission_denied)) {
			if (std::filesystem::is_regular_file(entry.path())) {
				uint file_size = std::filesystem::file_size(entry.path());
				file_sizes[entry.path()] = file_size;
				size += file_size;
			}
		}
	}
	else if (std::filesystem::is_regular_file(path)) {
		uint file_size = std::filesystem::file_size(path);
		file_sizes[path] = file_size;
		size += file_size;
	}

	return size;
}

void frog::file_op(const std::vector<std::filesystem::path>& source, std::filesystem::path destination, const char& action) {
	std::set<std::filesystem::path> conflicts;

	// TODO: Run this in another thread
	// TODO: Also scan for permissions
	// TODO: Consider adding a progress indicator by getting the total size of-
	// -the transfer before running the actions.

	// Scan for conflicts
	for (const auto& src : source) {
		if (!std::filesystem::exists(src)) {
			std::fprintf(stderr, "Source path does not exist: %s\n", src.c_str());
			continue;
		}

		std::filesystem::path dest_path = destination / src.filename();

		if (std::filesystem::exists(dest_path)) {
			conflicts.insert(dest_path);
		}
	}

	// TODO: Prompt the user for a resolution to the conflict
	// Print conflicts
	if (!conflicts.empty()) {
		std::printf("Conflicts detected:\n");
		for (const auto& conflict : conflicts) {
			std::printf(" - %s\n", conflict.c_str());
			return;
		}
	}
	else {
		std::printf("No conflicts detected.\n");
	}

	// Cancel
	if (action == 'c') {
		return;
	}

	if (!std::filesystem::exists(destination)) {
		std::filesystem::create_directories(destination);
	}


	// Calculate total size of transfer
	uint total_size = 0;
	std::map<std::filesystem::path, uint> file_sizes;
	for (const auto& src : source) {
		total_size += calculate_size(src, file_sizes);
	}
	std::printf("Total transfer size: %s\n", to_human_readable(total_size).c_str());

	// TODO: Delete action should also run here (Share async and permission check code)
	// Perform actions
	for (const auto& src : source) {
		std::filesystem::path dest_path = destination / src.filename();
		std::printf("Transfering: %s which is %s\n", src.c_str(), to_human_readable(file_sizes[src]).c_str());

		// Merge
		if (action == 'm') {
			std::filesystem::copy(src, dest_path, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
		}

		// Duplicate
		else if (action == 'd') {
			int iteration = 1;
			std::filesystem::path new_dest_path = dest_path;
			while (std::filesystem::exists(new_dest_path)) {
				new_dest_path = destination / (src.stem().string() + "_" + std::to_string(iteration++) + src.extension().string());
			}

			if (std::filesystem::is_directory(src)) {
				std::filesystem::create_directories(new_dest_path);
				for (const auto& entry : std::filesystem::recursive_directory_iterator(src, std::filesystem::directory_options::skip_permission_denied)) {
					std::filesystem::path sub_new_dest_path = new_dest_path / std::filesystem::relative(entry.path(), src);
					if (std::filesystem::is_directory(entry.path())) {
						std::filesystem::create_directories(sub_new_dest_path);
					}
					else {
						std::filesystem::copy(entry.path(), sub_new_dest_path);
					}
				}
			}
			else {
				std::filesystem::copy(src, new_dest_path);
			}
		}

		// TODO: Check if the source and destination paths are on the same filesystem
		// This helps unnecessarily copying content.

		// Move (Same filesystem)
		else if (action == 'v') {
			try {
				std::filesystem::rename(src, dest_path);
			}
			catch (const std::filesystem::filesystem_error& e) {
				std::fprintf(stderr, "Error moving %s to %s: %s\n", src.c_str(), dest_path.c_str(), e.what());
			}
		}

		// Move (Copy then delete)
		else if (action == 't') {
			std::filesystem::copy(src, dest_path, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
			std::filesystem::remove_all(src);
		}
	}
}
