#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "datamgr.h"
#include "lib/dplist.h"
#include "sbuffer.h"
#include "sensor_db.h"


void* element_copy(void*);
void element_free(void**);
int element_compare(void*,void*);

void clear_array();

typedef struct sensors {
    uint16_t sensor_id;
    uint16_t room_id;
    double running_avg;
    time_t last_modified;
    double temperatures[RUN_AVG_LENGTH];
}sensor_t;

/* initialize variables */
int response_read;
int check;
char* message_dmgr;
dplist_t *sensor_list;
extern pthread_cond_t datamgr;
sensor_t *sensor_data;
sensor_data_t *data_dup;

void datamgr_parse_sbuffer(FILE *fp_sensor_map, sbuffer_t *sbuffer) {
    uint16_t room_buffer;
    uint16_t sensor_buffer;

    //create a dplist
    sensor_list = dpl_create(&element_copy, &element_free, &element_compare);
    printf("Start room-sensor mapping\n");
    for (int i = 0; fscanf(fp_sensor_map, "%hu %hu\n", &room_buffer, &sensor_buffer) != EOF; i++) {
        sensor_data = malloc(sizeof(sensor_t));
        if (sensor_data == NULL) {
            printf("error");
        }
        sensor_data->room_id = room_buffer;
        sensor_data->sensor_id = sensor_buffer;
        sensor_data->running_avg = 0;
        sensor_data->last_modified = time(NULL);
//        printf("room: %d sensor: %d\n", room_buffer, sensor_buffer);
        dpl_insert_at_index(sensor_list, sensor_data, i, false);


    }
//    printf("room-sensor mapping is inserted in dplist\n");

    data_dup = malloc(sizeof(sensor_data_t));
    while (response_read == SBUFFER_SUCCESS) {
        response_read = sbuffer_read(sbuffer, data_dup);
        pthread_cond_signal(&datamgr); // give a signal to the storemgr_thread in main.c that data can be deleted from sbuffer
        int c = 0;
        check = VALID;
        for (int i = 0; i < dpl_size(sensor_list); i++) {
            sensor_data = dpl_get_element_at_index(sensor_list, i);
            if (sensor_data->sensor_id == data_dup->id) {
                break;
            } else {
                c++;
            }
        }
        if (c == dpl_size(sensor_list) && data_dup->value != 0) {
            asprintf(&message_dmgr, "Received from invalid sensor node ID %d\n",
                     data_dup->id);
            fifo_write(message_dmgr);
            free(message_dmgr);
            check = INVALID;
        }

        if(check == VALID){
            //put value in temperature array
            for (int i = 0; i < RUN_AVG_LENGTH; i++) {
                if (sensor_data->temperatures[i] == data_dup->value) {
                    break;
                }
                if (sensor_data->temperatures[i] == 0) {
                    sensor_data->temperatures[i] = data_dup->value;
                    break;
                }
            }

            //calculate the running average
            double sum = 0;
            for (int k = 0; k < RUN_AVG_LENGTH; k++) {
                sum += sensor_data->temperatures[k];
            }
            sensor_data->running_avg = sum / RUN_AVG_LENGTH;
            sensor_data->last_modified = data_dup->ts;

            int counter = 0;
            for (int j = 0; j < RUN_AVG_LENGTH; j++) {
                if (sensor_data->temperatures[j] != 0) {
                    counter++;
                }
            }
            if (counter == RUN_AVG_LENGTH) {
                if (sensor_data->running_avg > SET_MAX_TEMP) {
                    asprintf(&message_dmgr, "Temperature: %f in room: %d from sensor: %d is too hot\n",
                             sensor_data->running_avg, sensor_data->room_id, sensor_data->sensor_id);
                    fifo_write(message_dmgr);
                    free(message_dmgr);
                    clear_array();

                } else if (sensor_data->running_avg <= SET_MIN_TEMP) {
                    asprintf(&message_dmgr, "Temperature: %f in room: %d from sensor: %d is too cold\n",
                             sensor_data->running_avg, sensor_data->room_id, sensor_data->sensor_id);
                    fifo_write(message_dmgr);
                    free(message_dmgr);
                    clear_array();

                } else {
                    asprintf(&message_dmgr, "Temperature: %f in room: %d from sensor: %d is ok\n",
                             sensor_data->running_avg, sensor_data->room_id, sensor_data->sensor_id);
                    fifo_write(message_dmgr);
                    free(message_dmgr);
                    clear_array();
                }
            }
        }
    }
    free(data_dup);
    free(sensor_data);
}

void datamgr_free(){
    dpl_free(&sensor_list, false);
}

void* element_copy(void* element){
    sensor_t * sensor = (sensor_t *)element;
    sensor_t * sensorcopy = malloc(sizeof(sensor_t));
    sensorcopy->sensor_id = sensor->sensor_id;
    sensorcopy->room_id= sensor->room_id;
    sensorcopy->running_avg=sensor->running_avg;
    sensorcopy->last_modified= sensor->last_modified;
    for(int i=0; i<RUN_AVG_LENGTH; i++) {
        sensorcopy->temperatures[i]=sensor->temperatures[i];
    }
    return (void*)sensorcopy;
}

void element_free(void** element){
    sensor_t* dummyelement = *element;
    free(dummyelement);
}

int element_compare(void* x, void* y){
 return ((((sensor_data_t*)x)->value < ((sensor_data_t*)y)->value) ? -1 : (((sensor_data_t*)x)->value == ((sensor_data_t*)y)->value) ? 0 : 1);
}

void clear_array(){
    for (int m = 0; m < RUN_AVG_LENGTH; m++) {
        sensor_data->temperatures[m] = 0;
    }
}