#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <filesystem>
#include <map>

class disk {
	public:
		disk(const std::filesystem::path& path);

		std::string device_name;
		uint64_t size_in_b;
		std::vector<std::string> partitions;
};

std::map<std::string, std::string> get_mounts();
std::vector<disk> get_disks();
