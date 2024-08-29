#include "config_parser.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>

config_parser::config_parser(const std::string &filename) {
	std::ifstream file(filename);
	std::string line;
	std::string current_section;

	if (file.is_open()) {
		while (std::getline(file, line)) {
			line = trim(line);

			if (line.empty() || line[0] == ';' || line[0] == '#')
				continue;

			else if (line[0] == '[' && line[line.size() - 1] == ']')
				current_section = line.substr(1, line.size() - 2);

			else {
				size_t delimiter_pos = line.find('=');
				if (delimiter_pos != std::string::npos) {
					std::string key = trim(line.substr(0, delimiter_pos));
					std::string value = trim(line.substr(delimiter_pos + 1));
					data[current_section][key] = value;
				}
			}
		}
		file.close();
	}
}

std::string config_parser::get_value(const std::string &section, const std::string &key) {
	auto section_iter = data.find(section);
	if (section_iter == data.end())
		return "empty";

	auto key_iter = section_iter->second.find(key);
	if (key_iter != section_iter->second.end())
		return key_iter->second;

	return "empty";
}

std::string config_parser::trim(const std::string &str) {
	size_t first = str.find_first_not_of(' ');
	if (std::string::npos == first)
		return str;

	size_t last = str.find_last_not_of(' ');
	return str.substr(first, (last - first + 1));
}

std::vector<std::string> config_parser::get_keys(const std::string &section) {
	std::vector<std::string> keys;
	auto section_iter = data.find(section);

	if (section_iter == data.end())
		return keys;

	for (const auto &pair : section_iter->second)
		keys.push_back(pair.first);

	std::reverse(keys.begin(), keys.end());

	return keys;
}
