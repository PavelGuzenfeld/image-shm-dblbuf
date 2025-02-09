#pragma once
#include "error.h" // handle_errorno
#include "exception-rt/exception.hpp" 
#include <cassert>      // assert
#include <expected>     // std::expected, std::unexpected
#include <fcntl.h>      // O_CREAT, O_RDWR
#include <fmt/format.h> // fmt::format
#include <semaphore.h>  // sem_open, sem_wait, sem_post, sem_close
#include <string>       // std::string

namespace shm::impl
{
    class Semaphore
    {
    public:
        Semaphore(std::string const &name, int initial_value = 0)
            : sem_name_(std::move(name))
        {
            sem_t *sem = sem_open(name.c_str(), O_CREAT | O_EXCL, 0644, initial_value);
            if (sem == SEM_FAILED)
            {
                if (errno == EEXIST)
                {
                    sem = sem_open(name.c_str(), O_RDWR, 0644, initial_value);
                    if (sem == SEM_FAILED)
                    {
                        throw std::runtime_error(fmt::format("sem_open failed: {} for semaphore: {}", strerror(errno), name));
                    }
                }
                else
                {
                    throw std::runtime_error(fmt::format("sem_open failed: {} for semaphore: {}", strerror(errno), name));
                }
            }

            // check the semaphore's current value to avoid stale/unclean semaphores
            int sem_value{};
            if (sem_getvalue(sem, &sem_value) == -1)
            {
                sem_close(sem);
                throw std::runtime_error(fmt::format("sem_getvalue failed: {} for semaphore: {}", strerror(errno), name));
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
                    throw std::runtime_error(fmt::format("sem_open failed during reset: {} for semaphore: {}", strerror(errno), name));
                }
            }

            sem_ = sem;
        }

        inline bool is_valid() const
        {
            return sem_ != nullptr;
        }

        inline void wait()
        {
            assert(is_valid() && "semaphore is not valid");
            auto const result = sem_wait(sem_);
            handle_errorno(result, "sem_wait");
        }

        inline void post()
        {
            assert(is_valid() && "semaphore is not valid");
            auto const result = sem_post(sem_);
            handle_errorno(result, "sem_post");
        }

        void destroy()
        {
            if(!is_valid())
            {
                return;
            }
            post();
            sem_unlink(sem_name_.c_str());
            sem_close(sem_);
            sem_ = nullptr;
        }

    private:
        std::string sem_name_;
        sem_t *sem_ = nullptr;
    };

    class Guard
    {
    public:
        // Acquire the semaphore upon construction
        explicit Guard(Semaphore &semaphore)
            : sem_(semaphore), locked_(false)
        {
            sem_.wait();
            locked_ = true;
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
                sem_.post();
                locked_ = false;
            }
        }
    };

} // namespace shm::impl