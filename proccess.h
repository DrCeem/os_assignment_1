#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include <math.h>
#include <sys/time.h>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)

#define BUFFER_SIZE 1024   /* Maximum size for exchanged string */

/* Define a structure that will be imposed on the shared
    memory object */

struct shmbuf {
    //Variable indicating which one of the 2 proccess wrote something in the shared memory: True -> Proccess A, False -> Proccess B
    bool writen_by_A;
    //Semaphore used by Proccess: B thread: read_memory_and_output to let input_and_save_thread of proccess A know that it can access the memory
    sem_t  semaphore1;  
    //Semaphore used by Proccess: A thread: input_and_save_thread to let read_memory_and_output_thread of proccess B know that it can access the memory        
    sem_t  semaphore2;   
    //Semaphore used by Proccess: A thread: read_memory_and_output to let input_and_save_thread of proccess B know that it can access the memory
    sem_t  semaphore3; 
    //Semaphore used by Proccess: B thread: input_and_save_thread to let read_memory_and_output_thread of proccess A know that it can access the memory
    sem_t  semaphore4;    
    //Semaphore used by both proccesses to let proccess know if the exiting string has been inserted in which case it should end the execution
    //and cancel any open threads    
    sem_t  exit_semaphore;
    //The total number of packages that the message currently being sent through the memory was divided into
    size_t package_num;
    //The id of the package that is currenlty being sent (cooresponds to the increasing order number of each package starting from 0)
    int package_id;
    //Stores the time the first package of a message was stored in the shared memory so the time it took to read by the other proccess can be 
    //callculated
    struct timeval time_first_package_sent;
    //The total size of the message being sent
    size_t lenght;
    //The buffer that stores the data in the shared memory
    char   buffer[BUFFER_SIZE];   /* Data being transferred */
};
