#pragma once
#include "impl/flat_shm.h"
#include "flat-type/flat.hpp"

namespace flat_shm
{
    template <FlatType FLAT_TYPE>
    struct SharedMemory
    {
        static constexpr std::expected<SharedMemory<FLAT_TYPE>, std::string> create(std::string const &shm_name) noexcept
        {
            auto const size = sizeof(FLAT_TYPE);
            auto impl = shm::impl::create(shm_name, size);
            if (impl.has_value())
            {
                return SharedMemory<FLAT_TYPE>{std::move(impl.value())};
            }
            return std::unexpected(impl.error());
        }

        SharedMemory(SharedMemory const &) = delete;
        SharedMemory &operator=(SharedMemory const &) = delete;

        SharedMemory(SharedMemory &&other) noexcept
            : impl_(std::move(other.impl_))
        {
        }

        SharedMemory &operator=(SharedMemory &&other) noexcept
        {
            if (this != &other)
            {
                shm::impl::destroy(impl_);
                impl_ = std::move(other.impl_);
            }
            return *this;
        }

        ~SharedMemory() noexcept
        {
            shm::impl::destroy(impl_);
        }

        inline FLAT_TYPE &get() noexcept
        {
            return *static_cast<FLAT_TYPE *>(shm::impl::get(impl_));
        }

        inline auto size() const noexcept
        {
            return impl_.size_;
        }

        inline auto path() const noexcept
        {
            return impl_.file_path_;
        }

    private:
        shm::impl::shm impl_;


        SharedMemory(shm::impl::shm &&impl) noexcept
            : impl_(std::move(impl))
        {
        }
    };
} // namespace flat_shem