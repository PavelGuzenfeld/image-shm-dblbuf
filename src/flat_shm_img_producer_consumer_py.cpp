#include "image-shm-dblbuf/image.hpp"
#include "image-shm-dblbuf/impl/semaphore.h"
#include "image-shm-dblbuf/impl/shm.h"
#include <cstring> // For std::memcpy
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
namespace py = pybind11;

struct ProducerConsumer
{
    shm::impl::Shm shm_;
    shm::impl::Semaphore sem_read_;
    shm::impl::Semaphore sem_write_;
    std::shared_ptr<img::Image4K_RGB> image_ = std::make_shared<img::Image4K_RGB>();

    ProducerConsumer(std::string const &shm_name)
        : shm_(shm_name, sizeof(img::Image4K_RGB)),
          sem_read_(shm_name + "_read_sem", 0),
          sem_write_(shm_name + "_write_sem", 1)
    {
    }
};

//--------------------------------------------------------------------------------------------
using Image = img::Image4K_RGB;

struct AtomicProducerConsumer
{
    shm::impl::Shm shm_;
    std::shared_ptr<img::Image4K_RGB> image_ = std::make_shared<Image>();

    AtomicProducerConsumer(std::string const &shm_name)
        : shm_(shm_name, sizeof(Image))
    {
    }
};

// Create the Pybind11 module
PYBIND11_MODULE(Share_memory_image_producer_consumer, m)
{
    // Expose ImageType enum
    py::enum_<img::ImageType>(m, "ImageType")
        .value("RGB", img::ImageType::RGB)
        .value("RGBA", img::ImageType::RGBA)
        .value("NV12", img::ImageType::NV12)
        .export_values();

    // Expose Image4K_RGB
    py::class_<img::Image4K_RGB, std::shared_ptr<img::Image4K_RGB>>(m, "Image4K_RGB")
        .def(py::init<>())                                              // default constructor
        .def_readwrite("timestamp", &img::Image4K_RGB::timestamp)       // expose timestamp
        .def_readwrite("frame_number", &img::Image4K_RGB::frame_number) // expose frame_number
        .def("get_data", [](const img::Image4K_RGB &self)
             {
                 py::ssize_t shape[] = {
                     static_cast<py::ssize_t>(img::Image4K_RGB::height),
                     static_cast<py::ssize_t>(img::Image4K_RGB::width),
                     static_cast<py::ssize_t>(img::channels(img::Image4K_RGB::type))};
                 py::ssize_t strides[] = {
                     static_cast<py::ssize_t>(img::Image4K_RGB::width * img::channels(img::Image4K_RGB::type)),
                     static_cast<py::ssize_t>(img::channels(img::Image4K_RGB::type)),
                     1};

                 return py::array_t<uint8_t>(
                     {shape, shape + 3},     // shape
                     {strides, strides + 3}, // strides
                     self.data.data());      // pointer to the data
             })
        .def("set_data", [](img::Image4K_RGB &self, py::array_t<uint8_t> array)
             {
        auto buf = array.request();
        if (buf.ndim != 3 ||
            buf.shape[0] != img::Image4K_RGB::height ||
            buf.shape[1] != img::Image4K_RGB::width ||
            buf.shape[2] != static_cast<std::size_t>(img::channels(img::Image4K_RGB::type))) {
            throw std::runtime_error("Invalid array shape; expected (2160, 3840, 3)");
        }
        std::memcpy(self.data.data(), buf.ptr, buf.size * sizeof(uint8_t)); });

    py::class_<ProducerConsumer>(m, "ProducerConsumer")
        .def(py::init<std::string>(), py::return_value_policy::reference_internal)
        .def("store", [](ProducerConsumer &self, img::Image4K_RGB const &image)
             { *static_cast<img::Image4K_RGB *>(self.shm_.get()) = image; })
        .def("load", [](ProducerConsumer &self) -> std::shared_ptr<img::Image4K_RGB>
             {
                 std::memcpy(self.image_.get(), self.shm_.get(), sizeof(img::Image4K_RGB));
                 return self.image_; }, py::return_value_policy::reference_internal);

    py::class_<AtomicProducerConsumer>(m, "AtomicProducerConsumer")
        .def(py::init<std::string>(), py::return_value_policy::reference_internal)
        .def("store", [](AtomicProducerConsumer &self, img::Image4K_RGB const &image)
             { std::memcpy(self.shm_.get(), &image, sizeof(Image)); })
        .def("load", [](AtomicProducerConsumer &self) -> std::shared_ptr<img::Image4K_RGB>
             {
                    std::memcpy(self.image_.get(), self.shm_.get(), sizeof(Image));
                    return self.image_; }, py::return_value_policy::reference_internal);
}
