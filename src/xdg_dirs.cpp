#include "xdg_dirs.hpp"

#include <fstream>
#include <algorithm>

void get_xdg_user_dirs() {
	std::string home_dir = std::getenv("HOME");
	std::ifstream file(home_dir + "/.config/user-dirs.dirs");

	if (!file)
		return;

	xdg_dirs[home_dir] = "folder-home";

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
				key.replace(home_pos, 5, home_dir);

			// Assign appropiate icon
			std::transform(value.begin(), value.end(), value.begin(), ::tolower);
			value.erase(0, 4);
			value.erase(value.length() - 4, 4);
			value = "folder-" + value;

			xdg_dirs[key] = value;
		}
	}
}
