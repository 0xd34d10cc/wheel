#pragma once

#include <atomic>
#include <cstddef>
#include <cassert>
#include <condition_variable>
#include <mutex>
#include <memory>


namespace wh {

namespace detail {

// circular buffer
template <typename T>
struct Buffer {
  std::unique_ptr<T[]> ptr;

  std::size_t start{0};
  std::size_t size{0};
  std::size_t capacity{0};

  Buffer() = default;
  Buffer(std::size_t cap)
      : ptr(std::make_unique<T[]>(cap)), start(0), size(0), capacity(cap) {}
  Buffer(const Buffer&) = delete;
  Buffer(Buffer&&) noexcept = default;
  Buffer& operator=(const Buffer&) = delete;
  Buffer& operator=(Buffer&&) noexcept = default;
  ~Buffer() noexcept = default;

  bool empty() const noexcept { return size == 0; }
  bool full() const noexcept { return size == capacity; }

  void push(T&& value) noexcept {
    assert(!full());

    std::size_t end = (start + size) % capacity;
    ptr[end] = std::move(value);
    size += 1;
  }

  void pop(T& value) noexcept {
    assert(!empty());

    value = std::move(ptr[start]);
    start = (start + 1) % capacity;
    size -= 1;
  }
};

template <typename T>
struct State {
  std::mutex mutex;
  std::condition_variable not_full;
  std::condition_variable not_empty;
  std::atomic<bool> receiver_alive{true};
  std::atomic<std::size_t> senders{0};
  Buffer<T> buffer;

  State(std::size_t cap) : buffer(cap) {}
};

template <typename T>
using StatePtr = std::shared_ptr<State<T>>;

template <typename T>
using WeakStatePtr = std::weak_ptr<State<T>>;

}  // namespace detail

template <typename T>
class Sender {
 public:
  Sender() noexcept = default;
  Sender(detail::StatePtr<T> state) noexcept { reset(std::move(state)); }
  Sender(const Sender& other) noexcept { reset(other.m_state); }
  Sender(Sender&&) noexcept = default;
  Sender& operator=(const Sender& other) noexcept {
    if (this == &other) {
      return *this;
    }

    reset(other.m_state);
    return *this;
  }
  Sender& operator=(Sender&& other) noexcept {
    if (this == &other) {
      return *this;
    }

    reset(std::move(other.m_state));
    return *this;
  }
  ~Sender() noexcept { reset(nullptr); }

  std::size_t send_some(T* begin, std::size_t n) const noexcept {
    std::unique_lock<std::mutex> lock(m_state->mutex);
    while (m_state->receiver_alive && m_state->buffer.full()) {
      m_state->not_full.wait(lock);
    }

    if (!m_state->receiver_alive) {
      return 0;
    }

    assert(!m_state->buffer.full());
    bool was_empty = m_state->buffer.empty();
    std::size_t sent = 0;
    while (!m_state->buffer.full() && sent != n) {
      m_state->buffer.push(std::move(*begin));
      ++begin;
      ++sent;
    }

    if (was_empty) {
      m_state->not_empty.notify_one();
    }

    return sent;
  }

  std::size_t send_all(T* begin, std::size_t n) const noexcept {
    std::size_t sent = 0;
    while (sent != n) {
      std::size_t s = send_some(begin + sent, n - sent);
      if (s == 0) {
        return sent;
      }

      sent += s;
    }

    return sent;
  }

  bool send(T&& value) const noexcept { return send_some(&value, 1) == 1; }

 private:
  void reset(detail::StatePtr<T> new_state) noexcept {
    if (m_state) {
      if (--m_state->senders == 0) {
        m_state->not_empty.notify_one();
      }
    }

    m_state = std::move(new_state);
    if (m_state) {
      ++m_state->senders;
    }
  }

  detail::StatePtr<T> m_state;
};

template <typename T>
class Receiver {
 public:
  Receiver() noexcept = default;
  Receiver(detail::StatePtr<T> state) noexcept { reset(std::move(state)); }
  Receiver(const Receiver&) = delete;
  Receiver(Receiver&&) noexcept = default;
  Receiver& operator=(const Receiver&) = delete;
  Receiver& operator=(Receiver&& other) noexcept {
    if (this == &other) {
      return *this;
    }

    reset(std::move(other.m_state));
    return *this;
  }
  ~Receiver() noexcept { reset(nullptr); }

  bool receive(T& value) noexcept {
    if (!m_buffer.empty()) {
      m_buffer.pop(value);
      return true;
    }

    std::unique_lock<std::mutex> lock(m_state->mutex);
    while (m_state->senders != 0 && m_state->buffer.empty()) {
      m_state->not_empty.wait(lock);
    }

    bool was_full = m_state->buffer.full();
    std::swap(m_buffer, m_state->buffer);

    if (was_full) {
      m_state->not_full.notify_all();
    }

    if (!m_buffer.empty()) {
      m_buffer.pop(value);
      return true;
    }

    assert(m_state->senders == 0);
    return false;
  }

 private:
  void reset(detail::StatePtr<T> state) noexcept {
    if (m_state) {
      m_state->receiver_alive = false;
      m_state->not_full.notify_all();
      m_buffer = detail::Buffer<T>();
    }

    m_state = std::move(state);
    if (m_state) {
      m_buffer = detail::Buffer<T>(m_state->buffer.capacity);
    }
  }

  detail::StatePtr<T> m_state;
  detail::Buffer<T> m_buffer;
};

template <typename T>
std::pair<Sender<T>, Receiver<T>> channel(std::size_t cap) {
  auto state = std::make_shared<detail::State<T>>(cap);
  return {Sender<T>{state}, Receiver<T>{state}};
}

}  // namespace wh