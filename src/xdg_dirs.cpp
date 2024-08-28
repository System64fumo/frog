#include "xdg_dirs.hpp"

#include <fstream>
#include <algorithm>

void get_xdg_user_dirs() {
	std::ifstream file(std::getenv("HOME") + std::string("/.config/user-dirs.dirs"));

	if (!file) {
		std::cerr << "Could not open the user-dirs.dirs file" << std::endl;
		return;
	}

	std::string line;
	while (std::getline(file, line)) {
		if (line.find("XDG_") == 0) {
			auto delimiter_pos = line.find('=');
			auto key = line.substr(delimiter_pos + 1);
			auto value = line.substr(0, delimiter_pos);

			// Strip the leading and trailing quotes
			if (key.front() == '"')
				key.erase(0, 1);
			if (key.back() == '"')
				key.pop_back();

			// Expand $HOME if present
			size_t home_pos = key.find("$HOME");
			if (home_pos != std::string::npos)
				key.replace(home_pos, 5, std::getenv("HOME"));

			// Assign appropiate icon
			std::transform(value.begin(), value.end(), value.begin(), ::tolower);
			value.erase(0, 4);
			value.erase(value.length() - 4, 4);
			value = "folder-" + value;

			xdg_dirs[key] = value;
		}
	}
}
