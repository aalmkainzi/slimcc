// file: pthread_posix_spawn_example.c
// Compile: gcc -std=c11 -Wall -Wextra -pthread pthread_posix_spawn_example.c -o example

_Nameprefix posix = "posix_";
_Nameprefix posix::thread = "pthread_";

_Capture _Nameprefix posix, posix::thread
{
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <errno.h>
    #include <spawn.h>
    #include <sys/wait.h>
    #include <pthread.h>
}

extern char **environ; // required by posix_spawn

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cond  = PTHREAD_COND_INITIALIZER;
static int ready = 0;

void *spawner_thread(void *arg) {
    (void)arg;
    
    pid_t child_pid;
    char *argv[] = { "/bin/echo", "hello from posix_spawn child", NULL };
    
    posix::spawnattr_t attr;
    if (posix::spawnattr_init(&attr) != 0) {
        perror("posix_spawnattr_init");
        return (void*)1;
    }
    
    // spawn child running /bin/echo
    int rc = posix::spawn(&child_pid, argv[0], NULL, &attr, argv, environ);
    posix::spawnattr_destroy(&attr);
    
    if (rc == 0) {
        printf("[thread] posix_spawn succeeded, child pid = %d\n", (int)child_pid);
        int status;
        if (waitpid(child_pid, &status, 0) == -1) {
            perror("waitpid");
        } else {
            if (WIFEXITED(status))
                printf("[thread] child exited with status %d\n", WEXITSTATUS(status));
            else
                printf("[thread] child terminated abnormally\n");
        }
    } else {
        // posix_spawn returns an errno-style value
        fprintf(stderr, "[thread] posix_spawn failed: %s\n", strerror(rc));
    }
    
    // signal main thread that we're done (use pthread_ APIs)
    posix::thread::mutex_lock(&lock);
    ready = 1;
    posix::thread::cond_signal(&cond);
    posix::thread::mutex_unlock(&lock);
    
    return NULL;
}

int pt(void) {
    _Nameprefix pt = posix::thread;
    
    pthread_t th;
    
    if (pt::create(&th, NULL, spawner_thread, NULL) != 0) {
        perror("pthread_create");
        return 1;
    }
    
    // wait until the thread signals (using pthread_cond_wait)
    pt::mutex_lock(&lock);
    while (!ready) {
        pthread_cond_wait(&cond, &lock);
    }
    pt::mutex_unlock(&lock);
    
    printf("[main] thread signaled ready — joining\n");
    
    if (pt::join(th, NULL) != 0) {
        perror("pthread_join");
        return 1;
    }
    
    printf("[main] worker thread joined; exiting\n");
    return 0;
}