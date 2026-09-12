#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t method;      // 0: BLE Disconnect Flood, 1: Connection Drain, 2: Random
    uint16_t timeout;
    uint8_t adv_type;
    uint8_t target_count;
    uint8_t targets[16][6];
    uint8_t all_devices; // 1 = deauth all, 0 = specific
} bt_deauth_config_t;

void attack_bt_deauth_init(void);
void attack_bt_deauth_start(bt_deauth_config_t *cfg);
void attack_bt_deauth_stop(void);
bool attack_bt_deauth_is_running(void);
