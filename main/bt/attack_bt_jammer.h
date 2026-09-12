#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct { uint8_t power; } bt_jammer_config_t;
void attack_bt_jammer_init(void);
void attack_bt_jammer_start(bt_jammer_config_t *cfg);
void attack_bt_jammer_stop(void);
bool attack_bt_jammer_is_running(void);
