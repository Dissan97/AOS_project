#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct node {
        char *path;
        int absolute;
        int id_dir;
        int add;
        int is_hardlink;
        char *which_file;
        int create;
        int is_black_listed;
        struct node *next;
        struct node *prev;
} node_t;

typedef struct node_list {
        node_t *head;
        node_t *tail;
} node_list_t;

void append_node(node_list_t *list, const char *path, int absolute, int id_dir,
                 int add, int is_hardlink, const char *which_file, int create);
void free_node(node_t *node);
void free_list(node_list_t *list);
void print_list(const node_list_t *list);
void init_list(node_list_t *list);
