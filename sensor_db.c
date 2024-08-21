//
// Created by senne on 11/14/22.
//
#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "sensor_db.h"
#include "config.h"

/* initialize variables */
char* message;
extern char* fifo_file;
extern pthread_mutex_t mutex_fifo;

FILE * open_db(char * filename, bool append){
    FILE* fPointer = NULL;
    if(append == false){
        fPointer = fopen(filename, "w"); //write mode -> file gets overwritten and file pointer goes to place 0.
        asprintf(&message, "A new data.csv file has been created in write mode.\n");
        fifo_write(message);
        free(message);
    }
    else if (append == true){
        fPointer = fopen(filename, "a"); //append mode -> file pointer goes to last char in file
        asprintf(&message, "A new data.csv file has been created in append mode.\n");
        fifo_write(message);
        free(message);
    }
    return fPointer;
}

int insert_sensor(FILE * f, sensor_id_t id, sensor_value_t value, sensor_ts_t ts){
//    printf("reading from sbuffer: %hd,%f,%ld\n", id, value, ts);
    // fprintf returns -1 when an error occurs
    if(fprintf(f, "%hu,%f,%ld \n", id, value, ts) < 0){
        asprintf(&message,"An error occurred when writing to the csv file.\n");
        fifo_write(message);
        free(message);
    }
    else {
        asprintf(&message, "Data insertion from sensor %hd succeeded.\n", id);
        fifo_write(message);
        free(message);
    }
    return 0;
}

int close_db(FILE * f){
    fclose(f);
    asprintf(&message, "the Data.csv file has been closed.\n");
    fifo_write(message);
    free(message);
    return 0;
}

void fifo_write(char* msg){
    FILE* fifo = fopen(fifo_file, "w");
    pthread_mutex_lock(&mutex_fifo);
    fputs(msg, fifo);
    fclose(fifo);
    pthread_mutex_unlock(&mutex_fifo);
}

void exit_on_failure(){
    printf("There was a failure, Please try again\n");
    unlink(fifo_file);
    exit(1);
}

