#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <wait.h>
#include "connmgr.h"
#include "sbuffer.h"
#include "sensor_db.h"
#include "datamgr.h"

#define THREADS_NUM 3
#define BUFFER_SIZE 100

/* initialize variables */
int port;
int fd;
int result = 0;
int response;
FILE* database_fp;
sbuffer_t* sbuffer;
char* fifo_file = "fifo";
pthread_mutex_t mutex_fifo;
pthread_mutex_t mutex_main;
pthread_cond_t datamgr;


void* connection_manager_thread(void* connection);
void* data_manager_thread(void* data);
void* storage_manager_thread(void* storage);

int main(int argc, char const *argv[]){
    // select correct port number for server
    if(argc == 2){
        port = atoi(argv[1]);
    }
    else{
        printf("You haven't specified which port to use. Please try again.\n");
        printf("./server_gateway <port_number>\n");
        return 0;
    }

    // make a FIFO file (named pipe)
    mkfifo(fifo_file, 0666);

    // fork the process
    pid_t pid;
    pid = fork();

    if(pid < 0){
        printf("Fork Failed\n");
        exit_on_failure();
    }

    if(pid > 0){
        //parent process
        //create a shared buffer
        sbuffer_init(&sbuffer);
        pthread_mutex_init(&mutex_fifo, NULL);
        pthread_mutex_init(&mutex_main, NULL);
        pthread_cond_init(&datamgr, NULL);

        //start logging process
        fifo_write("Start of logging process\n");

        //create different threads
        pthread_t threads[THREADS_NUM];
        pthread_attr_t attr;

        result = pthread_attr_init(&attr);
        if(result != 0){
            printf("Error while initializing attribute");
            exit_on_failure();
        }

        result = pthread_create(&threads[0], 0, (void*) connection_manager_thread, 0);
        if(result != 0){
            printf("Error while creating thread");
            exit_on_failure();
        }
        result = pthread_create(&threads[1], 0, (void*) data_manager_thread, 0);
        if(result != 0){
            printf("Error while creating thread");
            exit_on_failure();
        }
        result = pthread_create(&threads[2], 0, (void*) storage_manager_thread, 0);
        if(result != 0){
            printf("Error while creating thread");
            exit_on_failure();
        }

        pthread_join(threads[0], 0);
        pthread_join(threads[1], 0);
        pthread_join(threads[2], 0);
        sleep(3);

        //close db
        close_db(database_fp);
        printf("Server is shutting down...\n");
        sleep(3);

        //end logging process
        fifo_write("End of logging process\n");

        //wait untill child is done
        wait(NULL);

        //delete the named pipe
        unlink(fifo_file);

        close(fd);

        //free memory
        pthread_mutex_destroy(&mutex_main);
        pthread_mutex_destroy(&mutex_fifo);
        pthread_cond_destroy(&datamgr);
        sbuffer_free(&sbuffer);
        fflush(stdout);
    }

    if(pid == 0){
        // child process
        // logger implementation
        int counter = 0;
        FILE* fifo;
        FILE* log_pointer = fopen("gateway.log", "w");
        char logger_msg[BUFFER_SIZE];
        do{
            pthread_mutex_lock(&mutex_fifo);
            fifo = fopen(fifo_file, "r");
            char* str_result = NULL;

            str_result = fgets(logger_msg, BUFFER_SIZE, fifo);

            if(strcmp(logger_msg, "End of logging process\n") == 0){
                printf("Received: %s", logger_msg);
                fprintf(log_pointer, "%d %lu %s",counter++, (unsigned long)time(NULL), logger_msg);
                fflush(log_pointer);
                printf("Exiting childprocess\n");
                pthread_mutex_unlock(&mutex_fifo);
                break;
            }

            else if(str_result != NULL){
                printf("Received: %s", logger_msg);
                fprintf(log_pointer, "%d %lu %s",counter++, (unsigned long)time(NULL), logger_msg);
                fflush(log_pointer);
            }
            pthread_mutex_unlock(&mutex_fifo);
        } while(1);
        fclose (fifo);
        fclose (log_pointer);
        exit(0);
    }
    return 0;
}

void* connection_manager_thread(void* connection){
    connmgr_listen(port, sbuffer);
    connmgr_free();
    printf("Exiting connmgr...\n");
    pthread_exit(0);
    return NULL;
}

void* data_manager_thread(void* data){
    char* filename = "room_sensor.map";
    FILE *f = fopen(filename, "r");
    datamgr_parse_sbuffer(f, sbuffer);
    datamgr_free();
    printf("Exiting datamgr...\n");
    pthread_exit(0);
    return NULL;
}

void* storage_manager_thread(void* storage){
    char* filename = "data.csv";
    database_fp = open_db(filename, false); // open the db in write mode
    sensor_data_t *data;
    data = malloc(sizeof(sensor_data_t));
    while(1){
        pthread_cond_wait(&datamgr, &mutex_main);
        response = sbuffer_remove(sbuffer, data);
        if(response == SBUFFER_SUCCESS){
            insert_sensor(database_fp, data->id, data->value, data->ts); // insert data inside data.csv file
            fflush(database_fp);
        }
        else{
            printf("Exiting stormgr...\n");
            free(data);
            pthread_exit(0);
        }
    }
    return NULL;
}

