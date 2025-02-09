import Share_memory_image_producer_consumer_nb as shm_nb
from time import sleep

shm_name = "shared_memory_4k_rgb_atomic_nb"
consumer = shm_nb.DoubleBufferShem.create(shm_name)
print("Consumer created:", consumer)
sleep(1)
retrieved_image = consumer.load()
# print(f"Retrieved image frame number: {retrieved_image.frame_number}")

while True:
    sleep(1)

consumer.close()
    