#pragma once
#include <vector>
#include <string>
#include <cstdint>

class disk {
	public:
		disk(const uint64_t& size, const std::vector<std::string>& parts);

	private:
		uint64_t size_in_b;
		std::vector<std::string> partitions;
};

std::vector<disk> get_disks();
