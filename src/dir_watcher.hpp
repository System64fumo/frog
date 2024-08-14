#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <glibmm/dispatcher.h>
#include <queue>

class directory_watcher {
	public:
		directory_watcher(Glib::Dispatcher *dispatcher);
		~directory_watcher();

		void start_watching(const std::string &directory);

		std::queue<std::string> event_type;
		std::queue<std::string> event_name;
		std::mutex queue_mutex;
		std::string path;

	private:
		void watch_directory(const std::string &directory, std::stop_token stop_token);
		void stop_watching();

		std::jthread watcher_thread;
		std::atomic<bool> stop_thread;
		std::condition_variable cv;
		std::mutex mtx;
		int inotify_fd;
		int event_fd;

		Glib::Dispatcher *dispatcher = nullptr;
};
