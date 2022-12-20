/**
 * \author {Jeffee Hsiung}
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>
#include <pthread.h>
#include <aio.h>
#include "connmgr.h"
#include "datamgr.h"
#include "sensor_db.h"
#include "sbuffer.h"



void print_help(void);
// void set_termintate(bool term);
// bool get_terminate(void);

/* global varaibles */
int fd[2]; 
sem_t pipe_lock;
sbuffer_t* buffer;


int main(int argc, char *argv[]){

    pid_t pid;
    int server_port;
    bool terminate = false;

    if (argc != 2) {
        print_help();
        exit(EXIT_SUCCESS);
    } else {
        server_port = atoi(argv[1]);
    }
	// create the pipe
	if (pipe(fd) == -1) {
		perror("pipe creation failed\n"); exit(EXIT_FAILURE);
	}

	// fork a child process
    pid = fork();
    if (pid < 0){
        perror("Fork failed.\n"); exit(EXIT_FAILURE);
    }

	// parent process: main process
	if (pid > 0){

        int totalthread = 0;
        pthread_t threads[MAX_RD + MAX_WRT];

        if (sem_init(&pipe_lock, 0, 1) == -1){
            perror("sem_init failed\n"); exit(EXIT_FAILURE);
        }

        if (sbuffer_init(&buffer) != SBUFFER_SUCCESS){
            perror("sbuffer_init failed\n"); exit(EXIT_FAILURE);
        }

        close(fd[READ_END]);

        pthread_create(&threads[totalthread],NULL,connmgr_start,(void*) &server_port); totalthread++; 
        pthread_create(&threads[totalthread],NULL,datamgr_start,(void*) buffer); totalthread++;
        pthread_create(&threads[totalthread],NULL,sensor_db_start,(void*) buffer); totalthread++;

        
        while (totalthread >  0) {
            if(pthread_join(threads[totalthread-1],NULL) != 0){
                perror("main: failed to detach thread \n"); exit(EXIT_FAILURE);
            }else{
                totalthread--;
                printf(" main: threads in main left active: %d:\n",totalthread);
            }
        }

        printf("main: all threads terminated\n");
        //set_termintate(true);
        terminate = true;

        sbuffer_free(&buffer);
        printf("main: sbuffer freed\n");

        wait(NULL);

        printf("main: logger process terminated\n");

	    close(fd[WRITE_END]);

        sem_destroy(&pipe_lock);

        return 0;
        
        exit(EXIT_SUCCESS);

    }
	// child process: log process
    else
    {
        close(fd[WRITE_END]);

        int seq_num = 0;

        while (terminate == false)
        {

            FILE *log = fopen("gateway.log", "a+");
            if (log == NULL)
            {
                perror("logger opening file failed\n");
                exit(EXIT_FAILURE);
            }

            // read from the pipe
            char read_msg[BUFF_SIZE];
            int bytes_read = read(fd[READ_END], read_msg, sizeof(read_msg));
            if (bytes_read == -1)
            {
                perror("logger reading from pipe failed\n");
                exit(EXIT_FAILURE);
            }
            if (bytes_read == 0)
            {
                printf("logger read 0 bytes from pipe\n");
                break;
            }

            // get the message from pipe, add a sequence number and a timestamp to it
            // and write an ASCII message to the log file in the format: <seq_num> <timestamp> <message>
            time_t t;
            time(&t);
            fprintf(log, "%d %s %s", seq_num, ctime(&t), read_msg);
            fflush(log);

            seq_num++;


            if (fclose(log) != 0)
            {
                perror("logger closing file falied\n");
                exit(EXIT_FAILURE);
            }

        } 

        close(fd[READ_END]);

        printf("logger process terminated\n");

        exit(EXIT_SUCCESS);
    }
    
    return 0;
}

/**
 * Helper method to print a message on how to use this application
 */
void print_help(void) {
    printf("Use this program with 2 command line options: \n");
    printf("\t%-15s : TCP server port number\n", "\'server port\'");
}

// void set_termintate(bool term){
//     terminate = term;
// }

// bool get_terminate(void){
//     return terminate;
// }



