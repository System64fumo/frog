#include "disk.hpp"
#include "utils.hpp"

#include <blkid/blkid.h>
#include <sys/statvfs.h>
#include <fstream>
#include <thread>

disk_manager::disk_manager() {
	udev = udev_new();
	enumerate = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(enumerate, "block");
	udev_enumerate_scan_devices(enumerate);
	devices = udev_enumerate_get_list_entry(enumerate);
	get_disks();

	std::thread([&]() {
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
				struct udev_device* dev = udev_monitor_receive_device(mon);
				std::string action = udev_device_get_action(dev);
				std::string devnode = udev_device_get_devnode(dev);
				std::string subsystem = udev_device_get_subsystem(dev);

				if (subsystem == "block")
					on_disk_event(devnode.substr(5), (action == "add"));

				udev_device_unref(dev);
			}
		}
	}).detach();
}

disk_manager::~disk_manager() {
	udev_enumerate_unref(enumerate);
	udev_unref(udev);
}

void disk_manager::get_disks() {
	get_fstab();
	get_mounts();

	for (const auto& entry : std::filesystem::directory_iterator("/sys/block/")) {
		std::string disk_name = entry.path().filename();

		// Ignore non disks
		if (disk_name.find("loop") != std::string::npos ||
			disk_name.find("ram") != std::string::npos)
			continue;

		disk new_disk = create_disk(disk_name);

		// Itterate over the disk's partitions
		for (const auto& partition_entry : std::filesystem::directory_iterator(entry)) {
			std::string partition_name = partition_entry.path().filename();

			// Filter out non partition dirs
			if (partition_name.find(disk_name) == std::string::npos)
				continue;

			partition new_partition = create_partition(partition_name);

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
}

void disk_manager::get_fstab() {
	std::ifstream fstab_file("/etc/fstab");

	std::string line;

	while (std::getline(fstab_file, line)) {
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

		fstab[fstab_entry[0]] = fstab_entry;
	}

	fstab_file.close();
}

void disk_manager::get_mounts() {
	std::ifstream mounts_file("/proc/mounts");
	std::string device, mount_point, rest;

	while (mounts_file >> device >> mount_point) {
		std::getline(mounts_file, rest); // Skip the rest of the line
		const std::string prefix = "/dev/";
		if (device.compare(0, prefix.size(), prefix) == 0)
			device.erase(0, prefix.size());

		mounts[device] = mount_point;
	}
}

void disk_manager::on_disk_event(const std::string& devnode, const bool& add) {
	std::string parent_dev;
	if (devnode.find("sd") == 0)
		parent_dev = devnode.substr(0, 3);
	else if (devnode.find("mmcblk") == 0)
		parent_dev = devnode.substr(0, 7);
	else if (devnode.find("nvme") == 0)
		parent_dev = devnode.substr(0, 7);

	bool is_partition = (devnode != parent_dev);

	if (is_partition) {
		// TODO: Add partition code
	}
	else {
		bool found_disk = false;
		for (std::size_t i = 0; i < disks.size();) {
			if (disks[i].name == devnode) {
				found_disk = true;
				if (!add) {
					disks.erase(disks.begin() + i);
				}
			}
			else {
				++i;
			}
		}
		if (!found_disk && add) {
			disks.push_back(create_disk(devnode));
		}
	}

	dispatcher_on_changed.emit();
}

disk_manager::disk disk_manager::create_disk(const std::string& devnode) {
	disk new_disk;
	new_disk.name = devnode;

	// Check if the disk is removable
	std::ifstream file("/sys/block/" + new_disk.name + "/removable");
	std::string line;
	if (file.is_open()) {
		std::getline(file, line);
		new_disk.removable = line == "1";
	}

	// Get disk model
	// This could be simplified
	new_disk.model = "Unknown model";

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

	return new_disk;
}

disk_manager::partition disk_manager::create_partition(const std::string& devnode) {
	partition new_partition;
	new_partition.name = devnode;
	new_partition.should_show = true;
	if (fstab.find(new_partition.name) != fstab.end()) {
		new_partition.should_show = (fstab[new_partition.name][3].find("x-gvfs-show") != std::string::npos);
	}

	// Get parent
	// TODO: This could probably be better by matching the first 2 letters and itterating over every disk
	std::string parent_dev;
	if (new_partition.name.find("sd") == 0)
		parent_dev = new_partition.name.substr(0, 3);
	else if (new_partition.name.find("mmcblk") == 0)
		parent_dev = new_partition.name.substr(0, 6);
	else if (new_partition.name.find("nvme") == 0)
		parent_dev = new_partition.name.substr(0, 4);

	// Check if the partition is encrypted
	std::string holder_path = "/sys/block/" + parent_dev + "/" + new_partition.name + "/holders";
	if (std::filesystem::exists(holder_path)) {
		for (const auto& entry : std::filesystem::directory_iterator(holder_path)) {
			if (std::filesystem::is_directory(entry)) {
				std::ifstream file(entry.path().string() + "/dm/name");
				if (file.is_open()) {
					std::stringstream buffer;
					buffer << file.rdbuf();
					std::string holder_name = buffer.str();
					if (!holder_name.empty() && holder_name.back() == '\n')
						holder_name.pop_back();
					new_partition.holder = "mapper/" + holder_name;
				}
			}
		}
	}
	
	new_partition.mount_path = mounts[new_partition.name];
	bool mounted = !new_partition.mount_path.empty();

	// Initialize with default values
	new_partition.label = "Unknown";
	new_partition.type = "Unknown Type";
	new_partition.total_bytes = 0;
	new_partition.free_bytes = 0;
	new_partition.used_bytes = 0;

	// Only try to probe if the device file exists
	std::string device_path = "/dev/" + new_partition.name;
	if (std::filesystem::exists(device_path)) {
		blkid_probe pr = blkid_new_probe_from_filename(device_path.c_str());
		if (pr != nullptr) {
			blkid_probe_enable_partitions(pr, true);
			blkid_probe_set_superblocks_flags(pr, BLKID_SUBLKS_TYPE | BLKID_SUBLKS_LABEL);
			
			if (blkid_do_fullprobe(pr) == 0) {
				const char* pl = nullptr;
				if (blkid_probe_lookup_value(pr, "LABEL", &pl, nullptr) == 0) {
					new_partition.label = (pl != nullptr) ? std::string(pl) : "Unknown";
				}

				const char* pt = nullptr;
				if (blkid_probe_lookup_value(pr, "TYPE", &pt, nullptr) == 0) {
					new_partition.type = (pt != nullptr) ? std::string(pt) : "Unknown Type";
				}
			}
			blkid_free_probe(pr);
		}
	}

	if (mounted) {
		struct statvfs stats;
		std::string mount_path_to_use = new_partition.mount_path;
		
		// Use holder's mount path if available
		if (!new_partition.holder.empty() && mounts.find(new_partition.holder) != mounts.end()) {
			mount_path_to_use = mounts[new_partition.holder];
		}
		
		if (statvfs(mount_path_to_use.c_str(), &stats) == 0) {
			new_partition.total_bytes = stats.f_blocks * stats.f_frsize;
			new_partition.free_bytes = stats.f_bfree * stats.f_frsize;
			new_partition.used_bytes = new_partition.total_bytes - new_partition.free_bytes;
		}

		if (new_partition.label == "Unknown" && !new_partition.mount_path.empty()) {
			if (new_partition.mount_path == "/")
				new_partition.label = "System";
			else
				new_partition.label = std::filesystem::path(new_partition.mount_path).filename();
		}
	}
	return new_partition;
}
