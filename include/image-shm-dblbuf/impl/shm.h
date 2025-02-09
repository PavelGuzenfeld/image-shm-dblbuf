#pragma once
#include "exception-rt/exception.hpp"
#include <expected>     // std::expected, std::unexpected
#include <fcntl.h>      // O_CREAT, O_RDWR
#include <fmt/format.h> // fmt::format
#include <string>       // std::string
#include <sys/mman.h>   // mmap, PROT_WRITE, MAP_SHARED
#include <unistd.h>     // ftruncate, close, open

namespace shm::impl
{
    constexpr auto const SHARED_MEM_PATH = "/dev/shm/";
    constexpr auto const READ_WRITE_ALL = 0666;

    class Shm
    {
    public:
        Shm(std::string file_path, std::size_t size)
            : file_path_(path(file_path)),
              size_(size)
        {
            // 1) open the shared memory file
            int const fd = open(file_path.c_str(), O_CREAT | O_RDWR, READ_WRITE_ALL);
            if (fd < 0)
            {
                auto const error_msg = fmt::format("Shared memory open failed: {} for file: {}",
                                                   strerror(errno), file_path);
                throw std::runtime_error(std::move(error_msg));
            }

            // 2) set the size of the shared memory file
            if (ftruncate(fd, size) < 0)
            {
                close(fd);
                auto const error_msg = fmt::format("Shared memory ftruncate failed: {} for file: {}",
                                                   strerror(errno), file_path);
                throw std::runtime_error(std::move(error_msg));
            }

            // 3) map the shared memory
            void *shm_ptr = mmap(nullptr, size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
            if (shm_ptr == MAP_FAILED)
            {
                close(fd);
                auto const error_msg = fmt::format("mmap failed: {} for file: {}",
                                                   strerror(errno), file_path);
                throw std::runtime_error(std::move(error_msg));
            }

            fd_ = fd;
            data_ = shm_ptr;
        }

        Shm(Shm const &) = delete;
        Shm &operator=(Shm const &) = delete;

        Shm(Shm &&other) noexcept
            : file_path_(std::move(other.file_path_)),
              size_(other.size_),
              fd_(other.fd_),
              data_(other.data_)
        {
            other.fd_ = -1;
            other.data_ = nullptr;
            other.size_ = 0;
        }

        Shm &operator=(Shm &&other) noexcept
        {
            if (this != &other)
            {
                destroy();
                file_path_ = std::move(other.file_path_);
                fd_ = other.fd_;
                data_ = other.data_;
                size_ = other.size_;
                other.fd_ = -1;
                other.data_ = nullptr;
                other.size_ = 0;
            }
            return *this;
        }

        ~Shm() noexcept
        {
            destroy();
        }

        inline void *get() const noexcept
        {
            return data_;
        }

        inline std::size_t size() const noexcept
        {
            return size_;
        }

        inline std::string file_path() const noexcept
        {
            return file_path_;
        }

    private:
        constexpr std::string path(std::string const &file_path) noexcept
        {
            return fmt::format("{}{}", SHARED_MEM_PATH, file_path);
        }
        void destroy() noexcept
        {
            if (!file_path_.empty())
            {
                unlink(file_path_.c_str());
                file_path_.clear();
            }
            if (data_)
            {
                munmap(data_, size_);
                data_ = nullptr;
            }

            if (fd_ >= 0)
            {
                close(fd_);
                fd_ = -1;
            }
        }

        std::string file_path_;
        std::size_t size_ = 0;
        int fd_ = -1;
        mutable void *data_ = nullptr;
    };

} // namespace shm::impl
