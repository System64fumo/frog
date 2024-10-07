#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <filesystem>
#include <map>

#include <giomm/dbusconnection.h>
#include <giomm/dbusproxy.h>

class disk {
	public:
		disk(const std::filesystem::path& path);

		struct partition {
			std::string name;
			std::string label;
			uint64_t size;
		};

		std::string device_name;
		uint64_t size_in_b;
		std::vector<partition> partitions;

		static std::map<std::string, std::string> get_mounts();

	private:
		std::string to_human_readable(const uint64_t& bytes);
		uint64_t get_size(const std::string& path);
};

class disk_manager {
	public:
		void get_disks();

	private:
		Glib::RefPtr<Gio::DBus::Proxy> proxy;

		void extract_data(const Glib::VariantBase& variant_base);
};
