#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/policy.h>
#include <zephyr/pm/pm.h>
#include <zephyr/sys/printk.h>

#include "watchdog.h"

#define MAX_SUSPENDABLE_THREADS_COUNT 10

static const struct gpio_dt_spec app_led = GPIO_DT_SPEC_GET(DT_NODELABEL(led0), gpios);
static const struct gpio_dt_spec app_button_suspend = GPIO_DT_SPEC_GET(DT_NODELABEL(button0), gpios);
static const struct gpio_dt_spec app_button_resume = GPIO_DT_SPEC_GET(DT_NODELABEL(button2), gpios);

static struct gpio_callback suspend_app_button_callback_data;
static struct gpio_callback resume_app_button_callback_data;

static size_t suspendable_threads_count;
static k_tid_t suspendable_thread_ids[MAX_SUSPENDABLE_THREADS_COUNT];

static void __attribute__((unused)) burn_cpu(void)
{
	volatile int a = 0, i;

	for (i = 0; i < 50000000; i++)
	{
		a += 1239;
		a *= -7;
		a /= 11;
	}
}

static void thread_background_entry(void *p1, void *p2, void *p3)
{
	unsigned int i = 0;
	int channel;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	channel = task_wdt_add(2000, watchdog_callback, "background");
	if (channel < 0)
	{
		printk("Failed to initialize the watchdog for background thread; err=%d", -channel);
		return;
	}

	printk("Background thread is ready.\n");

	while (1)
	{
		printk("BACKGROUND %u\n", i);
		i++;

		task_wdt_feed(channel);
		k_msleep(1000);
	}
}
// Higher priority thread to be able to interrupt the busy waits in the main thread
K_THREAD_DEFINE(thread_background, 512, thread_background_entry, NULL, NULL, NULL, -1, K_ESSENTIAL, 1);

static void cache_suspendable_thread_callback(const struct k_thread *thread, void *user_data)
{
	k_tid_t thread_id;
	const char *name;
	bool keep_thread = false;

	ARG_UNUSED(user_data);

	// Retrieve the current thread name
	thread_id = (k_tid_t) thread;
	name = k_thread_name_get(thread_id);

	// Do not try to identify the thread if it has no name
	if (name == NULL)
		return;

	// The shell thread is always ready and keeps waking the system, so it needs to be suspended
	if (strcmp(name, "main") == 0)
		keep_thread = true;
	else if (strcmp(name, "shell_uart") == 0)
		keep_thread = true;
	else if (strcmp(name, "logging") == 0)
		keep_thread = true;
	else if (strcmp(name, "thread_background") == 0)
		keep_thread = true;

	if (keep_thread)
	{
		if (suspendable_threads_count >= MAX_SUSPENDABLE_THREADS_COUNT)
		{
			printk("No more room in the suspendable threads array.");
			k_sleep(K_FOREVER);
		}
		suspendable_thread_ids[suspendable_threads_count] = thread_id;
		suspendable_threads_count++;
		printk("Added the thread named \"%s\" to the suspendable list.\n", name);
	}
}

static void cache_suspendable_threads()
{
    suspendable_threads_count = 0;

    k_thread_foreach(cache_suspendable_thread_callback, NULL);

    printk("Cached %zu suspendable threads.\n", suspendable_threads_count);
}

static void suspend_threads()
{
	const char *name;
	k_tid_t thread_id;

	for (size_t i = 0; i < suspendable_threads_count; i++)
	{
		thread_id = suspendable_thread_ids[i];
		k_thread_suspend(thread_id);

		name = k_thread_name_get(thread_id);
		printk("Suspending thread named \"%s\".\n", name);
	}
}

static void resume_threads()
{
	const char *name;
	k_tid_t thread_id;

	for (size_t i = 0; i < suspendable_threads_count; i++)
	{
		thread_id = suspendable_thread_ids[i];
		k_thread_resume(thread_id);

		name = k_thread_name_get(thread_id);
		printk("Resuming thread named \"%s\".\n", name);
	}
}

static void gpio_callback(const struct device *port, struct gpio_callback *cb, gpio_port_pins_t pins)
{
	static bool is_suspended = false;

	ARG_UNUSED(port);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	printk("\033[35mGPIO interrupt callback.\033[0m\n");

	if (!is_suspended && (pins & (1 << 8)))
	{
		printk("\033[35mSUSPEND\033[0m\n");
		pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES); // Allow reaching suspend to idle
		task_wdt_suspend();
		suspend_threads();
		is_suspended = true;
	}
	else if (is_suspended && (pins & (1 << 10)))
	{
		printk("\033[35mRESUME\033[0m\n");
		task_wdt_resume();
		resume_threads();
		pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES); // Stay in active mode
		is_suspended = false;
	}
}

void power_notifier_entry(enum pm_state state)
{
	if (state == PM_STATE_SUSPEND_TO_IDLE)
	{
		// Tell that the App core is in suspend to idle mode
		gpio_pin_set_dt(&app_led, 1);
	}
}

void power_notifier_exit(enum pm_state state)
{
	if (state == PM_STATE_SUSPEND_TO_IDLE)
	{
		// Tell that the App core exited the suspend to idle mode
		gpio_pin_set_dt(&app_led, 0);
	}
}

static struct pm_notifier power_notifier =
{
	.state_entry = power_notifier_entry,
	.state_exit = power_notifier_exit
};

int main(void)
{
	int states_count, ret, channel;
	const struct pm_state_info *states;
	unsigned int i = 0;
	uint32_t start_time = 0, current_time;

	/* Set the main thread priority less than the shell and logging threads,
	 * so they continue working while the main thread is using 100% of the remaining resources.
	 */
	k_thread_priority_set(k_current_get(), 12);

	// Force staying in active mode by locking all other states
	pm_policy_state_lock_get(PM_STATE_RUNTIME_IDLE, PM_ALL_SUBSTATES);
	pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
	pm_policy_state_lock_get(PM_STATE_STANDBY, PM_ALL_SUBSTATES);
	pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_RAM, PM_ALL_SUBSTATES);
	pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_DISK, PM_ALL_SUBSTATES);

	cache_suspendable_threads();

	// Callbacks called when the power mode changes
	pm_notifier_register(&power_notifier);

	ret = watchdog_init();
	if (ret < 0)
		return -1;

	printk("Hello world from %s\n", CONFIG_BOARD_TARGET);

	// Retrieving power states
	states_count = pm_state_cpu_get_all(0, &states);
	printk("Power states count : %d.\n", states_count);

	if (!gpio_is_ready_dt(&app_led))
	{
		printk("App LED GPIO is not ready.\n");
		return -1;
	}
	if (!gpio_is_ready_dt(&app_button_resume))
	{
		printk("Suspend App button GPIO is not ready.\n");
		return -1;
	}
	if (!gpio_is_ready_dt(&app_button_suspend))
	{
		printk("Resume App button GPIO is not ready.\n");
		return -1;
	}

	ret = gpio_pin_configure_dt(&app_led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0)
	{
		printk("Failed to configure the App LED GPIO (%d).\n", ret);
		return -1;
	}
	ret = gpio_pin_configure_dt(&app_button_resume, GPIO_INPUT | GPIO_PULL_UP);
	if (ret < 0)
	{
		printk("Failed to configure the suspend App button GPIO (%d).\n", ret);
		return -1;
	}
	ret = gpio_pin_configure_dt(&app_button_suspend, GPIO_INPUT | GPIO_PULL_UP);
	if (ret < 0)
	{
		printk("Failed to configure the resume App button GPIO (%d).\n", ret);
		return -1;
	}

	ret = gpio_pin_interrupt_configure_dt(&app_button_suspend, GPIO_INT_EDGE_FALLING);
	if (ret < 0)
	{
		printk("Failed to configure the suspend App button interrupt (%d).\n", ret);
		return -1;
	}
	ret = gpio_pin_interrupt_configure_dt(&app_button_resume, GPIO_INT_EDGE_FALLING);
	if (ret < 0)
	{
		printk("Failed to configure the resume App button interrupt (%d).\n", ret);
		return -1;
	}

	gpio_init_callback(&suspend_app_button_callback_data, gpio_callback, BIT(app_button_suspend.pin));
	if (gpio_add_callback_dt(&app_button_suspend, &suspend_app_button_callback_data) < 0)
	{
		printk("Failed to add a callback to the suspend App button GPIO.\n");
		return -1;
	}
	gpio_init_callback(&resume_app_button_callback_data, gpio_callback, BIT(app_button_resume.pin));
	if (gpio_add_callback_dt(&app_button_resume, &resume_app_button_callback_data) < 0)
	{
		printk("Failed to add a callback to the resume App button GPIO.\n");
		return -1;
	}

	// Turn the power mode LED off to tell that the board is in active mode
	gpio_pin_set_dt(&app_led, 0);

	channel = task_wdt_add(2000, watchdog_callback, "main");
	if (channel < 0)
	{
		printk("Failed to initialize the watchdog for background thread; err=%d", -channel);
		return -1;
	}

	while (1)
	{
		printk("MAIN %u\n", i);
		i++;

		/* Wait 1 second without sleeping, so we do not enter any power management state and
		 * this makes the core consume as much energy as possible.
		 */
		do
		{
			current_time = k_uptime_get_32();
		} while ((current_time - start_time) < 1000);
		start_time = current_time;

		task_wdt_feed(channel);
	}

	return 0;
}
