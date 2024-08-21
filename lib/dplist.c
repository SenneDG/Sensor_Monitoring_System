/**
 * \author Senne De Greef
 */
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "dplist.h"


/*
 * definition of error codes
 * */
#define DPLIST_NO_ERROR 0
#define DPLIST_MEMORY_ERROR 1 // error due to mem alloc failure
#define DPLIST_INVALID_ERROR 2 //error due to a list operation applied on a NULL list

#ifdef DEBUG
#define DEBUG_PRINTF(...) 									                                        \
        do {											                                            \
            fprintf(stderr,"\nIn %s - function %s at line %d: ", __FILE__, __func__, __LINE__);	    \
            fprintf(stderr,__VA_ARGS__);								                            \
            fflush(stderr);                                                                         \
                } while(0)
#else
#define DEBUG_PRINTF(...) (void)0
#endif


#define DPLIST_ERR_HANDLER(condition, err_code)                         \
    do {                                                                \
            if ((condition)) DEBUG_PRINTF(#condition " failed\n");      \
            assert(!(condition));                                       \
        } while(0)


/*
 * The real definition of struct list / struct node
 */


struct dplist_node {
    dplist_node_t *prev, *next;
    void *element;
};

struct dplist {
    dplist_node_t *head;
    void *(*element_copy)(void *src_element);
    void (*element_free)(void **element);
    int (*element_compare)(void *x, void *y);
};


dplist_t *dpl_create(// callback functions
        void *(*element_copy)(void *src_element),
        void (*element_free)(void **element),
        int (*element_compare)(void *x, void *y)
) {
    dplist_t *list;
    list = malloc(sizeof(struct dplist));
    DPLIST_ERR_HANDLER(list == NULL, DPLIST_MEMORY_ERROR);
    list->head = NULL;
    list->element_copy = element_copy;
    list->element_free = element_free;
    list->element_compare = element_compare;
    return list;
}

void dpl_free(dplist_t **list, bool free_element){
    if(list == NULL || *list == NULL){
        return;
    }
    dplist_node_t * list_node = (*list)->head;
    dplist_node_t * tempNode;

    while (list_node) {
        tempNode = list_node;
        list_node = list_node->next;
        if (free_element == true) {
            (*list)->element_free((void*)tempNode->element);
        }
        free(tempNode);
    }
    *list = NULL;
    free(*list);
}

dplist_t *dpl_insert_at_index(dplist_t *list, void *element, int index, bool insert_copy) {
    dplist_node_t *ref_at_index, *list_node;
    if (list == NULL) return NULL;
    DPLIST_ERR_HANDLER(list == NULL, DPLIST_INVALID_ERROR);
    list_node = malloc(sizeof(dplist_node_t));
    DPLIST_ERR_HANDLER(list_node == NULL, DPLIST_MEMORY_ERROR);
    list_node->element = &element;

    // pointer drawing breakpoint
    if (list->head == NULL) { // covers case 1
        if(insert_copy==true){
            list_node->element = list->element_copy(element);
        }
        else{
            list_node->element = element;
        }
        list_node->prev = NULL;
        list_node->next = NULL;
        list->head = list_node;
        // pointer drawing breakpoint
    }
    else if (index <= 0) { // covers case 2
        if(insert_copy==true){
            list_node->element = list->element_copy(element);
        }
        else{
            list_node->element = element;
        }
        list_node->prev = NULL;
        list_node->next = list->head;
        list->head->prev = list_node;
        list->head = list_node;
        // pointer drawing breakpoint
    }
    else {
        ref_at_index = dpl_get_reference_at_index(list, index);
        // pointer drawing breakpoint
        if (index < dpl_size(list)) { // covers case 4
            if(insert_copy==true){
                list_node->element = list->element_copy(element);
            }
            else{
                list_node->element = element;
            }
            list_node->prev = ref_at_index->prev;
            list_node->next = ref_at_index;
            ref_at_index->prev->next = list_node;
            ref_at_index->prev = list_node;
            // pointer drawing breakpoint
        } else { // covers case 3
            if(insert_copy==true){
                list_node->element = list->element_copy(element);
            }
            else{
                list_node->element = element;
            }
            assert(ref_at_index->next == NULL);
            list_node->next = NULL;
            list_node->prev = ref_at_index;
            ref_at_index->next = list_node;
            // pointer drawing breakpoint
        }
    }
    return list;
}

dplist_t *dpl_remove_at_index(dplist_t *list, int index, bool free_element) {
    DPLIST_ERR_HANDLER(list == NULL, DPLIST_INVALID_ERROR);
    dplist_node_t * list_node;
    list_node = list->head;
    //case 1: no elements in the list
    if(list->head == NULL){
        return(NULL);
    }
        //case 2: index is first element in the list
    else if(index == 0){
        list_node = list->head;
        list_node->next->prev = NULL;
        list->head = list->head->next;
        if(free_element == true)
        {
            list->element_free(&list_node->element);
        }
        free(list_node);
        return(list);
    }
        //case 3: index is last element
    else if(index == dpl_size(list)){
        while(list->head->next != NULL){
            list_node = list->head->next; //find the last node in the list
        }
        list_node->prev->next = NULL;
        if(free_element == true)
        {
            list->element_free(&list_node->element);
        }
        free(list_node);
        return(list);
    }
        //case 4: index is middle element
    else{
        for(int i = 0; i < index; i++){
            list_node = list->head->next;
        }
        list_node->prev->next = list_node->next;
        list_node->next->prev = list_node->prev;
        if(free_element == true)
        {
            list->element_free(&list_node->element);
        }
        free(list_node);
        return(list);
    }
}

int dpl_size(dplist_t *list) {
    if (list == NULL) return 0;
    dplist_node_t * list_node;
    list_node = list->head;
    int counter=1; //counts everytime the list_node moves 1 node
    while(list_node->next != NULL) //goes through whole the list
    {
        list_node = list_node->next;
        counter++;
    }
    return counter;
}

void *dpl_get_element_at_index(dplist_t *list, int index) {
    if(list == NULL){
        return NULL;
    }
    dplist_node_t * ref_at_index, *list_node;
    list_node = list->head;
    if (list->head == NULL ) return (void *)0;
    else if(index<=0)
    {
        index=0;
    }
    else if(index>= dpl_size(list))
    {
        while(list_node->next != NULL){
            list_node = list_node->next;
            index++;
        }
    }
    ref_at_index = dpl_get_reference_at_index(list, index);
    return ref_at_index->element;
}

int dpl_get_index_of_element(dplist_t *list, void *element) {
    DPLIST_ERR_HANDLER(list == NULL, DPLIST_INVALID_ERROR);
    if (list->head == NULL) return -1;
    dplist_node_t * list_node = list->head;
    int index = 0;
    while(index < dpl_size(list)){
        if(list->element_compare(list_node->element, element) == 0){
            return index;
        }
        list_node = list_node->next;
        index++;
    }
    return -1;
}

dplist_node_t *dpl_get_reference_at_index(dplist_t *list, int index) {
    int count;
    dplist_node_t *dummy;
    DPLIST_ERR_HANDLER(list == NULL, DPLIST_INVALID_ERROR);
    if (list->head == NULL) return NULL;
    for (dummy = list->head, count = 0; dummy->next != NULL; dummy = dummy->next, count++) {
        if (count >= index) return dummy;
    }
    return dummy;
}

void *dpl_get_element_at_reference(dplist_t *list, dplist_node_t *reference) {
    if(list->head == NULL || reference == NULL){
        return NULL;
    }
    dplist_node_t * list_node;
    list_node = list->head;
    while(list_node->next != NULL){
        if(list_node == reference){
            return reference->element;
        }
        list_node = list_node->next;
    }
    return NULL;
}
