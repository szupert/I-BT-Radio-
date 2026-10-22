#pragma once
#include <atomic>
#include <cstdint>

// Bind only from the designated service entry point, never from loop().
// Foreign tasks return immediately; only the owner can touch active_.
class WebServiceOwner {
 public:
  bool bind(void* task) {
    if (!task) return false;
    void* empty = nullptr;
    return owner_.compare_exchange_strong(empty, task, std::memory_order_release,
                                         std::memory_order_relaxed) || empty == task;
  }
  bool enter(void* task) {
    if (!task || owner_.load(std::memory_order_acquire) != task) {
      refused_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    if (active_) {
      recursive_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    active_ = true;
    return true;
  }
  void leave() { active_ = false; } // owner only, paired by Scope
  uint32_t refused() const { return refused_.load(std::memory_order_relaxed); }
  uint32_t recursive() const { return recursive_.load(std::memory_order_relaxed); }
  class Scope {
   public:
    Scope(WebServiceOwner& gate, void* task) : gate_(gate), entered_(gate.enter(task)) {}
    ~Scope() { if (entered_) gate_.leave(); }
    explicit operator bool() const { return entered_; }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
   private:
    WebServiceOwner& gate_;
    bool entered_;
  };
 private:
  std::atomic<void*> owner_{nullptr};
  std::atomic<uint32_t> refused_{0}, recursive_{0};
  bool active_ = false;
};
