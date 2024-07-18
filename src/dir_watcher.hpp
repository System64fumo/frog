#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <condition_variable>

class directory_watcher {
	public:
		directory_watcher();
		~directory_watcher();

		void start_watching(const std::string &directory);

	private:
		void watch_directory(const std::string &directory, std::stop_token stop_token);
		void stop_watching();

		std::jthread watcher_thread;
		std::atomic<bool> stop_thread;
		std::condition_variable cv;
		std::mutex mtx;
		int inotify_fd;
		int event_fd;
};
