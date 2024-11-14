#include "window.hpp"
#include <fstream>

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

void frog::file_op_copy(const std::vector<std::filesystem::path>& file_paths) {
	for (const auto& path : file_paths) {
		std::printf("Copying: %s to %s\n", path.string().c_str(), current_path.c_str());

		std::string desired_name = current_path.string() +  "/" + path.filename().string();
		int dot_pos = desired_name.find_last_of('.');
		std::string name = desired_name;
		std::string extension;
		if (dot_pos != -1) {
			name = desired_name.substr(0, dot_pos);
			extension = desired_name.substr(dot_pos);
		}

		// TODO: This should only run if the copy is from the same directory to the same directory
		// Otherwise prompt the user for what action should be done with existing files
		int suffix = 0;
		while (std::filesystem::exists(desired_name)) {
			suffix++;
			desired_name = name + "-" + std::to_string(suffix) + extension;
		}

		std::ofstream file(desired_name);
		if (!file) {
			std::fprintf(stderr, "Failed to create a new file\n");
		}

		std::filesystem::copy_file(path, desired_name, std::filesystem::copy_options::overwrite_existing);
	}
}
