#!/bin/sh

printf "#define FACTORY_NVS_DATE \"$(date)\"" > include/factory_nvs.h
