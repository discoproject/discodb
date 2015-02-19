/* Originally from Emonk */

#ifndef ERLEXI_QUEUE_H
#define ERLEXI_QUEUE_H

#include "erl_nif.h"

typedef struct queue queue;
typedef struct qitem qitem;

queue *queue_new();
void queue_free(queue *queue);

int queue_empty(queue *queue);

int queue_push(queue *queue, void *item);
void *queue_pop(queue *queue);

int queue_send(queue *queue, void *item);
void *queue_receive(queue *);

#endif
