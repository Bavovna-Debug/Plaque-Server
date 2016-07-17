#include <c.h>

#include "chalkboard.h"
#include "report.h"
#include "task_list.h"
#include "tasks.h"

// Take a pointer to chalkboard. Chalkboard must be initialized
// before any routine of this module could be called.
//
extern struct Chalkboard *chalkboard;

int
InitTaskList(void)
{
    long long	    totalListSize;
	size_t		    maxListPageSize;
    int			    numberOfPages;
    size_t		    eachPageSize;
    size_t		    lastPageSize;
    int			    tasksPerPage;
    int				pageId;
    int				taskId;
    int				taskIdInPage;
    size_t			pageSize;
	void			**list;
	void			*page;
	struct Task		**taskInList;

	maxListPageSize = MAX_TASK_LIST_PAGE_SIZE;

	tasksPerPage = MAX_TASK_LIST_PAGE_SIZE / sizeof(struct Task *);

	totalListSize = (long)MAX_NUMBER_OF_TASK * (long)sizeof(struct Task *);
	if (totalListSize <= maxListPageSize) {
		numberOfPages = 1;
		eachPageSize = totalListSize;
		lastPageSize = 0;
	} else {
		if ((totalListSize % maxListPageSize) == 0) {
			numberOfPages = totalListSize / maxListPageSize;
			eachPageSize = maxListPageSize;
			lastPageSize = 0;
		} else {
			numberOfPages = (totalListSize / maxListPageSize) + 1;
			eachPageSize = maxListPageSize;
			lastPageSize = totalListSize % maxListPageSize;
		}
	}

	list = malloc(numberOfPages * sizeof(struct Task *));
	if (list == NULL)
	{
        ReportSoftAlert("Out of memory");
        return -1;
    }

	pageId = 0;
	taskId = 0;
	taskIdInPage = tasksPerPage;

	while (taskId < MAX_NUMBER_OF_TASK)
	{
		if (taskIdInPage == tasksPerPage) {
			taskIdInPage = 0;

			if ((taskId + 1) < numberOfPages) {
				pageSize = eachPageSize;
			} else {
				pageSize = (lastPageSize == 0) ? eachPageSize : lastPageSize;
			}

	    	page = malloc(pageSize);
			if (page == NULL)
			{
        		ReportSoftAlert("Out of memory");
    	    	return -1;
		    }

			list[pageId] = page;
			pageId++;
		}

		taskInList = (void *)((unsigned long)page + (unsigned long)(taskIdInPage * sizeof(struct Task *)));
		*taskInList = NULL;

		taskIdInPage++;
		taskId++;
	}

	chalkboard->tasks.list = list;

	return 0;
}

void
TaskListPushTask(int taskId, struct Task *task)
{
    int			    tasksPerPage;
    int				pageId;
    int				taskIdInPage;
	void			*page;
	struct Task		**taskInList;

	tasksPerPage = MAX_TASK_LIST_PAGE_SIZE / sizeof(struct Task *);

	pageId = taskId / tasksPerPage;
	taskIdInPage = taskId % tasksPerPage;
	page = chalkboard->tasks.list[pageId];

	taskInList = (void *)((unsigned long)page + (unsigned long)(taskIdInPage * sizeof(struct Task *)));
	*taskInList = task;
}

struct Task *
TaskListTaskById(int taskId)
{
    int			    tasksPerPage;
    int				pageId;
    int				taskIdInPage;
	void			*page;
	struct Task		**taskInList;

	tasksPerPage = MAX_TASK_LIST_PAGE_SIZE / sizeof(struct Task *);

	pageId = taskId / tasksPerPage;
	taskIdInPage = taskId % tasksPerPage;
	page = chalkboard->tasks.list[pageId];

	taskInList = (void *)((unsigned long)page + (unsigned long)(taskIdInPage * sizeof(struct Task *)));

	return *taskInList;
}
