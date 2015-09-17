#ifndef _PROCESSOR_
#define _PROCESSOR_

#include "db.h"
#include "tasks.h"

int getPlaquesForRadar(struct dbh *dbh, struct task *task);

int getPlaquesWithDetails(struct dbh *dbh, struct task *task);

int createPlaque(struct dbh *dbh, struct task *task);

#endif
