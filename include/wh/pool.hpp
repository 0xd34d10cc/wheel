#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>

namespace wh {

template <typename T>
class Pool;

template <typename T>
class PoolPtr {
 public:
  PoolPtr() : m_ptr(nullptr), m_pool(nullptr) {}

  PoolPtr(T* p, Pool<T>* pool) : m_ptr(p), m_pool(pool) {}

  PoolPtr(PoolPtr&& other) : m_ptr(other.m_ptr), m_pool(other.m_pool) {
    other.m_ptr = nullptr;
    other.m_pool = nullptr;
  }

  PoolPtr& operator=(PoolPtr&& other) noexcept {
    if (this == &other) {
      return *this;
    }

    reset();
    std::swap(m_ptr, other.m_ptr);
    std::swap(m_pool, other.m_pool);
    return *this;
  }

  ~PoolPtr() { reset(); }

  void reset() noexcept {
    if (m_ptr) {
      m_pool->put(m_ptr);
      m_ptr = nullptr;
      m_pool = nullptr;
    }
  }

  T* get() const { return m_ptr; }
  T& operator*() const { return *m_ptr; }
  T* operator->() const { return m_ptr; }

  explicit operator bool() const { return m_ptr != nullptr; }

 private:
  T* m_ptr;
  Pool<T>* m_pool;
};

template <typename T>
class Pool {
 public:
  union Slot {
    T value;
    Slot* next;  // pointer to the next free slot

    ~Slot() {}
  };

  Pool() noexcept = default;
  Pool(char* memory, std::size_t size_in_bytes) noexcept
      : m_slots(reinterpret_cast<Slot*>(memory)),
        m_size(size_in_bytes / sizeof(Slot)),
        m_free(nullptr) {
    assert(this->m_size != 0);
    for (std::size_t i = 0; i < this->m_size - 1; ++i) {
      this->m_slots[i].next = &this->m_slots[i + 1];
    }

    this->m_slots[this->m_size - 1].next = nullptr;
    this->m_free = &this->m_slots[0];
  }

  Pool(const Pool&) = delete;
  Pool(Pool&& other) noexcept : Pool() { this->swap(other); }
  Pool& operator=(const Pool&) = delete;
  Pool& operator=(Pool&& other) noexcept {
    if (this == &other) {
      return *this;
    }

    Pool tmp(std::move(*this));
    this->swap(other);
    return *this;
  }

  ~Pool() noexcept {
    assert(this->m_used == 0);
  }

  void swap(Pool& other) noexcept {
    std::swap(m_slots, other.m_slots);
    std::swap(m_size, other.m_size);
    std::swap(m_free, other.m_free);
  }

  std::size_t slots_taken() const noexcept { return m_used; }
  std::size_t slots_free() const noexcept { return m_size - m_used; }

  // Get RAII handle to initialized object in the pool
  template <typename... Args>
  PoolPtr<T> get(Args&&... args) {
    auto* storage = this->alloc();
    if (!storage) {
      return PoolPtr<T>();
    }

    auto* ptr = std::construct_at(storage, std::forward<Args>(args)...);
    return PoolPtr<T>(ptr, this);
  }

  // Put initialized object at |p| back in the pool
  void put(T* p) {
    std::destroy_at(p);
    this->free(p);
  }

 private:
  // Allocate uninitialized memory for object
  T* alloc() {
    // NOTE: this can be atomic
    auto* object = this->m_free;
    if (object != nullptr) {
      m_free = object->next;
      ++this->m_used;
    }

    return &object->value;
  }

  // Release uninitialized memory for object
  void free(T* object) {
    // NOTE: this can be atomic
    auto* slot = (Slot*)object;
    assert(slot >= this->m_slots);
    assert(slot < this->m_slots + this->m_size);
    slot->next = this->m_free;
    this->m_free = slot;
    --this->m_used;
  }

  Slot* m_slots{nullptr};  // pointer to the start of slots array
  std::size_t m_size{0};   // total number of slots
  Slot* m_free{nullptr};   // pointer to the head of free list

#if defined(_DEBUG)
  std::size_t m_used{0};
#endif
};

}  // namespace wh