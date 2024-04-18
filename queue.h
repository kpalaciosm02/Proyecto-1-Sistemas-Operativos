#ifndef QUEUE
#define QUEUE

#define MAX_PATH_LENGTH 512

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct QueueNode {
    char path[MAX_PATH_LENGTH];
    struct QueueNode* next;
};

struct Queue {
    struct QueueNode *front, *rear;
};

struct QueueNode* newQueueNode(char *path) {
    struct QueueNode* temp = (struct QueueNode*)malloc(sizeof(struct QueueNode));
    strncpy(temp->path, path, MAX_PATH_LENGTH - 1);
    temp->path[MAX_PATH_LENGTH - 1] = '\0';
    temp->next = NULL;
    return temp;
}

struct Queue* createQueue() {
    struct Queue* q = (struct Queue*)malloc(sizeof(struct Queue));
    q->front = q->rear = NULL;
    return q;
}

void enqueue(struct Queue* q, char *path) {
    struct QueueNode* temp = newQueueNode(path);
    if (q->rear == NULL) {
        q->front = q->rear = temp;
        return;
    }
    q->rear->next = temp;
    q->rear = temp;
}



char* dequeue(struct Queue* q) {
    if (q->front == NULL)
        return NULL;
    struct QueueNode* temp = q->front;
    q->front = q->front->next;
    if (q->front == NULL)
        q->rear = NULL;

    char *path = (char*)malloc(MAX_PATH_LENGTH * sizeof(char));
    if (path == NULL) {
        return NULL;
    }
    strncpy(path, temp->path, MAX_PATH_LENGTH - 1);
    path[MAX_PATH_LENGTH - 1] = '\0';

    free(temp);

    return path;
}

int isEmpty(struct Queue* q) {
    return q->front == NULL;
}

void printQueue(struct Queue *q) {
    struct QueueNode *current = q->front;

    while (current != NULL) {
        printf("%s\n", current->path);
        current = current->next;
    }
}

int size(struct Queue* q) {
    int count = 0;
    struct QueueNode *current = q->front;

    while (current != NULL) {
        count++;
        current = current->next;
    }

    return count;
}
#endif