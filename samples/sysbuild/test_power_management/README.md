# Build command

```
cd zephyr/samples/sysbuild/test_power_management
west build -p -b nrf54h20dk/nrf54h20/cpuapp -T sample.sysbuild.test_power_management.nrf54h20dk_cpuapp_cpurad .
```

# DK configuration

* Connect a wire from Port P1 pin 1 to Port P9 pin 1. This will connect LED1 to the Radio core LED.
