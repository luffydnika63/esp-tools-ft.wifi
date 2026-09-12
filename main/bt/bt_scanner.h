#pragma once
#include <stdint.h>
#include <stdbool.h>

#define BT_SCANNER_MAX_DEVICES 20

typedef struct {
    uint8_t addr[6];
    uint8_t addr_type;
    char name[32];
    int8_t rssi;
    bool in_use;
} bt_device_t;

void bt_scanner_init(void);
int bt_scanner_start_scan(int duration_ms); // duration, returns 0 on ok
int bt_scanner_get_count(void);
bt_device_t* bt_scanner_get_devices(void);
bt_device_t* bt_scanner_get_device_by_idx(int idx);
void bt_scanner_clear(void);
