FACTORY_FLASH_CONTROLLER_OVERLAY
FACTORY_NVS_CONTENT_FILE
FACTORY_NVS_PARTITION_DT_LABEL

-DCONFIG_xxx to set a Kconfig variable

execution :
-flash=<path> to specify the name and path of the binary file containing the generated NVS content

sans CRC data :
west build -b native_sim samples/subsys/nvs/factory/ -- -DFACTORY_FLASH_CONTROLLER_OVERLAY=demo_flash_controller.overlay -DFACTORY_NVS_PARTITION_DT_LABEL=storage_partition -DFACTORY_NVS_CONTENT_FILE=src/demo_nvs_content.c

avec CRC data :
west build -b native_sim samples/subsys/nvs/factory/ -- -DFACTORY_FLASH_CONTROLLER_OVERLAY=demo_flash_controller.overlay -DFACTORY_NVS_PARTITION_DT_LABEL=storage_partition -DFACTORY_NVS_CONTENT_FILE=src/demo_nvs_content.c -DCONFIG_NVS_DATA_CRC=y

lancement :
build/zephyr/zephyr.exe
