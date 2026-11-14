/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include "common.h"
#include "syscall.h"
#include "stdio.h"
#include "libmem.h"
#include "queue.h"
#include <string.h>
#include <stdlib.h>

void terminate_process(struct pcb_t *proc)
{
    if (proc == NULL)
    {
        return;
    }

    for (int i = 0; i < 10; i++)
    {
        if (proc->regs[i])
        {
            libfree(proc, proc->regs[i]);
            proc->regs[i] = 0;
        }
    }

#ifdef MM_PAGING

    proc->mm = NULL;
    proc->mram = NULL;
    proc->mswp = NULL;
    proc->active_mswp = NULL;

#endif

    proc->code = NULL;
    proc->page_table = NULL;

    free(proc);
}

int __sys_killall(struct pcb_t *caller, struct sc_regs *regs)
{
    char proc_name[100] = "";
    uint32_t data;
    // hardcode for demo only
    uint32_t memrg = regs->a1;

    /* TODO: Get name of the target proc */
    // proc_name = libread..
    int i = 0;

    data = 0;
    while (1)
    {
        data = 0;
        libread(caller, memrg, i, &data);
        proc_name[i] = (char)data;

        if (data == -1)
        {
            proc_name[i] = '\0';
            break;
        }
        i++;
    }
    printf("The procname retrieved from memregionid %d is \"%s\"\n", memrg, proc_name);

    /* TODO: Traverse proclist to terminate the proc
     *       stcmp to check the process match proc_name
     */
    // caller->running_list
    // caller->mlq_ready_queu

    /* TODO Maching and terminating
     *       all processes with given
     *        name in var proc_name
     */
    char proc_path[100] = "input/proc/";
    int len = strlen("input/proc/");
    for (i = 0; proc_name[i] != '\0'; i++)
    {
        proc_path[len + i] = proc_name[i];
    }
    proc_path[len + i] = '\0';

    struct queue_t *running_list = caller->running_list;
    int rl_size = running_list->size;

    for (int i = 0; i < rl_size; i++)
    {
        struct pcb_t *current = dequeue(running_list);

        if (current->pid == caller->pid)
        {
            enqueue(running_list, current);
            continue;
        }
        if (!strcmp(current->path, proc_path))
        {
            current->pc = -1;

            continue;
        }
        else
        {
            enqueue(running_list, current);
        }
    }

#ifdef MLQ_SCHED

    for (int level = 0; level < 140; level++)
    {
        struct queue_t *q = &caller->mlq_ready_queue[level];
        if (q)
        {
            int qsize = q->size;
            for (int j = 0; j < qsize; j++)
            {
                struct pcb_t *current = dequeue(q);
                if (current->pid == caller->pid)
                {

                    enqueue(q, current);

                    continue;
                }

                if (!strcmp(current->path, proc_path))
                {
                    printf("Processed %2d with path %s has terminated\n", level, current->path);
                    current->pc = -1;

                    terminate_process(current);
                    continue;
                }
                else
                {

                    enqueue(q, current);
                }
            }
        }
        else
        {
            printf("There are no processes in mlq_ready_queue at priority %2d \n", level);
        }
    }

#else
    struct queue_t *ready_queue = &caller->ready_queue;
    int rq_size = ready_queue->size;
    if (ready_queue)
    {
        for (int i = 0; i < rq_size; i++)
        {
            struct pcb_t *current = dequeue(ready_queue);
            if (current->pid == caller->pid)
            {
                enqueue(ready_queue, current);
                continue;
            }
            if (!strcmp(current->path, proc_path))
            {
                printf("Processed %2d with path %s has terminated\n", current->pid, current->path);
                current->pc = -1;
                terminate_process(current);
                continue;
            }
            else
            {
                enqueue(ready_queue, current);
            }
        }
    }
    else
    {
        printf("There is no processes in ready_queue \n")
    }
#endif
    return 0;
}
