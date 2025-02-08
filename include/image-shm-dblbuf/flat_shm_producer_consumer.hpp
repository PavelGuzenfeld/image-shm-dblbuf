#pragma once
#include "impl/flat_shm.h"
#include "impl/semaphore.h"
#include <functional>

namespace flat_shm
{
    template <typename T>
    struct FlatShmProducerConsumer
    {
        static constexpr std::expected<FlatShmProducerConsumer<T>, std::string> create(std::string const &shm_name) noexcept
        {
            auto const size = sizeof(T);
            auto impl = shm::impl::create(shm_name, size);
            if (impl.has_value())
            {
                auto const sem_read_name = fmt::format("/{}_read", shm_name);
                auto sem_read = shm::impl::create(sem_read_name, 0);
                if (!sem_read.has_value())
                {
                    return std::unexpected(std::move(sem_read.error()));
                }

                auto const sem_write_name = fmt::format("/{}_write", shm_name);
                auto sem_write = shm::impl::create(sem_write_name, 1);
                if (!sem_write.has_value())
                {
                    return std::unexpected(std::move(sem_write.error()));
                }

                return FlatShmProducerConsumer<T>{
                    std::move(impl.value()),
                    std::move(sem_read.value()),
                    std::move(sem_write.value())};
            }
            return std::unexpected(impl.error());
        }

        FlatShmProducerConsumer(FlatShmProducerConsumer const &) = delete;
        FlatShmProducerConsumer &operator=(FlatShmProducerConsumer const &) = delete;
        FlatShmProducerConsumer(FlatShmProducerConsumer &&other)
        {
            impl_ = std::move(other.impl_);
            sem_read_ = std::move(other.sem_read_);
            sem_write_ = std::move(other.sem_write_);
        }
        FlatShmProducerConsumer &operator=(FlatShmProducerConsumer &&other)
        {
            if (this != &other)
            {
                shm::impl::destroy(impl_);
                impl_ = std::move(other.impl_);
                sem_read_ = std::move(other.sem_read_);
                sem_write_ = std::move(other.sem_write_);
            }
            return *this;
        }

        ~FlatShmProducerConsumer() noexcept
        {
            shm::impl::destroy(impl_);
            shm::impl::destroy(sem_read_);
            shm::impl::destroy(sem_write_);
        }

        inline void produce(T const &data) noexcept
        {
            shm::impl::wait(sem_write_);
            get() = data;
            shm::impl::post(sem_read_);
        }

        inline T const &consume() noexcept
        {
            shm::impl::wait(sem_read_);
            T const &data = get();
            shm::impl::post(sem_write_);
            return data;
        }

        void consume(std::function<void(T const &)> consumer) noexcept
        {
            shm::impl::wait(sem_read_);
            consumer(get());
            shm::impl::post(sem_write_);
        }

    private:
        shm::impl::shm impl_;
        shm::impl::Semaphore sem_read_;
        shm::impl::Semaphore sem_write_;

        constexpr FlatShmProducerConsumer(shm::impl::shm &&impl, shm::impl::Semaphore &&sem_read, shm::impl::Semaphore &&sem_write) noexcept
            : impl_{std::move(impl)}, sem_read_{std::move(sem_read)}, sem_write_{std::move(sem_write)}
        {
        }

        inline T &get() noexcept
        {
            return *static_cast<T *>(shm::impl::get(impl_));
        }

    };
} // namespace flat_shm