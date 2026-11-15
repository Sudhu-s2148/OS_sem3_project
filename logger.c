#include "logger.h"

void run_producer_process(int id, SharedData* shared_data, sem_t* sem_mutex, sem_t* sem_empty, sem_t* sem_full) 
{
    char message[MAX_MSG_LEN];

    //suppose each producer has 3 steps to do
    for (int i = 1; i <= 3; i++) 
    {
        sprintf(message, "Worker: Process %d completed task step %d.", id, i);

        //wait for an empty slot in the queue
        sem_wait(sem_empty);
        
        //lock the queue (mutex)
        sem_wait(sem_mutex);
        
        //critical section
        strcpy(shared_data->log_queue[shared_data->q_tail], message);
        //move q_tail after enqueuing
        shared_data->q_tail = (shared_data->q_tail + 1) % QUEUE_SIZE;
        shared_data->q_count++;

        //unlock the queue
        sem_post(sem_mutex);
        
        //signal that there is a slot available for the logger to read
        sem_post(sem_full);
        
        //simulate 1 second of cpu time
        sleep(1);
    }

    //producer is done, decrement the counter
    sem_wait(sem_mutex);
    shared_data->active_producers--;

    if (shared_data->active_producers == 0) 
    {
        //if this is the last producer, send a dummy 'full' signal 
        //to wake up the logger so it can check the active_producers count and shut down
        sem_post(sem_full); 
    }

    sem_post(sem_mutex);

    exit(0); //kill child process
}

void run_logger_process(SharedData* shared_data, sem_t* sem_mutex, sem_t* sem_empty, sem_t* sem_full) 
{
    while (1) 
    {
        //wait for a slot (message to read)
        sem_wait(sem_full);
        
        //lock the queue
        sem_wait(sem_mutex);
        
        //critical section 
        //check if logger was awoken by dummy 'full' signal
        if (shared_data->q_count == 0 && shared_data->active_producers == 0) 
        {
            sem_post(sem_mutex); //unlock
            break;
        }

        printf("Log: %s\n", shared_data->log_queue[shared_data->q_head]);
        //move q_head after dequeueing
        shared_data->q_head = (shared_data->q_head + 1) % QUEUE_SIZE;
        shared_data->q_count--;
        //critical section end
        
        //unlock the queue
        sem_post(sem_mutex);
        
        //signal that an empty slot opened up for the producers
        sem_post(sem_empty);
    }

    printf("Log: All worker processes finished.\n");
    exit(0);
}

void test_logger() 
{
    printf("Starting concurrent logger\n");

    //cean up stale memory/semaphores from possible previous crashed runs
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_MUTEX_NAME);
    sem_unlink(SEM_EMPTY_NAME);
    sem_unlink(SEM_FULL_NAME);

    //create shared memory
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666); //create new shared memory object (0666 is read/write access to all users)
    ftruncate(shm_fd, sizeof(SharedData)); ////truncate to size of shared data struct
    
    //map it to the program's memory space
    SharedData* shared_data = mmap(0, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    
    //initialize the struct variables
    shared_data->q_head = 0;
    shared_data->q_tail = 0;
    shared_data->q_count = 0;
    shared_data->active_producers = 3;

    //initialize named semaphores
    //sem_mutex starts at 1 (unlocked)
    sem_t* sem_mutex = sem_open(SEM_MUTEX_NAME, O_CREAT, 0666, 1);
    // sem_empty starts at QUEUE_SIZE (5 empty slots)
    sem_t* sem_empty = sem_open(SEM_EMPTY_NAME, O_CREAT, 0666, QUEUE_SIZE);
    // sem_full starts at 0 (0 messages to read)
    sem_t* sem_full = sem_open(SEM_FULL_NAME, O_CREAT, 0666, 0);

    //fork the consumer (logger)
    pid_t logger_pid = fork();
    if (logger_pid == 0) 
    {
        run_logger_process(shared_data, sem_mutex, sem_empty, sem_full);
    }

    //simulate 3 producers
    //fork the producers (workers)
    pid_t producers[3];
    for (int i = 0; i < 3; i++) 
    {
        producers[i] = fork();
        if (producers[i] == 0) 
        {
            run_producer_process(i + 1, shared_data, sem_mutex, sem_empty, sem_full);
        }
    }

    //parent process waits for everyone to finish
    for (int i = 0; i < 3; i++) waitpid(producers[i], NULL, 0);
        waitpid(logger_pid, NULL, 0);

    //cleanup 
    //cleaning up resources
    printf("\nCleaning up resources\n");
    munmap(shared_data, sizeof(SharedData));
    close(shm_fd);
    shm_unlink(SHM_NAME);
    
    sem_close(sem_mutex);
    sem_close(sem_empty);
    sem_close(sem_full);
    sem_unlink(SEM_MUTEX_NAME);
    sem_unlink(SEM_EMPTY_NAME);
    sem_unlink(SEM_FULL_NAME);

    printf("concurrent logger finished\n");
}