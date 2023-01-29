#include "shared_memory.h"
#include "shared/log.h"
#include <fcntl.h>
#include <sys/stat.h>

// template <class T> void SharedMemory<T>::teardown(){
SharedMemory::~SharedMemory()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    if (m_mem != nullptr) {
        if (munmap(m_mem, m_size) == 0) {
            LOGTRACE("munmap successful");
        } else {
            LOGWARN("munmap NOT successful ERROR!!!");
        }
    }

    LOGTRACE("%s Unlinking shared memory", __PRETTY_FUNCTION__);
    if (shm_unlink(m_name.c_str()) == 0) {
        LOGTRACE("shm_unlink success");
    } else {
        LOGWARN("shm_unlink failed!!!");
    }
}

// template <class T> SharedMemory<T>::~SharedMemory()
// // SharedMemory::~SharedMemory()
// {
//     // }

// }
// template <class T> SharedMemory<T>::SharedMemory(SharedMemory::RoleEnum role, std::string region_name) :
// // m_role(role), m_size(sizeof(T))
// SharedMemory::SharedMemory(SharedMemory::RoleEnum role, std::string region_name)
//     : m_role(role), m_size(sizeof(uint32_t))
// {
//     (void)role;
//     (void)region_name;
// }
// template <class T> SharedMemory<T>::SharedMemory(SharedMemory::RoleEnum role, std::string region_name) :
// m_role(role), m_size(sizeof(T))
SharedMemory::SharedMemory(std::string region_name) : m_size(sizeof(uint32_t)), m_name(region_name)
{
    // if(m_role == RoleEnum::Controller){
    // m_mutex_sem = sem_open(SHARED_MEM_MUTEX_NAME.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR, 0);
    // }
    // else{
    // m_mutex_sem = sem_open(SHARED_MEM_MUTEX_NAME.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR, 0);

    // }
    // // Create a shared memory region that can be accessed as a virtual "SCSI bus"
    // //  mutual exclusion semaphore, mutex_sem with an initial value 0.
    // if ( == SEM_FAILED) {
    //     LOGERROR("Unable to open shared memory semaphore %s. Is scsisim already running?",
    //              SHARED_MEM_MUTEX_NAME.c_str());
    //     return 0;
    // }
    // LOGTRACE("%s Successfully created mutex %s", __PRETTY_FUNCTION__, SHARED_MEM_MUTEX_NAME.c_str())

    // sem_post(mutex_sem);

    LOGINFO("%s Opening shared memory %s", __PRETTY_FUNCTION__, region_name.c_str())
    // Get shared memory
    int mode = S_IRWXU | S_IRWXG;
    if ((m_fd_shared_mem = shm_open(region_name.c_str(), O_RDWR | O_CREAT | O_TRUNC, mode)) == -1) {
        LOGERROR("Unable to open shared memory %s.  Is scsisim already running?", region_name.c_str());
        sem_close(m_mutex_sem);
    }
    LOGTRACE("%s Successfully created shared memory %s", __PRETTY_FUNCTION__, region_name.c_str())

    // Extend the shared memory, since its default size is zero
    if (ftruncate(m_fd_shared_mem, sizeof(lockable_data_t)) == -1) {
        LOGERROR("Unable to expand shared memory");
        sem_close(m_mutex_sem);
        shm_unlink(region_name.c_str());
        return;
    }
    LOGINFO("%s Shared memory region expanded to %d bytes", __PRETTY_FUNCTION__, static_cast<int>(sizeof(uint32_t)))

    // signals.Reset(NULL, sizeof(uint32_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);

    shared_mem =
        (lockable_data_t*)mmap(NULL, sizeof(lockable_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, m_fd_shared_mem, 0);
    if (shared_mem == MAP_FAILED) {
        LOGERROR("Unabled to map shared memory");
        sem_close(m_mutex_sem);
        shm_unlink(region_name.c_str());
        return;
    }

    LOGINFO("%s Shared memory region successfully memory mapped", __PRETTY_FUNCTION__)

    // *signals.mem() = 0;
}

// template <class T > SharedMemory<T>::~SharedMemory() {

// // template <typename T>
// // shared_ptr<SharedMemory<T>> SharedMemory::Create(SharedMemory::Role role){

// //         mem_  = mmap(start, size, prot, flags, fd, offset);
// //         size_ = size;

// }