#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct { uint8_t target_idx; uint8_t all; } bt_spoof_config_t;
void attack_bt_spoof_init(void);
void attack_bt_spoof_start(bt_spoof_config_t *cfg);
void attack_bt_spoof_stop(void);
bool attack_bt_spoof_is_running(void);
