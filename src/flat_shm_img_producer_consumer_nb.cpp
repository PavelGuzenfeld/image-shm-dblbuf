#include "double-buffer-swapper/swapper.hpp"
#include "image-shm-dblbuf/image.hpp"
#include "image-shm-dblbuf/impl/flat_shm.h"
#include "image-shm-dblbuf/impl/semaphore.h"
#include "nanobind/nanobind.h"
#include "nanobind/ndarray.h"
#include "nanobind/stl/shared_ptr.h"
#include "single-task-runner/runner.hpp"

namespace nb = nanobind;
using namespace nb::literals;

struct ProducerConsumer
{
    shm::impl::shm shm_;
    shm::impl::Semaphore sem_read_;
    shm::impl::Semaphore sem_write_;
    std::shared_ptr<img::Image4K_RGB> image_ = std::make_shared<img::Image4K_RGB>();
};

[[nodiscard]] constexpr ProducerConsumer create(const std::string &shm_name)
{
    auto impl = shm::impl::create(shm_name, sizeof(img::Image4K_RGB));
    if (!impl)
        throw std::runtime_error(impl.error());

    auto sem_read = shm::impl::create(shm_name + "_read", 0);
    if (!sem_read)
        throw std::runtime_error(sem_read.error());

    auto sem_write = shm::impl::create(shm_name + "_write", 1);
    if (!sem_write)
        throw std::runtime_error(sem_write.error());

    return {std::move(impl.value()), std::move(sem_read.value()), std::move(sem_write.value())};
}

void destroy(ProducerConsumer &producer_consumer)
{
    shm::impl::destroy(producer_consumer.shm_);
    shm::impl::destroy(producer_consumer.sem_read_);
    shm::impl::destroy(producer_consumer.sem_write_);
}

//--------------------------------------------------------------------------------------------
using Image = img::Image4K_RGB;

struct AtomicProducerConsumer
{
    shm::impl::shm shm_;
    std::shared_ptr<img::Image4K_RGB> image_ = std::make_shared<Image>();
    img::Image4K_RGB *img_ptr_ = nullptr;
    std::unique_ptr<DoubleBufferSwapper<img::Image4K_RGB>> swapper_ = nullptr;
    std::unique_ptr<run::SingleTaskRunner> runner_ = nullptr;

    AtomicProducerConsumer(shm::impl::shm &&shm)
        : shm_(std::move(shm))
    {
        swapper_ = std::make_unique<DoubleBufferSwapper<img::Image4K_RGB>>(&img_ptr_, image_.get());
        runner_ = std::make_unique<run::SingleTaskRunner>([this]
                                                          { swapper_->swap(); }, [this](std::string_view msg)
                                                          { log(msg); });
    }

    inline void log(std::string_view msg) const noexcept
    {
        fmt::print("{}", msg);
    }
};

[[nodiscard]] constexpr AtomicProducerConsumer create_atomic(const std::string &shm_name)
{
    auto impl = shm::impl::create(shm_name, sizeof(Image));
    if (!impl)
        throw std::runtime_error(impl.error());

    return std::move(impl.value());
};

void destroy_atomic(AtomicProducerConsumer &producer_consumer)
{
    shm::impl::destroy(producer_consumer.shm_);
}
NB_MODULE(Share_memory_image_producer_consumer_nb, m)
{

    nb::class_<img::Image4K_RGB>(m, "Image4K_RGB")
        .def(nb::init<>())
        .def_rw("timestamp", &img::Image4K_RGB::timestamp)
        .def_rw("frame_number", &img::Image4K_RGB::frame_number)
        .def("shape", [](const img::Image4K_RGB &)
             { return std::array<size_t, 3>{
                   img::Image4K_RGB::height,
                   img::Image4K_RGB::width,
                   static_cast<size_t>(img::channels(img::Image4K_RGB::type))}; })

        .def("get_data", [](const img::Image4K_RGB &self)
             { return nb::ndarray<uint8_t const, nb::numpy, nb::shape<2160, 3840, 3>>((self.data.data())); }, nb::rv_policy::reference_internal)

        .def("set_data", [](img::Image4K_RGB &self, nb::ndarray<uint8_t const, nb::shape<img::Image4K_RGB::height, img::Image4K_RGB::width, static_cast<std::size_t>(img::channels(img::Image4K_RGB::type))>> array)
             { std::memcpy(self.data.data(), array.data(), array.size() * sizeof(uint8_t)); });

    nb::class_<ProducerConsumer>(m, "ProducerConsumer")
        .def_static("create", [](nb::str const &shm_name)
                    { return create(shm_name.c_str()); })
        .def(nb::init<>())
        .def("close", &destroy)
        .def("store", [](ProducerConsumer &self, img::Image4K_RGB const &image)
             {
            if (!self.image_) throw std::runtime_error("Image pointer is null.");
            std::memcpy(self.shm_.data_, &image, sizeof(img::Image4K_RGB));
            shm::impl::post(self.sem_read_); })
        .def("load", [](ProducerConsumer &self) -> std::shared_ptr<img::Image4K_RGB>
             {
            if (!self.image_) throw std::runtime_error("Image pointer is null.");
            shm::impl::wait(self.sem_read_);
            std::memcpy(self.image_.get(), self.shm_.data_, sizeof(img::Image4K_RGB));
            return self.image_; }, nb::rv_policy::reference_internal);

    nb::class_<AtomicProducerConsumer>(m, "AtomicProducerConsumer")
        .def_static("create", [](nb::str const &shm_name)
                    { return create_atomic(shm_name.c_str()); })
        .def(nb::init<shm::impl::shm &&>())
        .def("close", &destroy_atomic)
        .def("store", [](AtomicProducerConsumer &self, img::Image4K_RGB const &image)
             {
        if (!self.image_)
            throw std::runtime_error("Image pointer is null.");
        std::memcpy(self.shm_.data_, &image, sizeof(Image)); })

        .def("load", [](AtomicProducerConsumer &self) -> img::Image4K_RGB const *
             {
                if (!self.image_)
                     throw std::runtime_error("Image pointer is null.");
                self.img_ptr_ = static_cast<Image *>(self.shm_.data_);
                self.swapper_->stage(self.img_ptr_);
                self.runner_->trigger_once();
                return self.img_ptr_; }, nb::rv_policy::reference_internal);
}
