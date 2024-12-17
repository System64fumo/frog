#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <filesystem>
#include <map>

#include <giomm/dbusconnection.h>
#include <giomm/dbusproxy.h>

class disk_manager {
	public:
		disk_manager();

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

		std::vector<disk> get_disks();
		void get_disks_udisks();

	private:
		Glib::RefPtr<Gio::DBus::Proxy> proxy;

		std::map<std::string, std::vector<std::string>> get_fstab();
		std::map<std::string, std::string> get_mounts();
		void extract_data(const Glib::VariantBase& variant_base);
};
