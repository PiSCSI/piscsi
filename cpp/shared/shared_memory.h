//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI for Raspberry Pi
//
// Copyright (C) 2023 akuker
//
// Reference: https://en.cppreference.com/w/cpp/thread/shared_mutex
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

class SharedMemory
{
  public:
    SharedMemory(std::string, bool is_primary=false);
    ~SharedMemory();

    inline uint32_t get() const
    {
        std::shared_lock lock(m_shared_mem->mutex_);
        return m_shared_mem->value_;
    }

    inline void set(uint32_t new_val)
    {
        std::unique_lock lock(m_shared_mem->mutex_);
        m_shared_mem->value_ = new_val;
    }

    inline void set_bit(int bit_num, int value)
    {
        std::unique_lock lock(m_shared_mem->mutex_);
        if (value) {
            // Set bit
            m_shared_mem->value_ |= (1 << bit_num);
        } else {
            // Clear bit
            m_shared_mem->value_ &= ~(1 << bit_num);
        }
    }

    inline bool is_valid(){
        return m_valid;
    }

  private:
    typedef struct lockable_data {
        mutable std::shared_mutex mutex_;
        uint32_t value_;
    } lockable_data_t;

    lockable_data_t* m_shared_mem;

    bool m_valid;
    bool m_primary;
    int m_fd_shared_mem;
    std::string m_name;
};
