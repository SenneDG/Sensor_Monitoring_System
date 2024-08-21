/**
 * \author Senne De Greef
 */

#include <stdlib.h>
#include <pthread.h>
#include "sbuffer.h"

/**
 * basic node for the buffer, these nodes are linked together to create the buffer
 */
typedef struct sbuffer_node {
    struct sbuffer_node *next;  /**< a pointer to the next node*/
    sensor_data_t data;         /**< a structure containing the data */
} sbuffer_node_t;

/**
 * a structure to keep track of the buffer
 */
struct sbuffer {
    sbuffer_node_t *head;       /**< a pointer to the first node in the buffer */
    sbuffer_node_t *tail;       /**< a pointer to the last node in the buffer */
};

/* variables */
int done = 0;
pthread_mutex_t mutex;
pthread_cond_t dataavailable;

int sbuffer_init(sbuffer_t **buffer) {
    *buffer = malloc(sizeof(sbuffer_t));
    if (*buffer == NULL) return SBUFFER_FAILURE;
    (*buffer)->head = NULL;
    (*buffer)->tail = NULL;
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&dataavailable, NULL);
    return SBUFFER_SUCCESS;
}

int sbuffer_free(sbuffer_t **buffer) {
    sbuffer_node_t *dummy;
    if ((buffer == NULL) || (*buffer == NULL)) {
        return SBUFFER_FAILURE;
    }
    while ((*buffer)->head) {
        dummy = (*buffer)->head;
        (*buffer)->head = (*buffer)->head->next;
        free(dummy);
    }
    free(*buffer);
    *buffer = NULL;
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&dataavailable);
    return SBUFFER_SUCCESS;
}


int sbuffer_read(sbuffer_t *buffer, sensor_data_t *data){
    if (buffer == NULL){
        return SBUFFER_FAILURE;
    }
    pthread_mutex_lock(&mutex);
    if (buffer->head == NULL){
        while(buffer->head == NULL && !done) pthread_cond_wait(&dataavailable, &mutex);
        if(done){
            pthread_cond_signal(&dataavailable);
            pthread_mutex_unlock(&mutex);
            return SBUFFER_NO_DATA;
        }
    }

    *data = buffer->head->data;

    if(data->id == 0){
        done = 1;
        pthread_cond_signal(&dataavailable);
        pthread_mutex_unlock(&mutex);
        return SBUFFER_EOF;
    }

    pthread_cond_signal(&dataavailable);
    pthread_mutex_unlock(&mutex);
    return SBUFFER_SUCCESS;
}

int sbuffer_remove(sbuffer_t *buffer, sensor_data_t *data) {
    sbuffer_node_t *dummy;
    if (buffer == NULL){
        return SBUFFER_FAILURE;
    }
    pthread_mutex_lock(&mutex);
    if (buffer->head == NULL){
        while(buffer->head == NULL && !done) pthread_cond_wait(&dataavailable, &mutex);
        if(done){
            pthread_cond_signal(&dataavailable);
            pthread_mutex_unlock(&mutex);
            return SBUFFER_NO_DATA;
        }
    }

    *data = buffer->head->data;

    dummy = buffer->head;
    if (buffer->head == buffer->tail) // buffer has only one node
    {
        buffer->head = buffer->tail = NULL;
    } else  // buffer has many nodes empty
    {
        buffer->head = buffer->head->next;
    }
    free(dummy);

    if(data->id == 0){
        done = 1;
        pthread_cond_signal(&dataavailable);
        pthread_mutex_unlock(&mutex);
        return SBUFFER_EOF;
    }

    pthread_cond_signal(&dataavailable);
    pthread_mutex_unlock(&mutex);
    return SBUFFER_SUCCESS;
}

int sbuffer_insert(sbuffer_t *buffer, sensor_data_t *data) {
    sbuffer_node_t *dummy;
    if (buffer == NULL) return SBUFFER_FAILURE;
    dummy = malloc(sizeof(sbuffer_node_t));
    if (dummy == NULL) return SBUFFER_FAILURE;
    dummy->data = *data;
    dummy->next = NULL;
    if (buffer->tail == NULL) // buffer empty (buffer->head should also be NULL
    {
        buffer->head = buffer->tail = dummy;
    } else // buffer not empty
    {
        buffer->tail->next = dummy;
        buffer->tail = buffer->tail->next;
    }
//    printf( "writing: %hd,%lf,%ld \n", (uint16_t )data->id, (double)data->value, (time_t)data->ts);
    pthread_cond_signal(&dataavailable);
    return SBUFFER_SUCCESS;
}
