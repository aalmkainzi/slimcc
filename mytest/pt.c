#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_THREADS 4

void* thread_func(void* arg) {
    int id = *(int*)arg;
    printf("Hello from thread %d\n", id);
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    int ids[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        ids[i] = i;
        
        if (pthread_create(&threads[i], NULL, thread_func, &ids[i]) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("All threads finished.\n");
    return 0;
}