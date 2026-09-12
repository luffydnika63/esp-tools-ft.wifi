#include "bt_scanner.h"
#include "attack_bt_spam.h"
#include <string.h>
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"

static const char *TAG = "bt_scanner";
static bt_device_t devices[BT_SCANNER_MAX_DEVICES];
static int device_count = 0;
static bool scanning = false;

static int gap_event_handler(struct ble_gap_event *event, void *arg){
    if(event->type == BLE_GAP_EVENT_DISC){
        struct ble_gap_disc_desc *disc = &event->disc;
        const uint8_t *data = disc->data;
        uint8_t len = disc->length_data;
        char parsed_name[32]={0};
        for(int i=0;i+1 < (int)len;){
            uint8_t l = data[i];
            if(l==0 || i + l >= (int)len) break;
            uint8_t type = data[i+1];
            if(type==0x09 || type==0x08){
                int nlen = l-1;
                if(nlen>31) nlen=31;
                memcpy(parsed_name, &data[i+2], nlen);
                parsed_name[nlen]=0;
                break;
            }
            i += l+1;
        }
        // check duplicate
        for(int i=0;i<device_count;i++){
            if(memcmp(devices[i].addr, disc->addr.val, 6)==0) {
                devices[i].rssi = disc->rssi;
                if(parsed_name[0]!=0 && (devices[i].name[0]==0 || strncmp(devices[i].name,"BT ",3)==0)){
                    strncpy(devices[i].name, parsed_name, sizeof(devices[i].name)-1);
                    ESP_LOGI(TAG, "Updated BT name %s", parsed_name);
                }
                return 0;
            }
        }
        if(device_count >= BT_SCANNER_MAX_DEVICES) return 0;
        bt_device_t *d = &devices[device_count++];
        memcpy(d->addr, disc->addr.val, 6);
        d->addr_type = disc->addr.type;
        d->rssi = disc->rssi;
        d->in_use = true;
        if(parsed_name[0]!=0){
            strncpy(d->name, parsed_name, sizeof(d->name)-1);
        } else {
            // no name in adv - try to keep readable, show MAC-based name but keep MAC column separate
            snprintf(d->name, sizeof(d->name), "Unknown");
        }
        ESP_LOGI(TAG, "Found BT %s [%02X:%02X:%02X:%02X:%02X:%02X] RSSI %d", d->name, d->addr[5],d->addr[4],d->addr[3],d->addr[2],d->addr[1],d->addr[0], d->rssi);
        return 0;
    } else if(event->type == BLE_GAP_EVENT_DISC_COMPLETE){
        scanning=false;
        ESP_LOGI(TAG, "Scan complete, %d devices", device_count);
    }
    return 0;
}

void bt_scanner_init(void){
    // ensure host synced via spam init
    if(!ble_hs_synced()){
        attack_bt_spam_init();
        int t=50;
        while(!ble_hs_synced() && t-- >0) vTaskDelay(pdMS_TO_TICKS(100));
    }
}

int bt_scanner_start_scan(int duration_ms){
    if(scanning){
        ble_gap_disc_cancel();
        scanning=false;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    device_count=0;
    memset(devices,0,sizeof(devices));
    struct ble_gap_disc_params params={0};
    params.filter_policy = BLE_HCI_SCAN_FILT_NO_WL;
    params.passive = 0;
    params.itvl = 0x0010;
    params.window = 0x0010;
    params.filter_duplicates = 0;
    params.limited = 0;
    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, duration_ms, &params, gap_event_handler, NULL);
    if(rc==0){
        scanning=true;
        ESP_LOGI(TAG, "BT scan started %dms", duration_ms);
    } else {
        ESP_LOGE(TAG, "ble_gap_disc failed %d", rc);
        // fallback mock data if scan not possible (e.g. host busy)
        if(device_count==0){
            for(int i=0;i<3;i++){
                bt_device_t *d=&devices[device_count++];
                for(int j=0;j<6;j++) d->addr[j]= (uint8_t)(0xA0 + i*0x11 + j);
                d->rssi = -40 - i*10;
                snprintf(d->name, sizeof(d->name), "MockBT-%d", i+1);
                d->in_use=true;
            }
        }
    }
    return rc;
}

int bt_scanner_get_count(void){ return device_count; }
bt_device_t* bt_scanner_get_devices(void){ return devices; }
bt_device_t* bt_scanner_get_device_by_idx(int idx){
    if(idx<0 || idx>=device_count) return NULL;
    return &devices[idx];
}
void bt_scanner_clear(void){ device_count=0; memset(devices,0,sizeof(devices)); }
