
_Nameprefix Arwa = "Arwa_";
_Nameprefix Arwa::Math = "Arwa_MATH__";

_Apply _Nameprefix Arwa::Math
{
    
    int foo() { return 10; }
    int bar() { return 10; }
    int baz() { return 15; }
}

_Nameprefix pthread = "pthread_";

_Capture _Nameprefix pthread
{
    #include <pthread.h>
}



void* thread_func(void* arg) {
    int id = *(int*)arg;
    printf("Hello from thread %d\n", id);
    return NULL;
}

int main() {
    
    _Nameprefix pt = pthread;
    
    pthread_t threads[NUM_THREADS];
    int ids[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        ids[i] = i;
        
        if (pt::create(&threads[i], NULL, thread_func, &ids[i]) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pt::join(threads[i], NULL);
    }
    
    printf("All threads finished.\n");
    return 0;
}
