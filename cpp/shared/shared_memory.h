//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2023 akuker
// Portions of this file Copyright (C) 2018 Intel Corporation
//    (MIT licensed)
//    https://github.com/nodejs/node/blob/main/src/large_pages/node_large_page.h
//
//---------------------------------------------------------------------------

#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <semaphore.h>
#include <shared_mutex>
#include <string>
#include <sys/mman.h>
#include <thread>

// template <class T> class SharedMemory
class SharedMemory
{
  public:
    enum RoleEnum { Controller, User };
    SharedMemory() = default;
    // SharedMemory(RoleEnum, std::string);
   SharedMemory(std::string);

    // static shared_ptr<SharedMemory<T>> Create(RoleEnum);

    ~SharedMemory();

    // // inline bool operator==(T* rhs) const
    // inline bool operator==(uint32_t* rhs) const
    // {
    //     return mem_ == (void*)rhs;
    // }
    // // inline T* mem() const
    // inline uint32_t* mem() const
    // {
    //     return (uint32_t*)mem_;
    // }
    // // inline T read() const
    // inline uint32_t read() const
    // {
    //     return *mem();
    // }
    // SharedMemory(const SharedMemory&)    = delete;
    // SharedMemory(SharedMemory&&)         = delete;
    // void operator=(const SharedMemory&)  = delete;
    // void operator=(const SharedMemory&&) = delete;

    // inline bool IsValid()
    // {
    //     return ((mem_ != nullptr) && (mem_ != MAP_FAILED));
    // }

    // void teardown();

    // Multiple threads/readers can read the counter's value at the same time.
    //   T get() const {
    uint32_t get() const{
        std::shared_lock lock(shared_mem->mutex_);
        return shared_mem->value_;
    }

    // Only one thread/writer can increment/write the counter's value.
    //   void set(T new_val) {
    void set(uint32_t new_val) {
        std::unique_lock lock(shared_mem->mutex_);
        shared_mem->value_ = new_val;
    }

    void set_bit(int bit_num, int value)
    {
        std::unique_lock lock(shared_mem->mutex_);
        if (value) {
            // Set bit
            shared_mem->value_ |= (1 << bit_num);
        } else {
            // Clear bit
            shared_mem->value_ &= ~(1 << bit_num);
        }
    }

    // Only one thread/writer can reset/write the counter's value.
    void reset()
    {
        std::unique_lock lock(shared_mem->mutex_);
        shared_mem->value_ = 0;
    }

  private:
    typedef struct lockable_data {
        mutable std::shared_mutex mutex_;
        // T value_;
        uint32_t value_;
    } lockable_data_t;
    // typedef lockable_data_t struct lockable_data_struct;

    // explicit SharedMemory()
    // {
    // }

    lockable_data_t* shared_mem;

    // SharedMemory::RoleEnum m_role;
    size_t m_size = 0;
    void* m_mem   = nullptr;
    // void* mem_;
    sem_t* m_mutex_sem;
    int m_fd_shared_mem;
    std::string m_name;
};
