/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <factory_nvs.h>

int main(void)
{
	printf("Hello World! %s\n", CONFIG_BOARD_TARGET);
	printf("\033[32mfactory_nvs.h date : %s\033[0m\n", FACTORY_NVS_DATE);

	return 0;
}
