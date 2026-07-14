# Copyright (c) 2026 Semy BENADY
# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources(tpi1s1385.c)
zephyr_library_sources_ifdef(CONFIG_TPI1S1385_TRIGGER trigger_tpi1s1385.c)
