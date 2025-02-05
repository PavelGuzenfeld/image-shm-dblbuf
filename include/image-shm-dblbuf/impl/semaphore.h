#pragma once
#include <expected>     // std::expected, std::unexpected
#include <fcntl.h>      // O_CREAT, O_RDWR
#include <fmt/format.h> // fmt::format
#include <semaphore.h>  // sem_open, sem_wait, sem_post, sem_close
#include <string>       // std::string
#include <cassert>      // assert

namespace shm::impl
{
    struct Semaphore
    {
        std::string sem_name_;
        sem_t *sem_ = nullptr;
    };

    inline std::expected<Semaphore, std::string> create(std::string const &name, int initial_value = 0)
    {
        sem_t *sem = sem_open(name.c_str(), O_CREAT | O_EXCL, 0644, initial_value);
        if (sem == SEM_FAILED)
        {
            if (errno == EEXIST)
            {
                sem = sem_open(name.c_str(), O_RDWR, 0644, initial_value);
                if (sem == SEM_FAILED)
                {
                    return std::unexpected(fmt::format("sem_open failed: {} for semaphore: {}", strerror(errno), name));
                }
            }
            else
            {
                return std::unexpected(fmt::format("sem_open failed: {} for semaphore: {}", strerror(errno), name));
            }
        }

        // check the semaphore's current value to avoid stale/unclean semaphores
        int sem_value{};
        if (sem_getvalue(sem, &sem_value) == -1)
        {
            sem_close(sem);
            return std::unexpected(fmt::format("sem_getvalue failed: {} for semaphore: {}", strerror(errno), name));
        }

        // if the value is unexpectedly high or low, reset to the initial value
        if (sem_value < 0 || sem_value > initial_value)
        {
            fmt::print(stderr, "warning: resetting semaphore '{}' due to unexpected value {}\n", name, sem_value);
            sem_close(sem);
            sem_unlink(name.c_str());
            sem = sem_open(name.c_str(), O_CREAT | O_EXCL, 0644, initial_value);
            if (sem == SEM_FAILED)
            {
                return std::unexpected(fmt::format("sem_open failed during reset: {} for semaphore: {}", strerror(errno), name));
            }
        }

        return Semaphore{name, sem};
    }

    inline bool is_valid(Semaphore const &sem)
    {
        return sem.sem_ != nullptr;
    }

    inline bool wait(Semaphore &sem)
    {
        assert(is_valid(sem) && "semaphore is not valid");
        return sem_wait(sem.sem_) == 0;
    }

    inline void post(Semaphore &sem)
    {
        assert(is_valid(sem) && "semaphore is not valid");
        sem_post(sem.sem_);
    }

    void destroy(Semaphore &sem)
    {
        if (sem.sem_)
        {
            post(sem);
            sem_unlink(sem.sem_name_.c_str());
            sem_close(sem.sem_);
            sem.sem_ = nullptr;
        }
    }
    class Guard
    {
    public:
        // Acquire the semaphore upon construction
        explicit Guard(Semaphore &semaphore)
            : sem_(semaphore), locked_(false)
        {
            if (is_valid(sem_))
            {
                if (wait(sem_))
                {
                    locked_ = true;
                }
                else
                {
                    // You could throw an exception or handle error differently
                    // For minimal demonstration, just set locked_ to false.
                    locked_ = false;
                }
            }
        }

        Guard(const Guard &) = delete;
        Guard &operator=(const Guard &) = delete;
        Guard(Guard &&) = delete;
        Guard &operator=(Guard &&) = delete;

        ~Guard()
        {
            unlockIfNeeded();
        }

        bool isLocked() const { return locked_; }

    private:
        Semaphore &sem_;
        bool locked_;
        unsigned int count_;

        void unlockIfNeeded()
        {
            if (locked_)
            {
                post(sem_);
                locked_ = false;
            }
        }
    };
} // namespace shm::impl