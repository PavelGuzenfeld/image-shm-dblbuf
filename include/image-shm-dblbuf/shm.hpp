#pragma once
#include "double-buffer-swapper/swapper.hpp"
#include "image-shm-dblbuf/image.hpp"
#include "image-shm-dblbuf/impl/flat_shm.h"
#include "image-shm-dblbuf/impl/semaphore.h"
#include "single-task-runner/runner.hpp"

using Image = img::Image4K_RGB;

struct ReturnImage
{
    Image **img_ptr_ = nullptr;
};

struct DoubleBufferShem
{
    shm::impl::Shm shm_;
    shm::impl::Semaphore sem_;
    std::unique_ptr<Image> pre_allocated_ = nullptr;
    std::unique_ptr<DoubleBufferSwapper<Image>> swapper_;
    std::unique_ptr<run::SingleTaskRunner> runner_;
    Image *img_ptr_ = nullptr;
    ReturnImage return_image_;

    DoubleBufferShem(shm::impl::Shm &&shm, shm::impl::Semaphore &&sem)
        : shm_(std::move(shm)),
          sem_(std::move(sem))
    {
        pre_allocated_ = std::make_unique<Image>();
        swapper_ = std::make_unique<DoubleBufferSwapper<Image>>(&img_ptr_, pre_allocated_.get());
        runner_ = std::make_unique<run::SingleTaskRunner>([this]
                                                          {
                                                              shm::impl::wait(sem_);
                                                              swapper_->swap();
                                                              shm::impl::post(sem_); },
                                                          [this](std::string_view msg)
                                                          { log(msg); });
        return_image_.img_ptr_ = &img_ptr_;
        runner_->async_start();
    }

    inline void log(std::string_view msg) const noexcept
    {
        fmt::print("{}", msg);
    }
};

[[nodiscard]] constexpr DoubleBufferShem create_shm(const std::string &shm_name)
{
    auto impl = shm::impl::create(shm_name, sizeof(Image));
    if (!impl)
        throw std::runtime_error(impl.error());

    auto sem = shm::impl::create(shm_name + "_sem", 1);
    if (!sem)
        throw std::runtime_error(sem.error());

    return DoubleBufferShem(std::move(impl.value()), std::move(sem.value()));
};

void destroy_shm(DoubleBufferShem &self)
{
    self.runner_->async_stop();
    shm::impl::destroy(self.sem_);
    shm::impl::destroy(self.shm_);
    self.return_image_.img_ptr_ = nullptr;
}

void store(DoubleBufferShem &self, Image const &image)
{
    assert(self.shm_.data_ && "shared memory data is null");
    shm::impl::wait(self.sem_);
    *static_cast<Image *>(shm::impl::get(self.shm_)) = image;
    shm::impl::post(self.sem_);
}

ReturnImage load(DoubleBufferShem &self)
{
    auto *img = static_cast<Image *>(shm::impl::get(self.shm_));
    assert(img && "shared memory data is null");
    self.swapper_->set_active(img);
    self.swapper_->stage(img);
    self.runner_->trigger_once();
    return self.return_image_;
}