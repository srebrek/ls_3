#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "circular_buffer.h"
#include <dirent.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define MAX_LENGTH 256

typedef struct worker_args {
    pthread_t tid;
    circular_buffer *buffer;
    pthread_mutex_t *mxProcessedCount;
    int *processedCount;
} worker_args_t;

void readArguments(int argc, char **argv, char *workingDirPath, int *threadCount);
void walkThroughDirectory(const char *dirPath, circular_buffer *buffer, int *fileCount);
void *worker(void *args);

int main(int argc, char *argv[]) {
    int threadCount;
    char workingDirPath[MAX_LENGTH];
    readArguments(argc, argv, workingDirPath, &threadCount);
    printf("Working directory: %s\n", workingDirPath);
    printf("Thread count: %d\n", threadCount);

    worker_args_t *workersArgs = malloc(threadCount * sizeof(worker_args_t));
    if (workersArgs == NULL)
        ERR("malloc");

    circular_buffer *buffer = circular_buffer_init();
    int results = 0;
    int fileCount = 0;
    pthread_mutex_t mxResults = PTHREAD_MUTEX_INITIALIZER;

    for (int i = 0; i < threadCount; i++)
    {
        workersArgs[i].buffer = buffer;
        workersArgs[i].mxProcessedCount = &mxResults;
        workersArgs[i].processedCount = &results;
    }

    for (int i = 0; i < threadCount; i++)
    {
        if (pthread_create(&workersArgs[i].tid, NULL, worker, &workersArgs[i]) != 0)
            ERR("pthread_create");
    }

    walkThroughDirectory(workingDirPath, buffer, &fileCount);

    while (true)
    {
        pthread_mutex_lock(&mxResults);
        if (results == fileCount)
        {
            pthread_mutex_unlock(&mxResults);
            break;
        }
        pthread_mutex_unlock(&mxResults);
    }

    for (int i = 0; i < threadCount; i++) {
        circular_buffer_enqueue(buffer, "");
    }

    for (int i = 0; i < threadCount; i++) {
        if (pthread_join(workersArgs[i].tid, NULL) != 0) {
            free(workersArgs);
            circular_buffer_deinit(buffer);
            ERR("pthread_join");
        }
    }

    while (buffer->count > 0) {
        char *item = circular_buffer_dequeue(buffer);
        free(item);
    }

    free(workersArgs);
    circular_buffer_deinit(buffer);

    return EXIT_SUCCESS;
}

void readArguments(int argc, char **argv, char *workingDirPath, int *threadCount)
{
    if (argc >= 2)
    {
        strcpy(workingDirPath, argv[1]);
        workingDirPath[MAX_LENGTH - 1] = '\0';
    }
    if (argc >= 3)
    {
        *threadCount = atoi(argv[2]);
        if (*threadCount <= 0)
        {
            printf("Invalid value for 'thread count'");
            exit(EXIT_FAILURE);
        }
    }
}

void walkThroughDirectory(const char *dirPath, circular_buffer *buffer, int *fileCount)
{
    DIR *dir = opendir(dirPath);
    if (dir == NULL)
        ERR("opendir");

    struct dirent *currentPath;
    while ((currentPath = readdir(dir)) != NULL)
    {
        if (strcmp(currentPath->d_name, ".") == 0 || strcmp(currentPath->d_name, "..") == 0)
            continue;

        char path[MAX_LENGTH];
        strcpy(path, dirPath);
        strcat(path, "/");
        strcat(path, currentPath->d_name);
        path[MAX_LENGTH - 1] = '\0';

        if (currentPath->d_type == DT_DIR)
        {
            walkThroughDirectory(path, buffer, fileCount);
        }
        else if (currentPath->d_type == DT_REG)
        {
            circular_buffer_enqueue(buffer, path);
            (*fileCount)++;
        }
    }

    if (closedir(dir) == -1)
        ERR("closedir");
}

void *worker(void *args)
{
    worker_args_t *workerArgs = (worker_args_t *)args;
    circular_buffer *buffer = workerArgs->buffer;
    while (true)
    {
        char *item = circular_buffer_dequeue(buffer);
        if (item == NULL || strcmp(item, "") == 0) {
            free(item);
            break;
        }

        fprintf(stderr, "Witam, jestem pracownikiem %ld reprezentuje plik %s\n", workerArgs->tid, strrchr(item, '/') + 1);
        pthread_mutex_lock(workerArgs->mxProcessedCount);
        (*workerArgs->processedCount)++;
        pthread_mutex_unlock(workerArgs->mxProcessedCount);

        free(item);
        //sleep(1);
    }

    return NULL;
}
