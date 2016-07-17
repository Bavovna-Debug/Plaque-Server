#ifndef __TASK_LIST__
#define __TASK_LIST__

#include "mmps.h"
#include "tasks.h"

#define MAX_TASK_LIST_PAGE_SIZE		MB
#define MAX_NUMBER_OF_TASK			NUMBER_OF_BUFFERS_TASK

int
InitTaskList(void);

void
TaskListPushTask(int taskId, struct Task *task);

struct Task *
TaskListTaskById(int taskId);

#endif
