#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "circular_buffer.h"

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

typedef struct worker_args {
    pthread_t tid;
    circular_buffer *buffer;
    pthread_mutex_t *mxResults;
    int *results;
} worker_args_t;

int main(int argc, char *argv[]) {
    circular_buffer *buffer = circular_buffer_init();
    if (buffer == NULL) {
        ERR("circular_buffer_init");
    }

    circular_buffer_enqueue(buffer, "item1");
    circular_buffer_enqueue(buffer, "item2");
    circular_buffer_enqueue(buffer, "item3");

    char *item;
    item = circular_buffer_dequeue(buffer);
    printf("Dequeued: %s\n", item);
    free(item);

    item = circular_buffer_dequeue(buffer);
    printf("Dequeued: %s\n", item);
    free(item);

    item = circular_buffer_dequeue(buffer);
    printf("Dequeued: %s\n", item);
    free(item);

    circular_buffer_deinit(buffer);

    return EXIT_SUCCESS;
}

