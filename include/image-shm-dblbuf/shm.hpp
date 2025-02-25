#pragma once
#include "double-buffer-swapper/swapper.hpp"
#include "image-shm-dblbuf/image.hpp"
#include "shm/semaphore.hpp"
#include "shm/shm.hpp"
#include "single-task-runner/runner.hpp"
#include <fmt/core.h>

using Image = img::Image4K_RGB;

struct ReturnImage
{
    Image **img_ptr_ = nullptr;

    inline uint64_t timestamp() const noexcept
    {
        return (*img_ptr_)->timestamp;
    }
    inline uint64_t frame_number() const noexcept
    {
        return (*img_ptr_)->frame_number;
    }
};

void log(std::string_view msg) noexcept
{
    fmt::print("{}", msg);
}

struct DoubleBufferShem
{
    shm::Shm shm_;
    shm::Semaphore sem_;
    std::unique_ptr<Image> pre_allocated_;
    std::unique_ptr<DoubleBufferSwapper<Image>> swapper_;
    std::unique_ptr<run::SingleTaskRunner> runner_;
    Image *img_ptr_;
    ReturnImage return_image_;

    DoubleBufferShem(std::string const &shm_name)
        : shm_(shm::path(shm_name), sizeof(Image)),
          sem_(shm_name + "_sem", 1),
          pre_allocated_(std::make_unique<Image>()),
          img_ptr_(nullptr),
          return_image_{&img_ptr_}
    {
        swapper_ = std::make_unique<DoubleBufferSwapper<Image>>(&img_ptr_, pre_allocated_.get());
        runner_ = std::make_unique<run::SingleTaskRunner>([&]
                                                          {
                                                            sem_.wait();
                                                            swapper_->swap();
                                                            sem_.post(); },
                                                          [&](std::string_view msg)
                                                          { log(msg); });
        runner_->async_start();
        swapper_->set_active(get_shm());
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
        *static_cast<Image *>(shm_.get()) = image;
        sem_.post();
    }

    ReturnImage load()
    {
        swapper_->stage(get_shm());
        runner_->trigger_once();
        return return_image_;
    }

    Image *get_shm() const noexcept
    {
        auto ret_ptr = static_cast<Image *>(shm_.get());
        assert(ret_ptr && "shared memory data is null");
        return ret_ptr;
    }
};

