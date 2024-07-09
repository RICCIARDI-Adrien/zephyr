#include <zephyr/autoconf.h> // Allow to quickly define (in an ugly way) the Kconfig symbols for the NVS code
#include <zephyr/kernel.h> // For mutexes
#include <zephyr/sys_clock.h> // For timeout

// Note : use the existing lib/libc/armstdc/src/threading_weak.c to replace a part of this file ?

int z_impl_k_mutex_init(struct k_mutex *mutex)
{
	ARG_UNUSED(mutex);
	return 0;
}

int z_impl_k_mutex_lock(struct k_mutex *mutex, k_timeout_t timeout)
{
	ARG_UNUSED(mutex);
	ARG_UNUSED(timeout);
	return 0;
}

int z_impl_k_mutex_unlock(struct k_mutex *mutex)
{
	ARG_UNUSED(mutex);
	return 0;
}
