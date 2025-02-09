#pragma once
#include "double-buffer-swapper/swapper.hpp"
#include "image-shm-dblbuf/image.hpp"
#include "image-shm-dblbuf/impl/semaphore.h"
#include "image-shm-dblbuf/impl/shm.h"
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

    DoubleBufferShem(std::string const &shm_name, std::size_t size = sizeof(Image))
        : shm_(shm_name, size),
          sem_(shm_name + "_sem", 1),
          pre_allocated_(std::make_unique<Image>())
    {
        swapper_ = std::make_unique<DoubleBufferSwapper<Image>>(&img_ptr_, pre_allocated_.get());
        runner_ = std::make_unique<run::SingleTaskRunner>([&]
                                                          {
                                                            sem_.wait();
                                                            swapper_->swap();
                                                            sem_.post(); },
                                                          [&](std::string_view msg)
                                                          { log(msg); });
        return_image_.img_ptr_ = &img_ptr_;
        runner_->async_start();
    }

    inline void log(std::string_view msg) const noexcept
    {
        fmt::print("{}", msg);
    }

    ~DoubleBufferShem()
    {
        runner_->async_stop();
        sem_.destroy();
        return_image_.img_ptr_ = nullptr;
    }

    void store(Image const &image)
    {
        sem_.wait();
        // shm_.store(image);
        std::memcpy(shm_.get(), &image, sizeof(Image));
        sem_.post();
    }

    ReturnImage load()
    {
        auto *img = static_cast<Image *>(shm_.get());
        assert(img && "shared memory data is null");
        swapper_->set_active(img);
        swapper_->stage(img);
        runner_->trigger_once();
        return return_image_;
    }
};