#pragma once
#include <stdbool.h>
void attack_bt_sniffer_start(void);
void attack_bt_sniffer_stop(void);
bool attack_bt_sniffer_is_running(void);
int attack_bt_sniffer_get_count(void);
