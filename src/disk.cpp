#include "disk.hpp"

#include <fstream>

std::map<std::string, std::string> get_mounts() {
	std::map<std::string, std::string> mounted_partitions;
	std::ifstream mounts("/proc/mounts");
	std::string device, mount_point, rest;

	while (mounts >> device >> mount_point) {
		std::getline(mounts, rest); // Skip the rest of the line
		const std::string prefix = "/dev/";
		if (device.compare(0, prefix.size(), prefix) == 0)
			device.erase(0, prefix.size());

		mounted_partitions[device] = mount_point;
	}

	return mounted_partitions;
}

uint64_t get_size(const std::string& path) {
	std::ifstream size_file(path + "/size");
	std::uint64_t sectors = 0;
	size_file >> sectors;
	return sectors * 512;
}

std::string to_human_readable(const uint64_t& bytes) {
	const uint64_t KB = 1024;
	const uint64_t MB = KB * 1024;
	const uint64_t GB = MB * 1024;
	const uint64_t TB = GB * 1024;

	double value = static_cast<double>(bytes);
	std::string unit;

	if (bytes >= TB) {
		value /= TB;
		unit = "TB";
	} else if (bytes >= GB) {
		value /= GB;
		unit = "GB";
	} else if (bytes >= MB) {
		value /= MB;
		unit = "MB";
	} else if (bytes >= KB) {
		value /= KB;
		unit = "KB";
	} else {
		unit = "B";
	}

	std::ostringstream oss;
	oss << std::fixed << std::setprecision(2) << value << " " << unit;

	return oss.str();
}

disk::disk(const std::filesystem::path& path) {
	device_name = path.filename().string();
	//uint64_t disk_size = get_size(path.string());

	for (const auto& entry : std::filesystem::directory_iterator(path)) {
		if (entry.is_directory()) {
			partition part;
			part.name = entry.path().filename().string();

			// Get the partition size
			if (part.name.find(device_name) == 0)
				part.size = get_size(entry.path().string());

			partitions.push_back(part);
		}
	}
}