#include <stdio.h>
#include <stdlib.h>
#include "hw2_205356793_312613169.h"


int main(int argc, char **argv) {

    FILE *read_commands_file; // Init file to store command text file from user
    FILE **counter_files_array; // Init counter files array
    char line[MAX_LINE_LENGTH]; // Init string var to get the commands in the commands file line by line

    // User input validity check. Check all necessary files from user were inputted
    if (argc!=5){
        printf("Error: incompatible number of arguments.\n");
        exit(-1);
    }
    int num_of_threads = atoi(argv[2]);
    printf("received num of threads: %d\n", num_of_threads);
    if (num_of_threads > MAX_THREADS_NUM){
        printf("num_of_threads exceeds max number. Exit program");
        exit(-1);
    }
    int num_of_files = atoi(argv[3]);
    printf("received num of files: %d\n", num_of_files);
    if (num_of_threads > MAX_COUNTER_FILES){
        printf("num_of_files exceeds max number. Exit program");
        exit(-1);
    }
    int log_handler = atoi(argv[4]);
    printf("received log handler: %d\n",log_handler);

    // Read commands file from user input.
    read_commands_file = fopen(argv[1],"r");
    printf("opened %s!\n",argv[1]);
    if (read_commands_file == NULL){
        printf("Error: Could not open commands file.\n");
        exit(-1);
    }
    printf("Success opening commnds file and taking arguments!\n");

    // Handle case where all arguments are 0
    fseek( read_commands_file, 0, SEEK_END); // Go to read_commands_file end of file_mutexes
    int user_command_file_size = ftell(read_commands_file); // Tell the current position in read_command_file. If 0, the file is empty.
    rewind(read_commands_file);
    if (user_command_file_size == 0 && num_of_files == 0 && num_of_threads == 0){
        printf("Run command input files are empty, exiting program.");
        return 0;
    }

    // Create num_of_files counter files and store in counter_files_array
    counter_files_array = create_counter_files(num_of_files);

    // Init num_of_threads threads
    init_threads(num_of_threads, log_handler);

    // Initializing mutex for jobs linked list. REMEBER TO DESTROY
    if (pthread_mutex_init(&mutex, NULL)!=0){
        printf("Mutex initialization failed.\n");
        return 1;
    }

    // Initializing num_of_files mutexes for counter files memory access synchornization.
    for (int i=0; i< num_of_files; i++){
        if (pthread_mutex_init(&file_mutexes[i], NULL)!=0){
            printf("File mutex %d initialization failed.\n", i);
            return 1;
        }
    }

    // Dispatcher 
    char *word;
    int is_worker, i;
    int mili_sec = 0;
    int wait;
    while (fgets(line, sizeof(line), read_commands_file) != NULL)
    {
        // Checking a if the line starts with 'worker'
        word = strtok(line, " "); // readeing first word in the command
        while (word != NULL)
        {
            if (strcmp(word, "worker") == 0){ // in case of worker command line
                // Sending the command line to designated worker function
                worker(line); // IN GENERAL
            } 
            else{ // in case of a dispatcher command line
                if (strcmp(word, "dispatcher_msleep") == 0){
                    word = strtok(NULL, " ");
                    mili_sec = atoi(word)/1000;
                    sleep(mili_sec);
                }
                else if (strcmp(word, "dispatcher_wait") == 0){
                    do{
                        wait = 0;
                        for (i = 0 ; i < num_of_threads ; i++)
                        {
                            if (busy[i] == 1){
                                wait = 1;
                            }
                        }
                    }
                    while(wait == 1);
                }
            }
        }
    }

    // Terminate run program
    terminate_program(read_commands_file, counter_files_array, num_of_files); 
    return 0;
}