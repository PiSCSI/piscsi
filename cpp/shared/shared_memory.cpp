//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI for Raspberry Pi
//
// Copyright (C) 2023 akuker
//
//---------------------------------------------------------------------------
#include "shared_memory.h"
#include "shared/log.h"
#include <fcntl.h>
#include <sys/stat.h>

SharedMemory::SharedMemory(std::string region_name, bool is_primary)
    : m_valid(true), m_primary(is_primary), m_name(region_name)
{
    LOGINFO("%s Opening shared memory %s", __PRETTY_FUNCTION__, region_name.c_str())
    // Get shared memory
    int mode  = S_IRWXU | S_IRWXG;
    int oflag = O_RDWR | O_TRUNC;
    if (is_primary) {
        oflag |= O_CREAT;
    }
    if ((m_fd_shared_mem = shm_open(region_name.c_str(), oflag, mode)) == -1) {
        LOGERROR("Unable to open shared memory %s.  Is scsisim already running?", region_name.c_str());
        m_valid = false;
        return;
    }
    LOGTRACE("%s Successfully created shared memory %s", __PRETTY_FUNCTION__, region_name.c_str())

    // Extend the shared memory, since its default size is zero
    if (ftruncate(m_fd_shared_mem, sizeof(lockable_data_t)) == -1) {
        LOGERROR("Unable to expand shared memory");
        m_valid = false;
        shm_unlink(region_name.c_str());
        return;
    }
    LOGINFO("%s Shared memory region expanded to %d bytes", __PRETTY_FUNCTION__, static_cast<int>(sizeof(uint32_t)))

    m_shared_mem =
        (lockable_data_t*)mmap(NULL, sizeof(lockable_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, m_fd_shared_mem, 0);
    if (m_shared_mem == MAP_FAILED) {
        LOGERROR("Unabled to map shared memory");
        m_valid = false;
        shm_unlink(region_name.c_str());
        return;
    }
    LOGINFO("%s Shared memory region successfully memory mapped", __PRETTY_FUNCTION__)

    if (m_primary) {
        oflag = O_CREAT;
    } else {
        oflag = 0;
    }

    m_lock_name = m_name + "_lock";
    LOGINFO("%s Open file lock name: %s", __PRETTY_FUNCTION__, m_lock_name.c_str())
    m_file_lock = open(m_lock_name.c_str(), oflag);

    if (m_file_lock == -1) {
        LOGWARN("%s Unable to open the file lock (%s). Is scsisim running? Are you running as root?",
                __PRETTY_FUNCTION__, m_lock_name.c_str())
        m_valid = false;
    }
}

// Note: The normal logging functions can not be used here. The logger objects
// may have been freed by the time we get to this point
SharedMemory::~SharedMemory()
{
    // printf("%s", __PRETTY_FUNCTION__);
    if (m_shared_mem != nullptr) {
        if (munmap(m_shared_mem, sizeof(lockable_data_t)) != 0) {
            printf("WARNING: munmap NOT successful!\n");
        }
    }
    if (m_primary) {
        // printf("%s Unlinking shared memory", __PRETTY_FUNCTION__);
        if (shm_unlink(m_name.c_str()) != 0) {
            printf("WARNING: shm_unlink failed!!!\n");
        }
    }
}