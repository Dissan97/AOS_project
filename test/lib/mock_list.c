#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "include/mock_list.h"


node_t* create_node(const char *path, int absolute, int id_dir, int add, int is_hardlink, const char *which_file, int create) {
    node_t *new_node = malloc(sizeof(node_t));
    if (!new_node) {
        return NULL;  
    }

    new_node->path = malloc(strlen(path) + 1);
    if (!new_node->path) {
        free(new_node);  
        return NULL;
    }
    strcpy(new_node->path, path);

    new_node->absolute = absolute;
    new_node->id_dir = id_dir;
    new_node->add = add;
    new_node->is_hardlink = is_hardlink;

    if (is_hardlink) {
        new_node->which_file = malloc(strlen(which_file) + 1);
        if (!new_node->which_file) {
            free(new_node->path);  
            free(new_node);
            return NULL;
        }
        strcpy(new_node->which_file, which_file);
    } else {
        new_node->which_file = NULL; 
    }

    new_node->create = create;
    new_node->is_black_listed = 0;

    new_node->next = NULL;
    new_node->prev = NULL; 
    return new_node;
}

void append_node(node_list_t *list, const char *path, int absolute, int id_dir, int add,
 int is_hardlink, const char *which_file, int create) {
    node_t *new_node = create_node(path, absolute, id_dir, add, is_hardlink, which_file, create);
    if (!new_node) {
        fprintf(stderr, "Failed to create a new node.\n");
        return;
    }

    if (!list->head) {
        list->head = new_node;  
        list->tail = new_node;  
    } else {     
        list->tail->next = new_node;  
        new_node->prev = list->tail;   
        list->tail = new_node;          
    }
    
}

void free_node(node_t *node) {
    if (node) {
        free(node->path);  
        if (node->which_file) {
            free(node->which_file);  
        }
        free(node);  
    }
}

void free_list(node_list_t *list) {
    node_t *current;

    
    current = list->head;
    while (current) {
        node_t *next = current->next;
        free_node(current);
        current = next;
    }

    list->head = NULL;     
    list->tail = NULL;     
}

void print_list(const node_list_t *list) {
    node_t *current;

    printf("Add List:\n");
    current = list->head;
    while (current) {
        printf("Path: %s, Absolute: %d, ID: %d, Add: %d, Is Hardlink: %d, Which File: %s, create: %d\n", 
            current->path, current->absolute, current->id_dir, current->add, current->is_hardlink, 
            current->which_file ? current->which_file : "NULL", current->create);
        current = current->next;
    }
}
void init_list(node_list_t *list){
    list->head=NULL;
    list->tail=NULL;
}