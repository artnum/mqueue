# C Message Queue

A simple, thread-safe message queue written in C using pthreads. This library provides a multi-producer, multi-consumer queue for inter-thread communication.

-----

## Description

This project implements a message queue that allows multiple threads to safely send and receive messages. It is designed to be a foundational component for concurrent applications where different parts of the program need to communicate asynchronously.

The queue supports:

* **Multiple Producers**: Many threads can add messages to the queue simultaneously.
* **Multiple Consumers**: Many threads can process messages from the queue.
* **Direct-style messaging**: A requesting thread can wait for a specific response.

-----

## Design Choices

Several key design choices were made to ensure thread safety and performance:

* **Separate Input and Output Queues**: The system uses a single input queue for all incoming messages and a hash map of output queues for processed messages. This separates the concerns of adding new work and retrieving completed work, reducing contention.
* **Fine-Grained Locking**: Instead of a single global lock, the queue uses multiple mutexes. There is one mutex for the input queue and an array of mutexes for the output queues. This allows multiple threads to access different parts of the queue concurrently, improving parallelism.
* **Condition Variables for Efficient Waiting**: The implementation uses `pthread_cond_t` condition variables to avoid busy-waiting. Threads waiting for messages are put to sleep and are only woken up when a message is available, which is an efficient use of CPU resources.
* **Atomic Operations**: Atomic variables are used for managing state like the `running` flag, the number of waiting threads, and the next message ID. This ensures that these values are updated safely and correctly in a multi-threaded context without requiring a mutex.
* **Hash Map for Output**: Processed messages are stored in a hash map where the key is the message ID. This allows for a fast lookup of a specific message response. The size of this map can be configured at compile time.

-----

## Quick Start Example

Here is a short example of how to use the message queue:

```c
#include <stdio.h>
#include <pthread.h>
#include "src/include/mqueue.h"

// A simple worker thread that processes messages.
void *worker_thread(void *queue) {
    MQueue *q = (MQueue *)queue;
    MQueueMessage *msg = mqueue_get_in(q);

    if (msg) {
        // Process the message (e.g., cast and use the data)
        long my_data = (long)msg->message;
        printf("Worker processed message: %ld\n", my_data);

        // In a real application, you might modify the message
        // and add it to the output queue.
        msg->message = (uintptr_t)(my_data * 2);
        mqueue_add_out(q, msg);
    }
    return NULL;
}

int main() {
    // Create a new message queue
    MQueue *queue = mqueue_create();

    // Create a worker thread
    pthread_t worker;
    pthread_create(&worker, NULL, worker_thread, queue);

    // Add a message to the input queue
    long message_data = 123;
    int msg_id = mqueue_add_in(queue, (uintptr_t)message_data);
    printf("Main thread added message with ID: %d\n", msg_id);

    // Retrieve the processed message from the output queue
    long processed_data = (long)mqueue_get_out(queue, msg_id);
    printf("Main thread received processed message: %ld\n", processed_data);

    // Clean up
    pthread_join(worker, NULL);
    mqueue_destroy(queue, NULL); // No special free function needed for long

    return 0;
}
```

### Compiling

Remember to link against the pthreads library.

```bash
gcc -o my_app my_app.c mqueue.c -lpthread
```

-----

## Useful Information

* **`MQUEUE_HASH_MAP_SIZE`**: The size of the output hash map can be changed by defining `MQUEUE_HASH_MAP_SIZE` before including `mqueue.h`. A larger value might reduce collisions at the cost of more memory.
* **`free_message`**: The `mqueue_destroy` function takes an optional function pointer `free_message` that will be called for any remaining messages in the queue. This is useful for freeing dynamically allocated message data to prevent memory leaks.
* **Error Handling**: In a production application, you should add more robust error checking for the return values of the queue functions.

-----

## License

This project is licensed under the MIT License.

```
MIT License

Copyright (c) 2025 Etienne Bagnoud

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

-----

## This README.md was generated by Gemini

The C code for the message queue was human-written, but this explanatory `README.md` file was generated by Google's Gemini to provide clear and comprehensive documentation.
