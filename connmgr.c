//
// Created by senne on 12/12/22.
//
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>
#include <unistd.h>
#include "connmgr.h"
#include "config.h"
#include "lib/tcpsock.h"
#include "sbuffer.h"
#include "sensor_db.h"

pthread_mutex_t mutex_conn;
pthread_cond_t connection;
tcpsock_t *server;
sbuffer_t *buffer;
int conn_counter;
int result_conn = 0;
int closed_connections = 0;
int signal = 1;
int connections[MAX_CONN];
char* message_conn;
extern char* fifo_file;

struct Printer {
    tcpsock_t *client;
    int bytes, result;
    sensor_data_t data;
};

void* printing(void* args);

void connmgr_listen(int PORT, sbuffer_t *sbuffer){
    buffer = sbuffer;
    pthread_mutex_init(&mutex_conn, NULL);
    pthread_cond_init(&connection, NULL);
    conn_counter = 0;

    pthread_t printers[MAX_CONN];
    pthread_attr_t attr;
    result_conn = pthread_attr_init(&attr);
    if(result_conn != 0){
        printf("Error while initializing attribute");
        exit_on_failure();
    }

    printf("Server is started\n");
    if (tcp_passive_open(&server, PORT) != TCP_NO_ERROR){
        printf("There occured an error while opening the server.\n");
        exit_on_failure();
    }

    // create threadpool of MAX_CONN threads
    for(int i=0; i<MAX_CONN; i++){
        struct Printer *p = malloc(sizeof(struct Printer));
        result_conn = pthread_create(&printers[0], &attr, (void*) printing, p);
        if(result_conn != 0){
            printf("Error while creating thread");
            exit_on_failure();
        }
    }

    pthread_cond_wait(&connection, &mutex_conn);

    if (tcp_close(&server) != TCP_NO_ERROR) exit_on_failure();
    printf("Server is shutting down\n");

    //sending EOF data to notify sbuffer that the last packet is sent.
    sensor_data_t *EOF_data;
    EOF_data = malloc(sizeof(sensor_data_t));
    EOF_data->id = 0;
    EOF_data->value = 0;
    EOF_data->ts = 0;
    sbuffer_insert(sbuffer, EOF_data);
    free(EOF_data);
}

void connmgr_free(){
    tcp_close(&server);
    pthread_mutex_destroy(&mutex_conn);
    pthread_cond_destroy(&connection);
}

void* printing(void* args){
    struct Printer *p = args;
    do {
        if (tcp_wait_for_connection(server, &p->client) != TCP_NO_ERROR){
            printf("There occured an error on tcp_wait_for_connection()\n");
            exit_on_failure();
        }
        printf("Incoming client connection\n");
        int first = 1;
        conn_counter++;
        printf("conn_counter: %d\n", conn_counter);
        do{
            pthread_mutex_lock(&mutex_conn);
            for(int i=0; i<=1; i++){
                p->bytes = sizeof(p->data.id);
                p->result = tcp_receive(p->client, (void *) &(p)->data.id, &p->bytes);
                // read temperature
                p->bytes = sizeof(p->data.value);
                p->result = tcp_receive(p->client, (void *) &(p)->data.value, &p->bytes);
                // read timestamp
                p->bytes = sizeof(p->data.ts);
                p->result = tcp_receive(p->client, (void *) &(p)->data.ts, &p->bytes);
//                pthread_mutex_unlock(&mutex_conn);

                if(conn_counter > MAX_CONN){
                    p->result = TCP_CONNECTION_CLOSED;
                }


                if (first == 1 && p->result != TCP_CONNECTION_CLOSED) {
//                    pthread_mutex_lock(&mutex_conn);
                    asprintf(&message_conn, "Sensor node %hd has opened a new connection\n", p->data.id);
                    fifo_write(message_conn);
                    free(message_conn);
                    first = 0;
                    //add id to connections array
                    for(int k = 0; k<MAX_CONN;k++){
                        if(connections[k] == p->data.id){
                            printf("id: %d is already in use in position %d\n", p->data.id, k);
                            printf("Server closed the connection because id is already in use. First close other client.\n"
                                   "IMPORTANT: This connection does not affect the count of the MAX_CONN.\n");
                            tcp_close(&(p->client));
                            conn_counter--;
                            printf("conn_counter: %d\n", conn_counter);
                            break;
                        }
                        if(connections[k] == 0){
                            connections[k] = p->data.id;
//                            printf("id: %d got added to connections array on position %d\n", p->data.id, k);
                            break;
                        }

                    }
//                    pthread_mutex_unlock(&mutex_conn);
                }
                if ((p->result == TCP_NO_ERROR) && p->bytes) {
//                    pthread_mutex_lock(&mutex_conn);
//                    printf("sensor id = %" PRIu16 " - temperature = %g - timestamp = %ld\n", p->data.id, p->data.value,
//                           (long int) p->data.ts);
                    sbuffer_insert(buffer, &(p->data));
//                    pthread_mutex_unlock(&mutex_conn);
                }
                pthread_mutex_unlock(&mutex_conn);
            }
        } while (p->result == TCP_NO_ERROR);
        if (p->result == TCP_CONNECTION_CLOSED){
            pthread_mutex_lock(&mutex_conn);
            printf("Peer has closed connection\n");
            asprintf(&message_conn, "Sensor node %hd has closed a the connection.\n", p->data.id);
            fifo_write(message_conn);
            free(message_conn);
            for(int k = 0; k<MAX_CONN;k++){
                if(connections[k] == p->data.id){
//                    printf("id: %d got deleted from connections array on position %d\n", p->data.id, k);
                    connections[k] = 0;
                }
            }
            tcp_close(&(p->client));
            closed_connections++;
            printf("closed_connections: %d, you can make %d more connection(s) before the server will shutdown\n",
                   closed_connections, MAX_CONN-closed_connections);
            sleep(2);
            if(closed_connections == MAX_CONN && signal == 1){
                pthread_cond_signal(&connection);
                signal = 0;
            }
            pthread_mutex_unlock(&mutex_conn);
            free(p);
            pthread_exit(0);
        }
        else{
            printf("Error occured on connection to peer\n");
            tcp_close(&(p->client));
        }
    } while (conn_counter < MAX_CONN);
    return NULL;
}
