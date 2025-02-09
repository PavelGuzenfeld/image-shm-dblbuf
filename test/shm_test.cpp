#include "image-shm-dblbuf/shm.hpp"
#include <cassert>
#include <fmt/core.h>
#include <thread> // std::this_thread::sleep_for

void test_result_address_switch()
{
    auto const shm_name = "test";
    auto shm = DoubleBufferShem(shm_name, sizeof(Image));

    assert(shm.shm_.get() != nullptr);

    auto img_ptr = std::make_unique<Image>();
    img_ptr->timestamp = 123456789;
    img_ptr->frame_number = 123;
    std::fill(img_ptr->data.begin(), img_ptr->data.end(), 0x42);

    shm.store(*img_ptr);
    assert(static_cast<Image *>(shm.shm_.get())->timestamp == 123456789);
    assert(static_cast<Image *>(shm.shm_.get())->frame_number == 123);
    assert(std::all_of(static_cast<Image *>(shm.shm_.get())->data.begin(),
                       static_cast<Image *>(shm.shm_.get())->data.end(),
                       [](auto const &v)
                       { return v == 0x42; }));

    fmt::print("Shm memory address: {}\n", static_cast<void *>(shm.shm_.get()));
    fmt::print("Pre-allocated memory address: {}\n", static_cast<void *>(shm.pre_allocated_.get()));
    fmt::print("Image memory address: {}\n", static_cast<void *>(img_ptr.get()));

    auto result = shm.load();
    fmt::print("Image memory address: {}, Memory address: {}\n", static_cast<void *>(result.img_ptr_), static_cast<void *>(*result.img_ptr_));
    assert(*result.img_ptr_ == shm.shm_.get() && "Image pointer should point to the shared memory");
    assert((*result.img_ptr_)->timestamp == 123456789);
    assert((*result.img_ptr_)->frame_number == 123);
    assert(std::all_of((*result.img_ptr_)->data.begin(),
                       (*result.img_ptr_)->data.end(),
                       [](auto const &v)
                       { return v == 0x42; }));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    fmt::print("Image memory address: {}, Memory address: {}\n", static_cast<void *>(result.img_ptr_), static_cast<void *>(*result.img_ptr_));
    assert(*result.img_ptr_ == shm.pre_allocated_.get() && "Image pointer should point to the pre-allocated memory");
    assert((*result.img_ptr_)->timestamp == 123456789);
    assert((*result.img_ptr_)->frame_number == 123);
    assert(std::all_of((*result.img_ptr_)->data.begin(),
                       (*result.img_ptr_)->data.end(),
                       [](auto const &v)
                       { return v == 0x42; }));
}

int main()
{
    test_result_address_switch();
    fmt::print("All tests passed!\n");
    return 0;
}