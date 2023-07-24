# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

add_library(mbed-stm32g071gbu6 INTERFACE)

target_sources(mbed-stm32g071gbu6
    INTERFACE
        PeripheralPins.c
)

target_include_directories(mbed-stm32g071gbu6
    INTERFACE
        .
)

target_link_libraries(mbed-stm32g071gbu6 INTERFACE mbed-stm32g071xb)
