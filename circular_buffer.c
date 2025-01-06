#include "circular_buffer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

circular_buffer* circular_buffer_init() {
    circular_buffer *cb = malloc(sizeof(circular_buffer));
    if (cb == NULL) 
        ERR("malloc");
    cb->head = 0;
    cb->tail = 0;
    cb->count = 0;
    if (pthread_mutex_init(&cb->mxBuffer, NULL) != 0) {
        free(cb);
        ERR("pthread_mutex_init");
    }
    return cb;
}

void circular_buffer_deinit(circular_buffer *buffer) {
    pthread_mutex_destroy(&buffer->mxBuffer);
    free(buffer);
}

void circular_buffer_enqueue(circular_buffer *buffer, const char *item) {
    pthread_mutex_lock(&buffer->mxBuffer);
    while (buffer->count == BUFFER_SIZE) {
        pthread_mutex_unlock(&buffer->mxBuffer);
        pthread_mutex_lock(&buffer->mxBuffer);
    }
    buffer->buffer[buffer->head] = strdup(item);
    if (buffer->buffer[buffer->head] == NULL) {
        pthread_mutex_unlock(&buffer->mxBuffer);
        ERR("strdup");
    }
    buffer->head = (buffer->head + 1) % BUFFER_SIZE;
    buffer->count++;
    pthread_mutex_unlock(&buffer->mxBuffer);
}

char* circular_buffer_dequeue(circular_buffer *buffer) {
    pthread_mutex_lock(&buffer->mxBuffer);
    while (buffer->count == 0) {
        pthread_mutex_unlock(&buffer->mxBuffer);
        pthread_mutex_lock(&buffer->mxBuffer);
    }
    char *item = buffer->buffer[buffer->tail];
    buffer->tail = (buffer->tail + 1) % BUFFER_SIZE;
    buffer->count--;
    pthread_mutex_unlock(&buffer->mxBuffer);
    return item;
}
