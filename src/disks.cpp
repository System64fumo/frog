#include "disk.hpp"

#include <fstream>

disk::disk(const std::filesystem::path& path) {
	std::ifstream size_file(path.string() + "/size");
	device_name = path.filename().string();

	std::uint64_t sectors = 0;
	size_file >> sectors;
	sectors = sectors * 512;

	std::printf("Block device: %s, Size: %ld Bytes\n", device_name.c_str(), sectors);

	for (const auto& entry : std::filesystem::directory_iterator(path)) {
		if (entry.is_directory()) {
			std::string partition_name = entry.path().filename().string();
			if (partition_name.find(device_name) == 0) {
				partitions.push_back(partition_name);
				std::printf("  Partition: %s\n", partition_name.c_str());
			}
		}
	}
}

// TODO: Put this back in the sidebar section
std::vector<disk> get_disks() {
	std::vector<disk> disks;
	for (const auto& entry : std::filesystem::directory_iterator("/sys/block/")) {
		if (!entry.is_directory())
			continue;

		disk d(entry.path());
		disks.push_back(d);
	}
	return disks;
}
