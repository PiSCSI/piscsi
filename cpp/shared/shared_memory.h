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
#include <sys/file.h>
#include <sys/mman.h>
#include <thread>

#include "shared/log.h"

#pragma once
class SharedMemory
{
  public:
    SharedMemory(std::string, bool is_primary = false);
    ~SharedMemory();

    inline uint32_t get() const
    {
        uint32_t value;
        flock(m_file_lock, LOCK_EX);
        value = m_shared_mem->value_;
        flock(m_file_lock, LOCK_UN);
        return value;
    }

    inline void set(uint32_t new_val)
    {
        flock(m_file_lock, LOCK_EX);
        m_shared_mem->value_ = new_val;
        flock(m_file_lock, LOCK_UN);
    }

    inline void set_bit(int bit_num, int value)
    {
        flock(m_file_lock, LOCK_EX);
        if (value) {
            // Set bit
            m_shared_mem->value_ |= (1 << bit_num);
        } else {
            // Clear bit
            m_shared_mem->value_ &= ~(1 << bit_num);
        }
        flock(m_file_lock, LOCK_UN);
    }

    inline bool is_valid()
    {
        return m_valid;
    }

  private:
    typedef struct lockable_data {
        uint32_t value_;
    } lockable_data_t;

    lockable_data_t* m_shared_mem;

    int m_file_lock;

    bool m_valid;
    bool m_primary;
    int m_fd_shared_mem;
    std::string m_name;
    std::string m_lock_name;
};
