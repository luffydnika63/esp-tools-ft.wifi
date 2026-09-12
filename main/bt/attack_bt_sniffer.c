#include "attack_bt_sniffer.h"
#include "bt_scanner.h"
#include "attack_bt_spam.h"
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
static const char *TAG="attack_bt_sniffer";
static bool running=false;
void attack_bt_sniffer_start(void){
    if(running){ ESP_LOGW(TAG,"already running"); return;}
    bt_scanner_init();
    running=true;
    bt_scanner_start_scan(10000);
    ESP_LOGI(TAG,"Sniffer started 10s scan");
    // keep running flag for status, but scanner runs once; we set finished after scan
    // For continuous, we could loop, but simple: mark running for 10s then auto finish
    // Attack wrapper will handle timeout
}
void attack_bt_sniffer_stop(void){
    if(!running) return;
    running=false;
    ESP_LOGI(TAG,"Sniffer stopped");
}
bool attack_bt_sniffer_is_running(void){ return running; }
int attack_bt_sniffer_get_count(void){ return bt_scanner_get_count(); }
