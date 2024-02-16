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
job_node *get_last_job_from_queue() {
    job_node *last = head;
    while (last->next != NULL){
        last = last->next;
    }
    return last;
}

insert_job_to_queue (char line[MAX_LINE_LENGTH])
{
    job_node *last = get_last_job_from_queue();
    job_node *new_node = (job_node*)malloc(sizeof(job_node));
    if (new_node == NULL){
        // Couldn't allocate memory for a new job_node
        printf("Couldn't allocate memory for a new job_node");
        exit(-1);
    }
    new_node->next = NULL;
    strcpy(new_node->text, line);
    last->next = new_node;
}

void execute_command(char *command){
    printf("command received: %s\n", command);
}

void parse_worker_line(char *line, int thread_id){
    char *line_ptr, *cmd_ptr, *rmn_ptr, *command;
    char *temp_line;
    char *remaining_line=line; //initially remain equals to the whole line argument

    // if (log_handler == 1) {
    //     //write to thread_file[thread_id]
    // }

    line_ptr = strtok_r(line, ";", &remaining_line); //break the line by semicolons
    while (line_ptr != NULL)
    {
        if (strstr(line_ptr, "repeat"))
        {
            char *integers = "1234567890";
            char *num = strpbrk(line_ptr, integers);
            temp_line = strdup(remaining_line);
            strcpy(temp_line, remaining_line);
            char *origin = strdup(remaining_line); //to keep track on the original line
            command = strtok_r(temp_line, ";",&remaining_line);
            for (int i = 0; i<atoi(num); i++){
                while (command != NULL){
                    execute_command(command);
                    //printf("current command: %s\n", command);
                    command = strtok_r(NULL, ";", &remaining_line);
                }
                if (i == 0) {
                    command = strdup(origin);
                    cmd_ptr = command; //keep track on original command address
                    remaining_line = strdup(origin);
                    rmn_ptr = remaining_line; //keep track on original remain address
                }
                else {
                    command = cmd_ptr; //rewind command to the begining
                    remaining_line = strcpy(remaining_line,command); //copy command to remain to start over
                }
                command = strtok_r(NULL, ";", &remaining_line);                                
            }
            //command = strtok_r(NULL, ";", &remain);
            free(temp_line);
            free(origin);
            free(rmn_ptr);
            command = cmd_ptr;
            free(command);
        }
        execute_command(line_ptr);
        line_ptr = strtok_r(NULL, ";", &remaining_line);
    }
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

