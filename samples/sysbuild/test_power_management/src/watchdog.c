#include <zephyr/drivers/watchdog.h>
#include <zephyr/sys/printk.h>

#include "watchdog.h"

#define WATCHDOG_NAME watchdog0

int watchdog_init(void)
{
    const struct device *watchdog_device;
    int ret;

    watchdog_device = DEVICE_DT_GET_OR_NULL(DT_ALIAS(WATCHDOG_NAME));
    if (!watchdog_device) {
        printk("Error: failed to get the watchdog device from the device tree.\n");
        return -EINVAL;
    }

    if (!device_is_ready(watchdog_device)) {
        printk("Error: the hardware watchdog device is not ready.\n");
        return -EBUSY;
    }

    ret = task_wdt_init(watchdog_device);
    if (ret != 0) {
        printk("Error: failed to initialize the task watchdog (%d)\n.", ret);
        return ret;
    }

    return 0;
}
