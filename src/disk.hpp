#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <filesystem>

class disk {
	public:
		disk(const std::filesystem::path& path);

	private:
		std::string device_name;
		uint64_t size_in_b;
		std::vector<std::string> partitions;
};

std::vector<disk> get_disks();
