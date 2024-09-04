#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <filesystem>
#include <map>

class disk {
	public:
		disk(const std::filesystem::path& path);

		struct partition {
			std::string name;
			std::string label;
			uint64_t size;
		};

		std::string device_name;
		uint64_t size_in_b;
		std::vector<partition> partitions;
};

std::map<std::string, std::string> get_mounts();
std::vector<disk> get_disks();
