//
// Created by senne on 11/14/22.
//

#ifndef SENSOR_DB_H
#define SENSOR_DB_H

#define BUFFER_SIZE 100

#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include <stdbool.h>
#include "sbuffer.h"


/**
 * an operation to open a text file with a given name, and providing an indication
 * if the file should be overwritten if the file already exists or if the data should
 * be appended to the existing file.
 */
FILE * open_db(char * filename, bool append);

/**
 * an operation to append a single sensor reading to the csv file
 */
int insert_sensor(FILE * f, sensor_id_t id, sensor_value_t value, sensor_ts_t ts);

/**
 * an operation to close the csv file
 */
int close_db(FILE * f);

/**
 * Opens a named pipe (fifo) file and writes the message that is given as attribute in the file.
 * Than the fifo file gets closed again.
 * \param msg a message that will be put inside the named pipe.
 * */
void fifo_write(char* msg);

/**
 * Ensures a clean termination of the program. Closes the fifo file and exit the program.
 * */
void exit_on_failure();

#endif //SENSOR_DB_H
