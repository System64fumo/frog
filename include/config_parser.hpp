#pragma once
#include <string>
#include <vector>
#include <unordered_map>

// INI parser
class config_parser {
	public:
		config_parser(const std::string &filename);
		std::vector<std::string> get_keys(const std::string &section);
		std::unordered_map<std::string, std::unordered_map<std::string, std::string>> data;

	private:
		std::string trim(const std::string &str);
};
