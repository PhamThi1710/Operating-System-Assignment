#include <stdio.h>
#include <stdlib.h>
#include "queue.h"
// 2 TODO - doing

void swap(struct pcb_t *a, struct pcb_t *b)
{
        struct pcb_t tmp = *a;
        *a = *b;
        *b = tmp;
}
void reheapUp(struct queue_t *q, int position)
{
        if (position == 0)
        {
                return;
        }
        int present = (position - 1) / 2;
        if (q->proc[position]->priority < q->proc[present]->priority)
        {
                swap(q->proc[position], q->proc[present]);
        }
        reheapUp(q, position -= 1);
}
void reheapDown(struct queue_t *q, int index)
{
        int left = 2 * index + 1;
        if (left >= q->size)
        {
                return;
        }
        if (q->size - 1 < left + 1)
        {
                if (q->proc[left]->priority < q->proc[index]->priority)
                {
                        swap(q->proc[left], q->proc[index]);
                }
        }
        else
        {
                int right = left + 1;
                int child = (q->proc[left]->priority < q->proc[right]->priority) ? left : right;
                if (q->proc[child]->priority < q->proc[index]->priority)
                {
                        swap(q->proc[child], q->proc[index]);
                }
                reheapDown(q, child);
        }
}
int empty(struct queue_t *q)
{
        if (q == NULL)
                return 1;
        return (q->size == 0);
}

void enqueue(struct queue_t *q, struct pcb_t *proc)
{
        if (q->size < MAX_QUEUE_SIZE)
        {
                struct pcb_t *newP = (struct pcb_t *)malloc(sizeof(struct pcb_t));
                newP = proc;
                q->proc[q->size] = newP;
                q->size++;
                reheapUp(q, q->size - 1);
        }

        /* TODO: put a new process to queue [q] */
}

struct pcb_t *dequeue(struct queue_t *q)
{
        if (q->size == 0)
        {
                return NULL;
        }
        struct pcb_t *result = (struct pcb_t *)malloc(sizeof(struct pcb_t));
        result = q->proc[0];
        q->proc[0] = q->proc[--q->size];

        q->proc[q->size] = NULL;
        // free(q->proc[q->size]);
        reheapDown(q, 0);
        return result;
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */
}
