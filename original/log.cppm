//
// Created by 23530 on 2026/11/26.
//
module;

#import <string_view>
#import <cstdio>
#import <atomic>
#import <thread>
#import <format>
#import <filesystem>
#import <queue>
#import <mutex>
#import <condition_variable>

export module utility:log;

namespace utility {
class log_sink {

  std::FILE* log_target = stdout;
  std::thread log_thread;
  std::queue<std::string> log_queue;
  bool keep_logging = true;

  std::mutex log_mutex;
  std::condition_variable cv;


  public:

  static log_sink& get_local_sink() {
    thread_local log_sink local_sink;
    return local_sink;
  }

  void change_target(std::string_view new_target) {
    std::FILE* new_file_target = std::fopen(new_target.data(),"w");

    if (new_file_target == nullptr) {
      return;
    }

    std::lock_guard guard(log_mutex);
    if (this->log_target != stdout) {
      std::fclose(this->log_target);
    }
    this->log_target = new_file_target;

  }

  explicit log_sink(std::string_view target_file = "") {
    {
      std::lock_guard guard(log_mutex);
      log_thread = std::thread([&] {work_thread();});
    }

    if (!target_file.empty()) {
      change_target(target_file);
    }
  }

  ~log_sink() {
    {
      std::lock_guard guard(log_mutex);
      keep_logging = false;
      cv.notify_one();
    }
    log_thread.join();
    if (log_target != stdout) {
      std::fclose(log_target);
    }
  }

  void add_message(std::string_view message) {
    std::lock_guard guard(log_mutex);
    log_queue.emplace(message);
    cv.notify_one();

  }

  void work_thread() {
    for (;;) {
      std::unique_lock lock(log_mutex);
      cv.wait(lock, [this] { return !keep_logging || !log_queue.empty(); });
      if (log_queue.empty()) {
        return;
      }
      std::fprintf(log_target, "%s", log_queue.front().data());
      log_queue.pop();
    }
  }

};

}

export namespace utility {

void set_target(std::string_view new_target) {
  log_sink::get_local_sink().change_target(new_target);
}

template <typename... Args>
void log(std::string_view message, Args&&... args) {
  log_sink::get_local_sink().add_message(std::vformat(message, std::forward<Args>(args)...));
}

}
