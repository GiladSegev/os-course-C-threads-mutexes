#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#define MAX_THREADS_NUM 4096
#define MAX_COUNTER_FILES 100
#define MAX_LINE_LENGTH 1024

//Each thread created will have an index as an id and end_time variable to store the time it finished working
typedef struct thread_data {
    int thread_id;
    long long end_time;
} thread_data;

//Jobs will be implemented as a linked list. each job will have text field to store the command it needs to do.
typedef struct job_node {
    char text[MAX_LINE_LENGTH];
    struct job_node *next;
} job_node;

pthread_mutex_t mutex; //mutex for job queue
pthread_mutex_t file_mutexes[MAX_COUNTER_FILES];
pthread_cond_t job_available = PTHREAD_COND_INITIALIZER;
//**May need another condition to signal the dispatcher to wait**//

int num_of_threads,num_of_files, log_handler;
job_node *head; //will serve as the head of the job queue
int busy[MAX_THREADS_NUM] = {0}; //each cell in this list signals if thread[i] is working or not. For use of the dispatcher_wait command.
FILE **threads_logfiles, *dispatcher_file;

FILE *create_dispatcher_file(){
    dispatcher_file = fopen("dispatcher.txt", "w+");
    if (dispatcher_file == NULL) {
        printf("failed creating dispatcher.txt.\n");
        exit(-1);
    }
    return dispatcher_file;
}

FILE **create_thread_files(){
    static FILE *thread_files[MAX_THREADS_NUM];
    char file_name[13];

    for (int i=0; i<num_of_threads; i++){
        if (i<10){
            snprintf(file_name, 13, "thread0%d.txt",i);
        }
        else {
            snprintf(file_name, 13, "thread%d.txt", i);
        }
        thread_files[i] = fopen(file_name, "w+");
        if (thread_files[i] == NULL){
            printf("Failed in creating thread file %d.\n", i);
            exit(1);
        }
        fputs("0\0", thread_files[i]);
        rewind(thread_files[i]);
    }
    return thread_files;
}

//On initialization call this function to create requiered number of counter files.
FILE **create_counter_files(int num_counter){
    static FILE *counter_files[MAX_COUNTER_FILES];
    char file_name[12];
    for (int i=0; i < num_counter;i++){
        if (i < 10){
            snprintf(file_name, 12, "count0%d.txt",i);
        }
        else {
            snprintf(file_name, 12, "count%d.txt", i);
        }
        counter_files[i] = fopen(file_name, "w+");
        if (counter_files[i] == NULL){
            printf("Failed in creating counter file %d.\n", i);
            exit(1);
        }
        fputs("0\0", counter_files[i]);
        rewind(counter_files[i]);
    }
    return counter_files;
}

//This function iterates over the nodes in the queue and returns the last node which holds the last job in the queue
job_node *get_last_job_from_queue(job_node *head) {
    job_node *last = head;
    while (last->next != NULL){
        last = last->next;
    }
    return last;
}

//This function receives the thread_data struct as an argument and handles this specific thread work load
void *works_func(void *arg){
    char line[MAX_LINE_LENGTH];
    //might need original line for log_handler
    char original_line[MAX_LINE_LENGTH];
    thread_data *td = (thread_data *)arg;
    int thread_id = td->thread_id;

    while(1)
    {
        pthread_mutex_lock(&mutex);
        while(head == NULL) { //while there are no jobs available go to sleep.
            pthread_cond_wait(&job_available, &mutex);
        }
        //if we reach here meaning OS woke the thread and there is a job available
        busy[thread_id] = 1;
        job_node *first_node = head;
        head = head->next;
        //WE NEED TO EXECUTE FROM THE FIRST JOB TO LAST. SO IN THE DISPACHER NEED TO ALWAYS MOVE "HEAD" TO  BE LAST  
        strcpy(line, first_node->text);
        strcpy(original_line, line);
        free(first_node); //remove last job from the queue so no other thread will take it, and execute it.
        pthread_mutex_unlock(&mutex);
        parse_worker_line(line, thread_id);

        //if(log_handler ==1) {}
        //Need to handle info for stats.txt

        busy[thread_id] = 0;
        //pthread_cond_signal(&dispatcher_wait);
    }


}

//On initialization call this function to create requiered number of threads.
void init_threads(int num_threads){
    pthread_t tid;
    thread_data td;

    for (int i=0; i<num_threads; i++){
        td.thread_id = i;
        pthread_create(&tid, NULL, works_func, (void *) &td);
        printf("Hello from thread %ld, index %d\n", tid, i);        
    }
    if (log_handler == 1) {
        threads_logfiles = create_thread_files();
        dispatcher_file = create_dispatcher_file();
    }
}

//call this function at the end of the program to destroy all mutexes.
void destroy_all_mutexes(){
    pthread_mutex_destroy(&mutex);
    for (int i = 0; i < MAX_COUNTER_FILES ; i++){
        pthread_mutex_destroy(&file_mutexes[i]);
    }
}

void terminate_program(FILE *command_file, FILE **counter_files_array, int num_of_files)
{
    // Destroy all created mutexes
    destroy_all_mutexes();
    // Close all files opened
    fclose(command_file);
    for (int i=0 ; i < num_of_files ; i++)
    {
        fclose(counter_files_array[i]);
    }
    // NEED TO SEE IF NEED TO FREE ALSO JOBS LINKED LIST
}

