#include "image-shm-dblbuf/shm.hpp"
#include "nanobind/nanobind.h"
#include "nanobind/ndarray.h"
#include "nanobind/stl/shared_ptr.h"
#include "nanobind/stl/string.h"

namespace nb = nanobind;
using namespace nb::literals;

struct ProducerConsumer
{
     shm::impl::Shm shm_;
     std::shared_ptr<img::Image4K_RGB> image_ = std::make_shared<img::Image4K_RGB>();

     ProducerConsumer(std::string const &shm_name)
         : shm_(shm_name, sizeof(img::Image4K_RGB))
     {
     }
};

//--------------------------------------------------------------------------------------------

NB_MODULE(image_shm_dblbuff, m)
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

     nb::class_<ReturnImage>(m, "ReturnImage")
         .def(nb::init<>())
         .def("timestamp", &ReturnImage::timestamp, nb::rv_policy::copy)
         .def("frame_number", &ReturnImage::frame_number, nb::rv_policy::copy)
         .def("get_data", [](const ReturnImage &self)
              { return nb::ndarray<uint8_t const, nb::numpy, nb::shape<2160, 3840, 3>>(((*self.img_ptr_)->data.data())); }, nb::rv_policy::reference_internal)
         .def("__repr__", [](const ReturnImage &self) -> std::string
              { return fmt::format("ReturnImage(ptr = {:p}, timestamp = {}, frame_number = {})",
                                   static_cast<const void *>(*self.img_ptr_),
                                   (*self.img_ptr_)->timestamp,
                                   (*self.img_ptr_)->frame_number); });

     nb::class_<ProducerConsumer>(m, "ProducerConsumer")
         .def(nb::init<std::string>(), nb::rv_policy::reference_internal)
         .def("store", [](ProducerConsumer &self, img::Image4K_RGB const &image)
              { *static_cast<img::Image4K_RGB *>(self.shm_.get()) = image; })
         .def("load", [](ProducerConsumer &self) -> std::shared_ptr<img::Image4K_RGB>
              {
                 *self.image_.get() = *static_cast<img::Image4K_RGB *>(self.shm_.get());
                 return self.image_; }, nb::rv_policy::reference_internal);

     nb::class_<DoubleBufferShem>(m, "DoubleBufferShem")
         .def(nb::init<std::string>(), nb::rv_policy::reference_internal)
         .def("store", [](DoubleBufferShem &self, img::Image4K_RGB const &image)
              { self.store(image); })

         .def("load", [](DoubleBufferShem &self) -> ReturnImage
              { return self.load(); }, nb::rv_policy::reference_internal)
         .def("__repr__", [](DoubleBufferShem const &self) -> std::string
              { return fmt::format("DoubleBufferShem(shm = {:p}, img_ptr = {:p}, img = {:p})",
                                   self.shm_.get(),
                                   static_cast<const void *>(self.img_ptr_),
                                   static_cast<const void *>(self.pre_allocated_.get())); });
}
