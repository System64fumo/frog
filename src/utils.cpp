#include "utils.hpp"

std::string to_human_readable(const uint& bytes) {
	// TODO: Consider adding support for *bit formats instead of just *byte formats
	const char* suffixes[] = {"B", "KB", "MB", "GB", "TB", "PB"};

	if (bytes == 0) {
		return "0 B";
	}

	int index = 0;
	double size = bytes;

	while (size >= 1024 && index < 8) {
		size /= 1024;
		index++;
	}

	char buffer[50];
	snprintf(buffer, sizeof(buffer), "%.2f %s", size, suffixes[index]);

	return std::string(buffer);
}
