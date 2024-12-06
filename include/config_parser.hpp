#pragma once
#include <string>
#include <vector>
#include <unordered_map>

// INI parser
class config_parser {
	public:
		config_parser(const std::string &filename);
		std::string get_value(const std::string &section, const std::string &key);
		std::vector<std::string> get_keys(const std::string &section);

	private:
		std::unordered_map<std::string, std::unordered_map<std::string, std::string>> data;
		std::string trim(const std::string &str);
};
