#include "image-shm-dblbuf/shm.hpp"
#include <fmt/core.h>
#include <cassert>

DoubleBufferShem *g_doubleBufferShem = nullptr;

int main()
{
    auto const shm_name = "test";
    auto shm = create_shm(shm_name);
    g_doubleBufferShem = &shm;

    auto img_ptr = std::make_unique<Image>();
    img_ptr->timestamp = 123456789;
    img_ptr->frame_number = 123;
    std::fill(img_ptr->data.begin(), img_ptr->data.end(), 0x42);

    store(shm, *img_ptr.get());
    assert(static_cast<Image *>(shm.shm_.data_)->timestamp == 123456789);
    assert(static_cast<Image *>(shm.shm_.data_)->frame_number == 123);
    assert(std::all_of(static_cast<Image *>(shm.shm_.data_)->data.begin(),
                       static_cast<Image *>(shm.shm_.data_)->data.end(),
                       [](auto const &v) { return v == 0x42; }));

    
    fmt::print("Shm memory address: {}\n", static_cast<void *>(shm.shm_.data_));
    fmt::print("Pre-allocated memory address: {}\n", static_cast<void *>(shm.pre_allocated_.get()));
    fmt::print("Image memory address: {}\n", static_cast<void *>(img_ptr.get()));


    //simulate load function
    // auto * img = static_cast<Image *>(shm.shm_.data_);
    // shm.swapper_->set_active(img);
    // assert(shm.img_ptr_ == img);
    // shm.swapper_->stage(img);
    
    //simulate the swapper thread
    // shm::impl::wait(shm.sem_);
    // shm.swapper_->swap();
    // shm::impl::post(shm.sem_);
    // assert(shm.img_ptr_ == shm.pre_allocated_.get());

    // Segmentation fault
    auto result = load(shm);
    assert((*result.img_ptr_)->timestamp == 123456789);

    destroy_shm(shm);
    return 0;
}