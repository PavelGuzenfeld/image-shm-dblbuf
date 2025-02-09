#include "image-shm-dblbuf/flat_shared_memory.hpp"
#include "image-shm-dblbuf/flat_shm_producer_consumer.hpp"
#include <assert.h>
#include <atomic>
#include <chrono>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <vector>

void shm_integral_test()
{
    using namespace flat_shm;

    {
        fmt::print("Test SharedMemory with int\n");
        auto shared_memory = SharedMemory<int>("int_file_name");
        shared_memory.get() = 42;
        assert(shared_memory.get() == 42 && "Failed to read int");
    }

    {
        fmt::print("Test SharedMemory with double\n");
        auto shared_memory = SharedMemory<double>("double_file_name");
        shared_memory.get() = 42.42;
        assert(shared_memory.get() == 42.42 && "Failed to read double");
    }

    {
        fmt::print("Test SharedMemory with char\n");
        auto shared_memory = SharedMemory<char>("char_file_name");
        shared_memory.get() = 'c';
        assert(shared_memory.get() == 'c' && "Failed to read char");
    }

    {
        fmt::print("Test SharedMemory with array\n");
        auto shared_memory = SharedMemory<int[10]>("array_file_name");
        int data[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        std::copy_n(data, 10, shared_memory.get());
        assert(std::all_of(shared_memory.get(), shared_memory.get() + 10, [i = 0](int v) mutable { return v == i++; }) && "Failed to read array");
        assert(shared_memory.size() == sizeof(data) && "Failed to get size of array");
        assert(shared_memory.path() == "/dev/shm/array_file_name" && "Failed to get path of array");
    }

    {
        fmt::print("Move constructor test\n");
        auto shared_memory = SharedMemory<int>("move_constructor_file_name");
        shared_memory.get() = 42;
        auto shared_memory2 = std::move(shared_memory);
        assert(shared_memory2.get() == 42 && "Failed to read int after move constructor");
        (void)shared_memory2; // Avoid unused variable warning in release mode
    }

    {
        fmt::print("Move assignment test\n");
        auto shared_memory = SharedMemory<int>("move_assignment_file_name");
        shared_memory.get() = 42;
        auto shared_memory2 = SharedMemory<int>("move_assignment_file_name2");
        shared_memory2 = std::move(shared_memory);
        assert(shared_memory2.get() == 42 && "Failed to read int after move assignment");
        (void)shared_memory2; // Avoid unused variable warning in release mode
    }
}

void shm_structs_test()
{
    using namespace flat_shm;
    {
        fmt::print("Test SharedMemory with struct\n");
        struct FlatStruct
        {
            int a;
            double b;
            char buffer[50]; // Correct declaration of a fixed-size array
        };
        auto shared_memory = SharedMemory<FlatStruct>("struct_file_name");
        shared_memory.get() = {42, 42.42, "Hello, shared memory!"};
        assert(shared_memory.get().a == 42 && "Failed to read struct.a");
        assert(shared_memory.get().b == 42.42 && "Failed to read struct.b");
        assert(std::string(shared_memory.get().buffer) == "Hello, shared memory!" && "Failed to read struct.buffer");
        assert(shared_memory.size() == sizeof(FlatStruct) && "Failed to get size of struct");
        assert(shared_memory.path() == "/dev/shm/struct_file_name" && "Failed to get path of struct");
    }

    {
        fmt::print("Test SharedMemory with nested struct\n");

        struct FlatStruct
        {
            int a;
            double b;
            char buffer[50]; // Correct declaration of a fixed-size array
        };
        struct NestedFlatStruct
        {
            FlatStruct inner;
            int c;
        };

        auto shared_memory = SharedMemory<NestedFlatStruct>("nested_struct_file_name");
        auto start = std::chrono::high_resolution_clock::now();
        shared_memory.get() = {{42, 42.42, "Hello, shared memory!"}, 42};
        auto read_struct = shared_memory.get();
        auto end = std::chrono::high_resolution_clock::now();
        fmt::print("Time to write and read nested struct: {} us\n", std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
        assert(read_struct.inner.a == 42 && "Failed to read nested struct.inner.a");
        assert(read_struct.inner.b == 42.42 && "Failed to read nested struct.inner.b");
        assert(std::string(read_struct.inner.buffer) == "Hello, shared memory!" && "Failed to read nested struct.inner.buffer");
        assert(read_struct.c == 42 && "Failed to read nested struct.c");
        assert(shared_memory.size() == sizeof(NestedFlatStruct) && "Failed to get size of nested struct");
        assert(shared_memory.path() == "/dev/shm/nested_struct_file_name" && "Failed to get path of nested struct");
        (void)read_struct; // Avoid unused variable warning in release mode
    }
}

void shared_memory_struct_test()
{
    using namespace flat_shm;
    {
        fmt::print("Test SharedMemory with struct\n");
        struct FlatStruct
        {
            int a;
            double b;
            char buffer[50]; // Correct declaration of a fixed-size array
        };
        auto shared_memory = SharedMemory<FlatStruct>("struct_file_name");
        shared_memory.get() = {42, 42.42, "Hello, shared memory!"};
        auto read_struct = shared_memory.get();
        assert(read_struct.a == 42 && "Failed to read struct.a");
        assert(read_struct.b == 42.42 && "Failed to read struct.b");
        assert(std::string(read_struct.buffer) == "Hello, shared memory!" && "Failed to read struct.buffer");
        assert(shared_memory.size() == sizeof(FlatStruct) && "Failed to get size of struct");
        assert(shared_memory.path() == "/dev/shm/struct_file_name" && "Failed to get path of struct");
        (void)read_struct;   // Avoid unused variable warning in release mode
    }
}

void shm_image_with_semaphores()
{
    using namespace flat_shm;
    {
        fmt::print("Test SharedMemory with large image between processes 10K times - timed\n");
        constexpr auto SHARED_MEM_IMAGE_4K_SIZE = 3 * 3840 * 2160; // 4K image size (bytes)
        using ImageDataType = std::array<std::byte, SHARED_MEM_IMAGE_4K_SIZE>;

        struct TimedImage
        {
            std::chrono::high_resolution_clock::time_point time_stamp;
            ImageDataType pixels;
        };

        struct Stats
        {
            std::chrono::microseconds duration_accumulator = std::chrono::microseconds{0};
            size_t read_count = 0;
        };

        // create image data
        auto large_data = std::make_unique<TimedImage>();
        std::fill(large_data->pixels.begin(), large_data->pixels.end(), std::byte{0x42});

        // create shared memory
        auto shared_memory = SharedMemory<TimedImage>("image_shm_test");
        auto shared_stats = SharedMemory<Stats>("stats_shm_test");

        constexpr int N = 10; // number of subprocesses
        std::vector<pid_t> child_pids;

        // Create semaphores
        sem_t *sem_write = sem_open("/shm_write_sem", O_CREAT | O_EXCL, 0644, 1);
        sem_t *sem_read = sem_open("/shm_read_sem", O_CREAT | O_EXCL, 0644, 0);

        if (sem_write == SEM_FAILED || sem_read == SEM_FAILED)
        {
            perror("Failed to create semaphores");
            exit(EXIT_FAILURE);
        }

        auto const &read_stats = shared_stats.get();
        auto const &read_data = shared_memory.get();

        for (int i = 0; i < N; ++i)
        {
            pid_t pid = fork();
            if (pid < 0)
            {
                fmt::print("Failed to fork subprocess {}\n", i);
                continue;
            }

            if (pid == 0)
            {
                // child process
                sem_wait(sem_read); // Wait for parent to write

                // verify data
                for (std::size_t j = 0; j < SHARED_MEM_IMAGE_4K_SIZE; ++j)
                {
                    if (read_data.pixels[j] != std::byte{0x42})
                    {
                        fmt::print("Data mismatch in subprocess {} at index {}\n", i, j);
                        _exit(EXIT_FAILURE);
                    }
                }
                auto const now = std::chrono::high_resolution_clock::now();
                auto const read_duration = std::chrono::duration_cast<std::chrono::microseconds>(now - read_data.time_stamp).count();

                shared_stats.get() = {.duration_accumulator = std::chrono::microseconds{read_duration + read_stats.duration_accumulator.count()},
                                       .read_count = read_stats.read_count + 1};

                fmt::print("Subprocess {} completed successfully\n", i);
                sem_post(sem_write); // Signal parent
                _exit(EXIT_SUCCESS);
            }
            else
            {
                // parent process writes data for the child
                sem_wait(sem_write); // Wait for previous child to read

                large_data->time_stamp = std::chrono::high_resolution_clock::now();
                shared_memory.get() = *large_data;

                sem_post(sem_read); // Signal child
                child_pids.push_back(pid);
            }
        }

        // parent process waits for children to complete
        for (pid_t pid : child_pids)
        {
            int status;
            waitpid(pid, &status, 0);
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
            {
                fmt::print("Subprocess failed with PID {}\n", pid);
            }
        }

        // Calculate and print average
        if (read_stats.read_count > 0)
        {
            auto average_read_duration_us = read_stats.duration_accumulator.count() / read_stats.read_count;
            fmt::print("Average transfer duration: {} us\n", average_read_duration_us);
            fmt::print("Average transfer duration: {} ms\n", average_read_duration_us / 1000);
        }
        else
        {
            fmt::print("No successful reads.\n");
        }

        // Cleanup semaphores
        sem_close(sem_write);
        sem_close(sem_read);
        sem_unlink("/shm_write_sem");
        sem_unlink("/shm_read_sem");
    }
}

// void shm_image_lock_free()
// {
//     using namespace flat_shm;
//     {
//         fmt::print("Test SharedMemory with large image between processes - lock-free style\n");

//         // 4K image (3 channels * 3840 x 2160)
//         constexpr auto SHARED_MEM_IMAGE_4K_SIZE = 3 * 3840 * 2160;
//         using ImageDataType = std::array<std::byte, SHARED_MEM_IMAGE_4K_SIZE>;

//         // We store one 'TimedImage' in shared memory
//         struct TimedImage
//         {
//             std::chrono::high_resolution_clock::time_point time_stamp;
//             ImageDataType pixels;
//         };

//         // Instead of a plain struct, store atomics directly for lock-free updates
//         struct Stats
//         {
//             // We’ll accumulate times in microseconds
//             std::atomic<long long> duration_accumulator_us{0};
//             std::atomic<size_t> read_count{0};
//         };

//         // Our combined shared memory region:
//         // - data_ready: child spins on this until the parent publishes
//         // - data_read : parent spins on this until the child finishes reading
//         // - image     : the data to transfer
//         // - stats     : accumulative stats (duration, count)
//         struct SharedRegion
//         {
//             std::atomic<bool> data_ready{false};
//             std::atomic<bool> data_read{false};
//             TimedImage image{};
//             Stats stats{};
//         };

//         // Create shared memory
//         auto shared_mem = SharedMemory<SharedRegion>::create("image_shm_test_lockfree");

//         // Prepare the large image data in the parent
//         auto large_data = std::make_unique<TimedImage>();
//         std::fill(large_data->pixels.begin(), large_data->pixels.end(), std::byte{0x42});

//         constexpr int N = 10; // number of subprocesses
//         std::vector<pid_t> child_pids;

//         // For each child, we:
//         // 1) Reset data_ready/data_read to false
//         // 2) Parent publishes the data (timestamp + image) => data_ready = true
//         // 3) Child waits on data_ready, reads + verifies, updates stats => data_read = true
//         // 4) Parent waits on data_read, goes on to next child
//         //
//         // This ensures no race conditions using only atomic flags.

//         for (int i = 0; i < N; ++i)
//         {
//             // Clear flags for this child’s iteration
//             {
//                 auto &region = shared_mem->get();
//                 region.data_ready.store(false, std::memory_order_release);
//                 region.data_read.store(false, std::memory_order_release);
//             }

//             pid_t pid = fork();
//             if (pid < 0)
//             {
//                 fmt::print("Failed to fork subprocess {}\n", i);
//                 continue;
//             }
//             else if (pid == 0)
//             {
//                 // Child process: wait until parent sets data_ready = true
//                 auto &region = shared_mem->get(); // same as readRef but non-const

//                 while (!region.data_ready.load(std::memory_order_acquire))
//                 {
//                     sched_yield();
//                 }

//                 // Child verifies data
//                 const auto &image = region.image;
//                 for (std::size_t j = 0; j < SHARED_MEM_IMAGE_4K_SIZE; ++j)
//                 {
//                     if (image.pixels[j] != std::byte{0x42})
//                     {
//                         fmt::print("Data mismatch in subprocess {} at index {}\n", i, j);
//                         _exit(EXIT_FAILURE);
//                     }
//                 }

//                 // Compute time
//                 auto now = std::chrono::high_resolution_clock::now();
//                 auto read_duration_us = std::chrono::duration_cast<std::chrono::microseconds>(now - image.time_stamp).count();

//                 // Update stats atomically
//                 region.stats.duration_accumulator_us.fetch_add(read_duration_us, std::memory_order_relaxed);
//                 region.stats.read_count.fetch_add(1, std::memory_order_relaxed);

//                 fmt::print("Subprocess {} completed successfully\n", i);

//                 // Indicate to the parent that data is read
//                 region.data_read.store(true, std::memory_order_release);

//                 _exit(EXIT_SUCCESS);
//             }
//             else
//             {
//                 // Parent process: publish data for the child
//                 child_pids.push_back(pid);

//                 // 1) Set the timestamp + copy the large data
//                 {
//                     auto &region = shared_mem->get();
//                     region.image.time_stamp = std::chrono::high_resolution_clock::now();
//                     region.image.pixels = large_data->pixels;
//                 }

//                 // 2) data_ready = true
//                 {
//                     auto &region = shared_mem->get();
//                     region.data_ready.store(true, std::memory_order_release);
//                 }

//                 // 3) Wait until the child sets data_read = true
//                 {
//                     auto &region = shared_mem->get();
//                     while (!region.data_read.load(std::memory_order_acquire))
//                     {
//                         sched_yield();
//                     }
//                 }
//             }
//         }

//         // Wait for all children to complete
//         for (auto pid : child_pids)
//         {
//             int status;
//             waitpid(pid, &status, 0);
//             if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
//             {
//                 fmt::print("Subprocess failed with PID {}\n", pid);
//             }
//         }

//         // Print average duration if we had successful reads
//         {
//             const auto &region = shared_mem->get();
//             auto read_count = region.stats.read_count.load(std::memory_order_relaxed);
//             if (read_count > 0)
//             {
//                 auto duration_acc = region.stats.duration_accumulator_us.load(std::memory_order_relaxed);
//                 auto average_us = duration_acc / read_count;
//                 fmt::print("Average transfer duration: {} us\n", average_us);
//                 fmt::print("Average transfer duration: {} ms\n", static_cast<double>(average_us) / 1000.0);
//             }
//             else
//             {
//                 fmt::print("No successful reads.\n");
//             }
//         }

//         {
//             // Producer-consumer move constructor test
//             fmt::print("Producer-consumer move constructor test\n");
//             auto producer_consumer = FlatShmProducerConsumer<int>::create("producer_consumer_move_constructor_file_name");
//             producer_consumer->produce(42);
//             auto producer_consumer2 = std::move(producer_consumer);
//             auto read_int = producer_consumer2->consume();
//             assert(read_int == 42 && "Failed to read int after producer-consumer move constructor");
//             (void)producer_consumer;  // Avoid unused variable warning in release mode
//             (void)producer_consumer2; // Avoid unused variable warning in release mode
//             (void)read_int;           // Avoid unused variable warning in release mode
//         }
//     }
// }

int main()
{
    shm_integral_test();
    shm_structs_test();
    shared_memory_struct_test();
    shm_image_with_semaphores();
    // shm_image_lock_free(); FIXME: This test is not working  - error: stack usage is 4128 bytes [-Werror=stack-usage=]
    fmt::print("All tests passed\n");
    return 0;
}