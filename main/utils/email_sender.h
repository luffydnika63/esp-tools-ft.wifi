#pragma once
#include <stdbool.h>

bool email_is_configured(void);
void email_send_captured(const char *ssid, const char *password, const char *source);
