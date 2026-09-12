#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct { uint8_t mode; } bt_crash_config_t;
void attack_bt_crash_init(void);
void attack_bt_crash_start(bt_crash_config_t *cfg);
void attack_bt_crash_stop(void);
bool attack_bt_crash_is_running(void);
