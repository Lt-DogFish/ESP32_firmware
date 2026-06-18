#ifndef MY_WIFI_H
#define MY_WIFI_H

#include <stddef.h> // For size_t

#define WIFI_CONNECTED_BIT BIT0

/**
 * @brief Initialize the Wi-Fi station interface.
 */
void wifi_init(void);

/**
 * @brief Fetches the current local IP address as a string.
 * * @param ip_buf Pointer to the destination string buffer.
 * @param buf_len Size of the destination buffer (minimum 16 bytes recommended).
 */
void get_wifi_ip_string(char *ip_buf, size_t buf_len);

// Accessor function to safely bypass the file-level static restriction
EventGroupHandle_t get_wifi_event_group(void);

#endif