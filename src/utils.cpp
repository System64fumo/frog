#include "utils.hpp"
#include <ios>
#include <iomanip>

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
