#pragma once

#include <stdio.h>

#define TEE_LOG(fmt, ...) \
    printf("[GPAPI] %s: " fmt, __FUNCTION__, ##__VA_ARGS__)