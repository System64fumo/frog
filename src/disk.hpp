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
		struct partition {
			std::string name;
			std::string label;
			std::string type;
			std::string mount_path;
			unsigned long total_bytes;
			unsigned long free_bytes;
			unsigned long used_bytes;
		};

		struct disk {
			std::string device;
			std::vector<partition> partitions;
		};

		void get_disks();
		void get_disks_udisks();

	private:
		Glib::RefPtr<Gio::DBus::Proxy> proxy;

		std::map<std::string, std::string> get_mounts();
		void extract_data(const Glib::VariantBase& variant_base);
};
