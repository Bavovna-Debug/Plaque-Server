#ifndef __TASK_LIST__
#define __TASK_LIST__

#include "buffers.h"
#include "desk.h"
#include "tasks.h"

#define MAX_TASK_LIST_PAGE_SIZE		MB
#define MAX_NUMBER_OF_TASK			NUMBER_OF_BUFFERS_TASK

void **
initTaskList(struct desk *desk);

void
taskListPushTask(struct desk *desk, int taskId, struct task *task);

struct task *
taskListTaskById(struct desk *desk, int taskId);

#endif
