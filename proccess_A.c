#include "proccess.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

//Declare the functions (threads)
void* input_and_save_thread(void* shmempointer);

void* read_memory_and_output_thread(void* shmempointer);

//Initalizew variables for the staticstics that the proccess has to print once exiting (global variables so all threads can access them freely)
int total_received_message_count;
int total_sent_message_count;
int total_package_count;
int total_time_elapsed;

int main(int argc, char *argv[]) 
{
    //Initialize some variables 
    //File descriptor for the shared memory
    int file_descriptor;
     //The path assigned to the shared memory in shm_open
    char* shared_memory_path;
    //A pointer to the share memory
    struct shmbuf* shared_memory_pointer;
    //Set the shared memory path as the string that was inputed through command line
    shared_memory_path = argv[1];
    
    //If there were 3 arguments given set the standard output to be the file descriptor of the text file given
    if(argc > 2)
    {
        //Initialize a char* as the name of the output file
        char* output_file_name = argv[2];
        
        //Open the file and take the output file's file descriptor 
        int output_file_descriptor = open(output_file_name, O_WRONLY | O_CREAT | O_TRUNC, 0666); 

        //Check that the opening was done correctly
        if (output_file_descriptor == -1) {
            errExit("output_file_open");
        }
        
        //Redirect the standard output to the output file
        if (dup2(output_file_descriptor, STDOUT_FILENO) == -1) {
            errExit("output_file_dup_to_std_output");
        }
        
        close(output_file_descriptor);
    }

    //Create the shared memory for the two proccesses and get the file descriptor
    file_descriptor = shm_open(shared_memory_path, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (file_descriptor == -1)
        errExit("shm_open");
    
    //Allocate the size of the shared memory
    if (ftruncate(file_descriptor, sizeof(struct shmbuf)) == -1)
        errExit("ftruncate");

    //Map the shared memory to the pointer that was previously initialized    
    shared_memory_pointer = mmap(NULL, sizeof(*shared_memory_pointer), PROT_READ | PROT_WRITE,
                MAP_SHARED, file_descriptor, 0);
    if (shared_memory_pointer == MAP_FAILED)
        errExit("mmap");

    //Print out the path and address of the shared memory (for debugging)
    printf("Shared memory object \"%s\" has been created at address\"%p\"\n", shared_memory_path, shared_memory_pointer);

    close(file_descriptor);

    //Initialize the proccesse's global variables (accesible by all threads)
    total_received_message_count = 0;
    total_sent_message_count = 0;
    total_package_count = 0;
    total_time_elapsed = 0;

    //Initialize the 5 semaphores as 0
    if (sem_init(&shared_memory_pointer->semaphore1, 1, 0) == -1)
        errExit("sem_init-semaphore1");
    if (sem_init(&shared_memory_pointer->semaphore2, 1, 0) == -1)
        errExit("sem_init-semaphore2");
    if (sem_init(&shared_memory_pointer->semaphore3, 1, 0) == -1)
        errExit("sem_init-semaphore3");
    if (sem_init(&shared_memory_pointer->semaphore4, 1, 0) == -1)
        errExit("sem_init-semaphore4");
    if (sem_init(&shared_memory_pointer->exit_semaphore, 1, 0) == -1)
        errExit("sem_init-exit_semaphore");


    //Initialize 2 threads where one will be responsible from reading data from the standard input and save it in the shared memory
    //And the other will read data from the shared memory and print it in the standard output
    pthread_t input_thread,output_thread;

    //Initialize variable to check if thread creation succeded
    int res;

    //Create a thread to read from the standard input and save to the memory
    res = pthread_create(&input_thread, NULL, input_and_save_thread , (void*)shared_memory_pointer);
    //If the creation was unsuccesful exit with error
    if (res != 0) {
		    perror("Thread creation failed");
		    exit(EXIT_FAILURE);
	}
    //Create a thread to read data from the shared memory and print it in the standard output
    //If any of the threads end and return then it means the proccess should free the shared memory and end itself
    res = pthread_create(&output_thread,NULL,read_memory_and_output_thread , (void*)shared_memory_pointer);

    if (res != 0) {
		    perror("Thread creation failed");
		    exit(EXIT_FAILURE);
	}

    //Wait until either thread ends and posts exit semaphore so the shared memory can be freed
    
    if(sem_wait(&shared_memory_pointer->exit_semaphore)==-1) 
        errExit("sem_wait");
    
    //Print the total number of messages sent and received the total number of packages sent, the avarage number of packages per message sent
    //And the avarage time it took for the first package of each message to be read by the other proccess
    printf("Number of total messages sent: %d, Number of total messages received %d\n",total_sent_message_count,total_received_message_count);
    printf("Total Number of packages sent: %d\n",total_package_count);
    if(total_sent_message_count>0)
        printf("Avarage number of packages sent per message: %f \n",(float)total_package_count/total_sent_message_count);
    if(total_received_message_count>0)
        printf("Avarage time it took first package to arrive (in microseconds): %f \n",(float)total_time_elapsed/total_received_message_count);

    //Cancel both threads because we dont know which one the two caused the exiting to happen
    pthread_cancel(input_thread);
    pthread_cancel(output_thread);

    //Wait until the other proccess finishes using the shared memory and then unlink it 
    shm_unlink(shared_memory_path);

    return 0;
}

void* input_and_save_thread(void* shmempointer) {
    
    //Initalize a pointer to the shared memory segment 
    struct shmbuf* shared_memory_pointer = shmempointer;
    //Initialize a variable to let the thread know when it should end
    int running = 1;
    
    while (running)
    {
        //sleep(1);
    
        //Initiliaze a variable to read the text
        char buffer[BUFFER_SIZE];

        //For use in terminal
        //printf("Enter some text: \n");

        //Read the input through standard input
	    fgets(buffer, BUFFER_SIZE, stdin);
        
        //Get the length of the string given
        int length  = strlen(buffer);

        //Find and save the number of packages of 15 needed to send 
        int package_num =  ceil((float)length / 15.0);
        //Set the length in shared memory as the length of the string
        shared_memory_pointer->lenght = length;
        shared_memory_pointer->package_num = package_num;
        //Initialize a variable to identify each package being send through the shared memory
        //Send each package of 15 through the memory and wait till the semaphore is once again posted to continue sending the next one
        for(int package_id = 0; package_id < package_num; package_id++)
        {   
            //If its not the first package being sent for the current message wait for peer to post semaphore before altering the 
            //contents of the shared memory
            if(package_id > 0)
            {
                //If semaphore1 is = 0 wait until it is posted by peer
                if(sem_wait(&shared_memory_pointer->semaphore1)==-1) 
                    errExit("sem_wait");
            }
            //If it is the first package being sent for the current message dont wait for peer (since the other proccess doesn't have anything
            //to read ) and save the current time in the shared memory so peer can calculate the time it took for that package to arrive
            else
            {
                gettimeofday(&shared_memory_pointer->time_first_package_sent,NULL);
            }
            //Copy the 15 package_id-th characters in the shared memory and tell peer the memory can be read
            memcpy(&shared_memory_pointer->buffer [15*package_id],&buffer [15*package_id ], 15);

            //TESTING
            // printf("\nProccess A: Package id =%d, Packege contents\n", package_id);
            // write(STDERR_FILENO,&shared_memory_pointer->buffer [15*package_id], 15);
            
            //Used to let peer know the increasing number of package that is currently being sent
            shared_memory_pointer->package_id = package_id;
            shared_memory_pointer->package_num = package_num;
            
            //Lets all proccesses (threads) that the current package was writen by A (so that is it now read by the read_memory_and_output
            //thread of the same proccess)
            shared_memory_pointer->writen_by_A = true;

            //Post the semaphore to let peer know it can read the contents of the shared memory
            if(sem_post(&shared_memory_pointer->semaphore2) == -1)
                errExit("sem_post");   

        }
        //Increase the number of sent messages by 1 and the number of sent packages by package_num
        total_sent_message_count++;
        total_package_count += package_num;
        
        //TESTING 
        // printf("Shared memory after Proccess A insertion\n");
        // write(STDOUT_FILENO,&shared_memory_pointer->buffer,shared_memory_pointer->lenght);

        //If the input is the exiting string then set running to zero so the thread ends.
        if(strncmp(buffer,"#BYE#",5) == 0)
            running = 0;

        //Empty out the array created by the function
        for(size_t i=0; i <BUFFER_SIZE; i++)
            buffer[i] = '\0';  
        
    }
    //Post the exit semaphore to let the proccess know it should exit 
    if(sem_post(&shared_memory_pointer->exit_semaphore) == -1)
        errExit("sem_post"); 

    return NULL;
}

void* read_memory_and_output_thread(void* shmempointer) {
    
    //Initialize a variable to let the thread know when it should end
    struct shmbuf* shared_memory_pointer = shmempointer;
    int running = 1;
    
    while (running)
    {       
        //Only attempt to read the shared memory if (1) it has contents and (2) those contents were writen by proccess B
        if((!(shared_memory_pointer->writen_by_A)) && (shared_memory_pointer->lenght > 0)) {

        
            //Initiliaze a variable to read the text from the memory
            char buffer[BUFFER_SIZE];
            
            //Initialize a package id variable to get through the shared memory the increasing number of package being sent
            int package_id;
            //Initialize a variable containing the number of total packages in message so the thread knows when the message has been received
            int package_num;

            while (1) 
            {
                //Wait for peer to write the next package in memory once its done the semaphore will be posted and this thread will continue
                if(sem_wait(&shared_memory_pointer->semaphore4) == -1)
                    errExit("sem_wait");
                
                //Get the package id of the package currently being sent
                package_id = shared_memory_pointer->package_id;
                package_num = shared_memory_pointer->package_num;
                
                //Copy the string from the memory to the buffer varialbe to then print it on standard output
                memcpy(&buffer [15*package_id ], &shared_memory_pointer->buffer[15*package_id] , 15);
                
                //If it is the first package received then calculate how much delay it took to read said package and add it to total elapsed time
                if(package_id == 0)
                {
                    struct timeval current_time;
                    gettimeofday(&current_time,NULL);
                    total_time_elapsed += (int)current_time.tv_usec - (int)shared_memory_pointer->time_first_package_sent.tv_usec;
                }
                //If the package that was just copied was the last one break the loop
                if(package_id == package_num-1)
                    break;
            
                //Post the semaphore to let peer know it can be read once again
                if(sem_post(&shared_memory_pointer->semaphore3) == -1)
                    errExit("sem_post"); 
                
            }

            //TESTING
            // printf("Proccess A received shared memory\n");
            // write(STDOUT_FILENO,&shared_memory_pointer->buffer,shared_memory_pointer->lenght);

            //Increase the toal number of messages received for proccess A by 1.
            total_received_message_count++;
            
            //Once the package is received completly print out the contents of the memory through standard output
            write(STDOUT_FILENO, &buffer, shared_memory_pointer->lenght);

            //Set the length in shared memory as zero to "delete" its contents
            shared_memory_pointer->lenght = 0;
            shared_memory_pointer->package_id=0;

            //If the string read from the memory is the exiting string then set running to zero so the thread ends.
            if(strncmp(buffer, "#BYE#",5) == 0)
                running = 0;
            
             //Empty out the array created by the function
            for(size_t i=0; i < BUFFER_SIZE; i++)
            {
                buffer[i] = '\0';
                shared_memory_pointer->buffer[i] = '\0';
            }
        }  
    }
    //Post the exit semaphore to let the proccess know it should exit 
    if(sem_post(&shared_memory_pointer->exit_semaphore) == -1)
        errExit("sem_post"); 
    
    return NULL;

}
