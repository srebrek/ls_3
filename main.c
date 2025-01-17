#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "circular_buffer.h"
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define MAX_LENGTH 256
#define LETTERS 52

typedef unsigned int UINT;

typedef struct timespec timespec_t;

typedef struct worker_args {
    pthread_t tid;
    circular_buffer *buffer;
    pthread_mutex_t *mxProcessedCount;
    int *processedCount;
    int *letterCount;
    pthread_mutex_t *mxLetterCount;
} worker_args_t;

typedef struct manager_args {
    pthread_t tid;
    int *letterCount;
    pthread_mutex_t *mxLetterCount;
    sigset_t *mask;
} manager_args_t;

void readArguments(int argc, char **argv, char *workingDirPath, int *threadCount);
void walkThroughDirectory(const char *dirPath, circular_buffer *buffer, int *fileCount);
void *worker(void *args);
void countLetters(const char *path, int *letterCount, pthread_mutex_t *mxLetterCount);
void *manager(void *args);
void msleep(UINT milisec);

int main(int argc, char *argv[]) {
    int threadCount;
    char workingDirPath[MAX_LENGTH];
    readArguments(argc, argv, workingDirPath, &threadCount);
    printf("Working directory: %s\n", workingDirPath);
    printf("Thread count: %d\n", threadCount);

    worker_args_t *workersArgs = malloc(threadCount * sizeof(worker_args_t));
    if (workersArgs == NULL)
        ERR("malloc");

    int letterCount[LETTERS] = {0};
    pthread_mutex_t mxLetterCount[LETTERS] = PTHREAD_MUTEX_INITIALIZER;
    circular_buffer *buffer = circular_buffer_init();
    int results = 0;
    int fileCount = 0;
    pthread_mutex_t mxResults = PTHREAD_MUTEX_INITIALIZER;

    for (int i = 0; i < threadCount; i++)
    {
        workersArgs[i].buffer = buffer;
        workersArgs[i].mxProcessedCount = &mxResults;
        workersArgs[i].processedCount = &results;
        workersArgs[i].letterCount = letterCount;
        workersArgs[i].mxLetterCount = mxLetterCount;
    }

    sigset_t oldMask, newMask;
    sigemptyset(&newMask);
    sigaddset(&newMask, SIGUSR1);
    if (pthread_sigmask(SIG_BLOCK, &newMask, &oldMask))
        ERR("SIG_BLOCK error");

    for (int i = 0; i < threadCount; i++)
    {
        if (pthread_create(&workersArgs[i].tid, NULL, worker, &workersArgs[i]) != 0)
            ERR("pthread_create");
    }

    manager_args_t *managerArgs = malloc(sizeof(manager_args_t));
    if (managerArgs == NULL)
        ERR("malloc");

    managerArgs->letterCount = letterCount;
    managerArgs->mxLetterCount = mxLetterCount;
    managerArgs->mask = &newMask;
    
    if (pthread_create(&managerArgs->tid, NULL, manager, managerArgs) != 0)
        ERR("pthread_create");

    walkThroughDirectory(workingDirPath, buffer, &fileCount);

    printf("chodzenie skonczone\n");

    while (true)
    {
        if (kill(getpid(), SIGUSR1) == -1)
            ERR("kill");
        pthread_mutex_lock(&mxResults);
        if (results == fileCount)
        {
            pthread_mutex_unlock(&mxResults);
            break;
        }
        pthread_mutex_unlock(&mxResults);
        msleep(100);
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

    // for (int i = 0; i < LETTERS; i++) {
    //     printf("%c: %d\n", i < 26 ? 'a' + i : 'A' + i - 26, letterCount[i]);
    // }

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
        countLetters(item, workerArgs->letterCount, workerArgs->mxLetterCount);
        fprintf(stderr, "Pracownik %ld zakonczyl prace nad plikiem %s\n", workerArgs->tid, strrchr(item, '/') + 1);
        pthread_mutex_lock(workerArgs->mxProcessedCount);
        (*workerArgs->processedCount)++;
        pthread_mutex_unlock(workerArgs->mxProcessedCount);

        free(item);
        //sleep(1);
    }

    return NULL;
}

void countLetters(const char *path, int *letterCount, pthread_mutex_t *mxLetterCount)
{
    int fd = open(path, O_RDONLY);
    if(fd == -1)
        ERR("Cannot open file");

    char buffer;
    ssize_t bytesRead;
    while ((bytesRead = read(fd, &buffer, 1)) > 0) {
        int letter = buffer - 'a';
        if (letter >= 0 && letter < 26) {
            pthread_mutex_lock(&mxLetterCount[letter]);
            letterCount[letter]++;
            pthread_mutex_unlock(&mxLetterCount[letter]);
        }
        msleep(10);
    }

    if (bytesRead == -1)
        ERR("read");

    if (close(fd) == -1)
        ERR("close");
}

void *manager(void *args)
{
    printf("Manager started\n");
    manager_args_t *managerArgs = (manager_args_t *)args;
    while (true)
    {
        printf("Manager is waiting for SIGUSR1\n");
        int sig;
        if (sigwait(managerArgs->mask, &sig))
            ERR("sigwait");
        if (sig == SIGUSR1)
        {
            printf("Manager received SIGUSR1\n");
            for (int i = 0; i < LETTERS; i++)
            {
                pthread_mutex_lock(&managerArgs->mxLetterCount[i]);
            }
            for (int i = 0; i < LETTERS; i++)
            {
                printf("%c: %d\n", i < 26 ? 'a' + i : 'A' + i - 26, managerArgs->letterCount[i]);
            }
            for (int i = 0; i < LETTERS; i++)
            {
                pthread_mutex_unlock(&managerArgs->mxLetterCount[i]);
            }
        }
    }

    return NULL;
}

void msleep(UINT milisec)
{
    time_t sec = (int)(milisec / 1000);
    milisec = milisec - (sec * 1000);
    timespec_t req = {0};
    req.tv_sec = sec;
    req.tv_nsec = milisec * 1000000L;
    if (nanosleep(&req, &req))
        ERR("nanosleep");
}