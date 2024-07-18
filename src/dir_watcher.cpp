#include "dir_watcher.hpp"

#include <sys/inotify.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <iostream>
#include <cstring>

directory_watcher::directory_watcher() : stop_thread(false), inotify_fd(-1), event_fd(-1) {}

directory_watcher::~directory_watcher() {
	stop_watching();
}

void directory_watcher::start_watching(const std::string &directory) {
	stop_watching();

	stop_thread = false;
	event_fd = eventfd(0, EFD_NONBLOCK);
	if (event_fd < 0) {
		std::cerr << "eventfd() failed: " << std::strerror(errno) << std::endl;
		return;
	}

	watcher_thread = std::jthread([this, directory](std::stop_token stop_token) {
		this->watch_directory(directory, stop_token);
	});
}

void directory_watcher::stop_watching() {
	if (watcher_thread.joinable()) {
		stop_thread = true;
		uint64_t u = 1;
		write(event_fd, &u, sizeof(uint64_t));
		watcher_thread.request_stop();
		watcher_thread.join();
	}
	if (inotify_fd != -1) {
		close(inotify_fd);
		inotify_fd = -1;
	}
	if (event_fd != -1) {
		close(event_fd);
		event_fd = -1;
	}
}

void directory_watcher::watch_directory(const std::string &directory, std::stop_token stop_token) {
	inotify_fd = inotify_init();
	if (inotify_fd < 0) {
		std::cerr << "inotify_init() failed: " << std::strerror(errno) << std::endl;
		return;
	}

	int watch_descriptor = inotify_add_watch(inotify_fd, directory.c_str(), IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO);
	if (watch_descriptor < 0) {
		std::cerr << "inotify_add_watch() failed: " << std::strerror(errno) << std::endl;
		close(inotify_fd);
		return;
	}

	const size_t event_size = sizeof(struct inotify_event);
	const size_t buffer_size = 1024 * (event_size + 16);
	char buffer[buffer_size];

	int fds[2] = {inotify_fd, event_fd};
	while (!stop_token.stop_requested()) {
		fd_set read_fds;
		FD_ZERO(&read_fds);
		FD_SET(fds[0], &read_fds);
		FD_SET(fds[1], &read_fds);

		int max_fd = std::max(fds[0], fds[1]);
		int ret = select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr);
		if (ret < 0) {
			if (errno == EINTR) {
				continue;
			}
			std::cerr << "select() failed: " << std::strerror(errno) << std::endl;
			break;
		}

		if (FD_ISSET(event_fd, &read_fds)) {
			break;  // Stop requested
		}

		if (FD_ISSET(inotify_fd, &read_fds)) {
			int length = read(inotify_fd, buffer, buffer_size);
			if (length < 0) {
				if (errno == EINTR) {
					continue;
				}
				std::cerr << "read() failed: " << std::strerror(errno) << std::endl;
				break;
			}

			int i = 0;
			while (i < length) {
				struct inotify_event* event = (struct inotify_event*)&buffer[i];

				// TODO: Do something with this
				if (event->len) {
					if (event->mask & IN_CREATE) {
						std::cout << "File created: " << event->name << std::endl;
					} else if (event->mask & IN_DELETE) {
						std::cout << "File deleted: " << event->name << std::endl;
					} else if (event->mask & IN_MODIFY) {
						std::cout << "File modified: " << event->name << std::endl;
					} else if (event->mask & IN_MOVED_FROM) {
						std::cout << "File moved from: " << event->name << std::endl;
					} else if (event->mask & IN_MOVED_TO) {
						std::cout << "File moved to: " << event->name << std::endl;
					}
				}
				i += event_size + event->len;
			}
		}
	}

	close(inotify_fd);
	inotify_fd = -1;
}
