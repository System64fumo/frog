#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <filesystem>
#include <map>

#include <libudev.h>
#include <glibmm/dispatcher.h>
#include <giomm/dbusconnection.h>
#include <giomm/dbusproxy.h>

class disk_manager {
	public:
		disk_manager();
		~disk_manager();

		struct partition {
			std::string name;
			std::string label;
			std::string type;
			std::string mount_path;
			std::string holder;
			unsigned long total_bytes;
			unsigned long free_bytes;
			unsigned long used_bytes;
			bool should_show;
		};

		struct disk {
			std::string name;
			std::string model;
			bool removable;
			std::vector<partition> partitions;
		};

		std::vector<disk_manager::disk> disks;
		Glib::Dispatcher dispatcher_on_changed;

	private:
		struct udev* udev;
		struct udev_enumerate* enumerate;
		struct udev_list_entry* devices;

		std::map<std::string, std::vector<std::string>> fstab;
		std::map<std::string, std::string> mounts;

		void get_fstab();
		void get_mounts();
		void get_disks();

		disk create_disk(const std::string&);
		partition create_partition(const std::string&);
};
