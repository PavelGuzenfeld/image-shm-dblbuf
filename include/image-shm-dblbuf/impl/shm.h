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
    struct Shm
    {
        
        
        
        std::string file_path_;
        int fd_ = -1;
        void *data_ = nullptr;
        std::size_t size_ = 0;
    };

    inline std::expected<Shm, std::string> create(std::string const &shm_name, std::size_t size) noexcept
    {
        auto const file_path = fmt::format("{}{}", SHARED_MEM_PATH, shm_name);
        // 1) open the shared memory file
        int fd = open(file_path.c_str(), O_CREAT | O_RDWR, READ_WRITE_ALL);
        if (fd < 0)
        {
            auto const error_msg = fmt::format("Shared memory open failed: {} for file: {}",
                                               strerror(errno), file_path);
            return std::unexpected(std::move(error_msg));
        }

        // 2) set the size of the shared memory file
        if (ftruncate(fd, size) < 0)
        {
            close(fd);
            auto const error_msg = fmt::format("Shared memory ftruncate failed: {} for file: {}",
                                               strerror(errno), file_path);
            return std::unexpected(std::move(error_msg));
        }

        // 3) map the shared memory
        std::byte *shm_ptr = static_cast<std::byte *>(
            mmap(nullptr, size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0));
        if (shm_ptr == MAP_FAILED)
        {
            close(fd);
            auto const error_msg = fmt::format("mmap failed: {} for file: {}",
                                               strerror(errno), file_path);
            return std::unexpected(std::move(error_msg));
        }

        // 5) Construct and return the 'shm' object
        return Shm{file_path, fd, shm_ptr, size};
    }

    inline void destroy(Shm &instance) noexcept
    {
        if (!instance.file_path_.empty())
        {
            unlink(instance.file_path_.c_str());
            instance.file_path_.clear();
        }
        if (instance.data_)
        {
            munmap(instance.data_, instance.size_);
            instance.data_ = nullptr;
        }

        if (instance.fd_ >= 0)
        {
            close(instance.fd_);
            instance.fd_ = -1;
        }
    }

    inline void *get(Shm const &instance) noexcept
    {
        return instance.data_;
    }

    std::size_t size(Shm const &instance) noexcept
    {
        return instance.size_;
    }

} // namespace shm::impl
