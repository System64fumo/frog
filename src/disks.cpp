#include "disk.hpp"

#include <filesystem>
#include <fstream>

// TODO: Consider adding the search function to the disk itself rather than a get_disks() function
disk::disk(const uint64_t& size, const std::vector<std::string>& parts) {
	size_in_b = size;
	partitions = parts;
}

std::vector<disk> get_disks() {
	std::vector<disk> disks;
	for (const auto& entry : std::filesystem::directory_iterator("/sys/block/")) {
		if (entry.is_directory()) {
			std::string device_name = entry.path().filename().string();
			std::ifstream size_file(entry.path().string() + "/size");

			std::uint64_t sectors = 0;
			if (size_file.is_open())
				size_file >> sectors;
			sectors = sectors * 512;

			std::printf("Block device: %s, Size: %ld Bytes\n", device_name.c_str(), sectors);

			std::vector<std::string> partitions;
			for (const auto& entry : std::filesystem::directory_iterator(entry.path().string())) {
				if (entry.is_directory()) {
					std::string partition_name = entry.path().filename().string();
					if (partition_name.find(device_name) == 0) {
						partitions.push_back(partition_name);
						std::printf("  Partition: %s\n", partition_name.c_str());
					}
				}
			}
			disk d(sectors, partitions);
			disks.push_back(d);
		}
	}
	return disks;
}
