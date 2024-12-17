#include "disk.hpp"
#include "utils.hpp"

#include <blkid/blkid.h>
#include <sys/statvfs.h>
#include <libudev.h>
#include <fstream>
#include <thread>

disk_manager::disk_manager() {
	std::thread([&]() {
		struct udev *udev = udev_new();
		struct udev_monitor *mon = udev_monitor_new_from_netlink(udev, "udev");
		udev_monitor_filter_add_match_subsystem_devtype(mon, "block", nullptr);
		udev_monitor_enable_receiving(mon);
		int fd = udev_monitor_get_fd(mon);
		while (true) {
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(fd, &fds);
			int ret = select(fd + 1, &fds, nullptr, nullptr, nullptr);
			if (ret > 0 && FD_ISSET(fd, &fds)) {
				struct udev_device *dev = udev_monitor_receive_device(mon);
				std::string action = udev_device_get_action(dev) ? udev_device_get_action(dev) : "unknown";
				std::string devnode = udev_device_get_devnode(dev) ? udev_device_get_devnode(dev) : "unknown";
				std::string subsystem = udev_device_get_subsystem(dev) ? udev_device_get_subsystem(dev) : "unknown";

				// TODO: Do something with this
				if (subsystem == "block") {
					//std::printf("Device event\n");
				}

				udev_device_unref(dev);
			}
		}
	}).detach();
}

std::vector<disk_manager::disk> disk_manager::get_disks() {
	auto fstab = get_fstab();
	std::map<std::string, std::string> mounts = get_mounts();
	std::vector<disk> disks;

	for (const auto& entry : std::filesystem::directory_iterator("/sys/block/")) {
		std::string disk_name = entry.path().filename();

		// Ignore non disks
		if (disk_name.find("loop") != std::string::npos ||
			disk_name.find("ram") != std::string::npos)
			continue;

		disk new_disk;
		new_disk.name = disk_name;

		// Check if the disk is removable
		std::ifstream file("/sys/block/" + new_disk.name + "/removable");
		std::string line;
		if (file.is_open()) {
			std::getline(file, line);
			new_disk.removable = line == "1";
		}

		// Get disk model
		// TODO: This could be cached and used less often
		// This could also be simplified
		new_disk.model = "Unknown model";
		struct udev *udev = udev_new();
		struct udev_enumerate *enumerate = udev_enumerate_new(udev);
		udev_enumerate_add_match_subsystem(enumerate, "block");
		udev_enumerate_scan_devices(enumerate);
		struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);

		// Iterate through devices
		for (struct udev_list_entry *dev_list_entry = devices; dev_list_entry;
			dev_list_entry = udev_list_entry_get_next(dev_list_entry)) {

			const char *path = udev_list_entry_get_name(dev_list_entry);
			std::string p = path;

			if (p.substr(p.size() - new_disk.name.size()) != new_disk.name)
				continue;

			struct udev_device *dev = udev_device_new_from_syspath(udev, path);

			const char *model = udev_device_get_property_value(dev, "ID_MODEL");
			if (model == nullptr)
				continue;

			new_disk.model = model;
			udev_device_unref(dev);
		}

		udev_enumerate_unref(enumerate);
		udev_unref(udev);

		// Itterate over the disk's partitions
		for (const auto& partition_entry : std::filesystem::directory_iterator(entry)) {
			std::string partition_name = partition_entry.path().filename();

			// Filter out non partition dirs
			if (partition_name.find(disk_name))
				continue;

			partition new_partition;
			new_partition.name = partition_name;
			new_partition.mount_path = mounts[new_partition.name];
			new_partition.should_show = true;
			if (fstab.find(partition_name) != fstab.end()) {
				new_partition.should_show = (fstab[partition_name][3].find("x-gvfs-show") != std::string::npos);
			}

			// Check if the partition is encrypted
			for (const auto& entry : std::filesystem::directory_iterator("/sys/block/" + new_disk.name + "/" + new_partition.name + "/holders")) {
				if (std::filesystem::is_directory(entry)) {
					std::ifstream file(entry.path().string() + "/dm/name");
					std::stringstream buffer;
					buffer << file.rdbuf();
					new_partition.holder = "mapper/" + buffer.str().substr(0, buffer.str().size() - 1);
					new_partition.mount_path = mounts[new_partition.holder];
				}
			}

			bool mounted = !new_partition.mount_path.empty();

			blkid_probe pr = blkid_new_probe_from_filename(("/dev/" + new_partition.name).c_str());
			if (!pr)
				continue;

			blkid_probe_enable_partitions(pr, true);
			blkid_probe_set_superblocks_flags(pr, BLKID_SUBLKS_TYPE | BLKID_SUBLKS_LABEL);
			blkid_do_fullprobe(pr);

			const char* pl = nullptr;
			blkid_probe_lookup_value(pr, "LABEL", &pl, nullptr);
			new_partition.label = (pl != nullptr) ? std::string(pl) : new_disk.model;

			const char* pt = nullptr;
			blkid_probe_lookup_value(pr, "TYPE", &pt, nullptr);
			new_partition.type = (pt != nullptr) ? std::string(pt) : "Unknown Type";

			// Cleanup
			blkid_free_probe(pr);

			new_partition.used_bytes = 0;
			new_partition.free_bytes = 0;
			new_partition.used_bytes = 0;
			if (mounted) {
				struct statvfs stats;
				if (!new_partition.holder.empty())
					statvfs(mounts[new_partition.holder].c_str(), &stats);
				else
					statvfs(mounts[new_partition.name].c_str(), &stats);
				new_partition.total_bytes = stats.f_blocks * stats.f_frsize;
				new_partition.free_bytes = stats.f_bfree * stats.f_frsize;
				new_partition.used_bytes = new_partition.total_bytes - new_partition.free_bytes;
			}

			new_disk.partitions.push_back(new_partition);
		}
		disks.push_back(new_disk);
	}

	// Scan for network mounts
	for (const auto& entry : mounts) {
		if (entry.first.find(":/") != std::string::npos) {
			disk new_disk;
			new_disk.removable = false;
			new_disk.name = "Network disk";
			partition new_partition;

			size_t pos = entry.first.find(':');

			new_partition.name = entry.first.substr(0, pos);
			new_partition.should_show = true;
			if (fstab.find(new_partition.name) != fstab.end()) {
				new_partition.should_show = (fstab[new_partition.name][3].find("x-gvfs-show") != std::string::npos);
			}

			new_partition.mount_path = entry.second.c_str();
			new_partition.label = entry.second.substr(entry.second.rfind('/') + 1);

			struct statvfs stats;
			statvfs(entry.second.c_str(), &stats);
			new_partition.total_bytes = stats.f_blocks * stats.f_frsize;
			new_partition.free_bytes = stats.f_bfree * stats.f_frsize;
			new_partition.used_bytes = new_partition.total_bytes - new_partition.free_bytes;
			new_disk.partitions.push_back(new_partition);
			disks.push_back(new_disk);
		}
	}

	// Network shares (fstab)
	for (const auto& fstab_entry : fstab) {
		// Only check for network shares
		if (fstab_entry.first.find(":/") == std::string::npos)
			continue;

		// Check if the share is mounted
		if (mounts.find(fstab_entry.first) != mounts.end())
			continue;

		disk new_disk;
		new_disk.removable = false;
		new_disk.name = "Network disk";
		partition new_partition;
		new_partition.label = fstab_entry.second[1].substr(fstab_entry.second[1].rfind('/') + 1);
		new_partition.should_show = fstab_entry.second[3].find("x-gvfs-show") != std::string::npos;
		new_partition.mount_path = fstab_entry.second[1];
		new_partition.total_bytes = 1;
		new_partition.free_bytes = 0;
		new_partition.used_bytes = 0;

		new_disk.partitions.push_back(new_partition);
		disks.push_back(new_disk);
	}
	return disks;
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

std::map<std::string, std::vector<std::string>> disk_manager::get_fstab() {
	std::ifstream fstab("/etc/fstab");

	std::string line;
	std::map<std::string, std::vector<std::string>> fstab_map;

	while (std::getline(fstab, line)) {
		if (line.empty() || line[0] == '#')
			continue;

		std::istringstream stream(line);
		std::string token;

		std::vector<std::string> fstab_entry;
		while (stream >> token) {
			if (token.rfind("/dev/", 0) == 0)
				token = token.substr(5);
			else if (token.find(":/") != std::string::npos) {
				std::istringstream stream(line);
				stream >> token;
			}
			fstab_entry.push_back(token);
		}

		fstab_map[fstab_entry[0]] = fstab_entry;
	}

	fstab.close();
	return fstab_map;
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
