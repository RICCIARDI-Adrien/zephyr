#ifndef H_WATCHDOG_H
#define H_WATCHDOG_H

// Make the task watchdog API available to the includers of this file
#include <zephyr/task_wdt/task_wdt.h>

int watchdog_init(void);

#endif
