#include "disk.hpp"

#include <blkid/blkid.h>
#include <fstream>

disk::disk(const std::filesystem::path& path) {
	device_name = path.filename().string();
	//uint64_t disk_size = get_size(path.string());

	for (const auto& entry : std::filesystem::directory_iterator(path)) {
		if (entry.is_directory()) {
			partition part;
			part.name = entry.path().filename().string();

			// Get the partition size
			if (part.name.find(device_name) == 0) {
				std::string part_path = "/dev/" + part.name;
				blkid_probe pr = blkid_new_probe_from_filename(part_path.c_str());
				blkid_probe_enable_partitions(pr, true);
				blkid_probe_set_superblocks_flags(pr,
					BLKID_SUBLKS_USAGE | BLKID_SUBLKS_TYPE |
					BLKID_SUBLKS_MAGIC | BLKID_SUBLKS_LABEL);

				blkid_do_fullprobe(pr);
				const char* label = nullptr;
				blkid_probe_lookup_value(pr, "LABEL", &label, nullptr);
				if (label)
					part.label = label;
				part.size = get_size(entry.path().string());
				blkid_free_probe(pr);
			}

			partitions.push_back(part);
		}
	}
}

uint64_t disk::get_size(const std::string& path) {
	std::ifstream size_file(path + "/size");
	std::uint64_t sectors = 0;
	size_file >> sectors;
	return sectors * 512;
}

std::string disk::to_human_readable(const uint64_t& bytes) {
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

void disk_manager::get_disks() {
	std::map<std::string, std::string> mounts = get_mounts();

	for (const auto& entry : std::filesystem::directory_iterator("/sys/block/")) {
		std::string disk_name = entry.path().filename();

		// Ignore non disks
		if (disk_name.find("loop") != std::string::npos ||
			disk_name.find("ram") != std::string::npos ||
			disk_name.find("dm-") != std::string::npos)
			continue;

		std::printf("Disk: %s\n", disk_name.c_str());
		for (const auto& partition_entry : std::filesystem::directory_iterator(entry)) {
			std::string partition_name = partition_entry.path().filename();

			// Filter out non partition dirs
			if (partition_name.find(disk_name))
				continue;

			blkid_probe pr = blkid_new_probe_from_filename(("/dev/" + partition_name).c_str());
			if (!pr)
				continue;
			blkid_probe_enable_partitions(pr, true);
			blkid_probe_set_superblocks_flags(pr,
				BLKID_SUBLKS_USAGE | BLKID_SUBLKS_TYPE |
				BLKID_SUBLKS_MAGIC | BLKID_SUBLKS_LABEL);
			blkid_do_fullprobe(pr);

			const char* type = nullptr;
			blkid_probe_lookup_value(pr, "TYPE", &type, nullptr);

			std::printf("  Partition: %s\n", partition_name.c_str());
			std::printf("  Type: %s\n", type);
			std::printf("  Mounted on: %s\n", mounts[partition_name].c_str());

			// Cleanup
			blkid_free_probe(pr);
		}
	}
}

void disk_manager::get_disks_udisks() {
	proxy = Gio::DBus::Proxy::create_sync(
		Gio::DBus::Connection::get_sync(Gio::DBus::BusType::SYSTEM),
		"org.freedesktop.UDisks2",
		"/org/freedesktop/UDisks2",
		"org.freedesktop.DBus.ObjectManager");

	std::vector<Glib::VariantBase> args_vector;
	auto args = Glib::VariantContainerBase::create_tuple(args_vector);
	auto result = proxy->call_sync("GetManagedObjects", args);
	auto result_cb = Glib::VariantBase::cast_dynamic<Glib::VariantContainerBase>(result);
	auto result_base = result_cb.get_child(0);
	extract_data(result_base);
}

void disk_manager::extract_data(const Glib::VariantBase& variant_base) {
	auto variant = Glib::VariantBase::cast_dynamic<Glib::Variant<std::map<Glib::DBusObjectPathString, std::map<Glib::ustring, std::map<Glib::ustring, Glib::VariantBase>>>>>(variant_base);
	auto data_map = variant.get();

	for (const auto& [object_path, interface_map] : data_map) {
		std::string device_path;
		std::string mount_path;
		std::string type;

		for (const auto& [interface_name, property_map] : interface_map) {
				if (interface_name == "org.freedesktop.UDisks2.Block") {
					for (const auto& [property_name, value] : property_map) {
						if (property_name == "Device")
							device_path = value.print();
						else if (property_name == "IdType")
							type = value.print();
					}
				}

				if (interface_name == "org.freedesktop.UDisks2.Filesystem") {
					for (const auto& [property_name, value] : property_map) {
						if (property_name == "MountPoints")
							mount_path = value.print();
					}
				}
			}

			// Filter out unmountable devices
			if (!device_path.empty() && type != "''") {
				std::printf("Device: %s\n", device_path.c_str());
				std::printf("Mounted on: %s\n", mount_path.c_str());
				std::printf("Type: %s\n", type.c_str());
			}
	}
}

std::map<std::string, std::string> disk_manager::get_mounts() {
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
