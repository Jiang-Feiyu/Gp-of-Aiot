/* Get Start Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


/**
 * In this file, the following code blocks are marked for customization.
 * Each block starts with the comment: "// YOUR CODE HERE" 
 * and ends with: "// END OF YOUR CODE".
 *
 * [1] Modify the CSI Buffer and FIFO Lengths:
 *     - Adjust the buffer configuration based on your system if necessary.
 *
 * [2] Implement Algorithms:
 *     - Develop algorithms for motion detection, breathing rate estimation, and MQTT message sending.
 *     - Implement them in their respective functions.
 *
 * [3] Modify Wi-Fi Configuration:
 *     - Modify the Wi-Fi settings–SSID and password to connect to your router.
 *
 * [4] Finish the function `csi_process()`:
 *     - Fill in the group information.
 *     - Process and analyze CSI data in the `csi_process` function.
 *     - Implement your algorithms in this function if on-board. (Task 2)
 *     - Return the results to the host or send the CSI data via MQTT. (Task 3)
 *
 * Feel free to modify these sections to suit your project requirements!
 * 
 * Have fun building!
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "nvs_flash.h"
#include "esp_mac.h"
#include "rom/ets_sys.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "mqtt_client.h"
#include "esp_timer.h"
//  #include "esp_dsp.h"
 
#define MQTT_BROKER_URI "mqtt://192.168.43.16"
#define MQTT_TOPIC "esp32/data"
#define MQTT_PORT 1883
static const char *TAG;
 
static esp_mqtt_client_handle_t client = NULL;
 
// [1] YOUR CODE HERE
#define CSI_BUFFER_TIME 3000  //ms
#define SAMPLE_RATE 50
#define BUFFER_SIZE ((CSI_BUFFER_TIME * SAMPLE_RATE) / 1000)
#define FRAME_LENGTH 114
#define N_SUBCARRIERS 57
#define MOTION_THRESHOLD 0.15
#define CONFIRMATION_THRESHOLD 2
#define BREATHING_MIN_BPM 6
#define BREATHING_MAX_BPM 30

// Enable/Disable CSI Buffering. 1: Enable, using buffer, 0: Disable, using serial output
static bool CSI_Q_ENABLE = 1; 
static void csi_process(const int8_t *csi_data, int length);
// [1] END OF YOUR CODE
 
typedef struct {
    int16_t data[BUFFER_SIZE][FRAME_LENGTH];
    int64_t timestamps[BUFFER_SIZE];
    int write_index;
    int count;
    int consecutive_motion_count;
    int consecutive_static_count;
    bool motion_detected;
 } CSIBUFFER;

 static CSIBUFFER csi_buffer = {
    .write_index = 0,
    .count = 0,
    .consecutive_motion_count = 0,
    .consecutive_static_count = 0,
    .motion_detected = false
};
 
 // [2] YOUR CODE HERE
 // Modify the following functions to implement your algorithms.
 // NOTE: Please do not change the function names and return types.

 static double get_amplitude(int16_t imaginary, int16_t real) {
    return sqrt(imaginary * imaginary + real * real);
 }

 static double get_median(double* array, int size) {
    for (int i = 0; i < size - 1; i++) {
        for (int j = 0; j < size - i - 1; j++) {
            if (array[j] > array[j + 1]) {
                double temp = array[j];
                array[j] = array[j + 1];
                array[j + 1] = temp;
            }
        }
    }
    
    if (size % 2 == 0) {
        return (array[size/2 - 1] + array[size/2]) / 2.0;
    } else {
        return array[size/2];
    }
 }
 
 bool motion_detection() {
     // TODO: Implement motion detection logic using CSI data in CSI_Q
     if (csi_buffer.count < 2) {
        return false;
     }

     int n = N_SUBCARRIERS;
     double* variations = (double*)malloc(n * sizeof(double));
     int count = 0;

     for (int i = 0; i < n; i++) {
        double* amplitudes = (double*)malloc(csi_buffer.count * sizeof(double));

        for (int j = 0; j < csi_buffer.count; j++) {
            int real_idx = i * 2;
            int imaginary_idx = i * 2 + 1;
            amplitudes[j] = get_amplitude(csi_buffer.data[j][real_idx], csi_buffer.data[j][imaginary_idx]);
        }

        double mean = 0.0;
        for (int j = 0; j < csi_buffer.count; j++) {
            mean += amplitudes[j]; 
        }
        mean /= csi_buffer.count;

        if (mean > 0) {
            double sum_diff = 0;
            for (int j = 1; j < csi_buffer.count; j++) {
                sum_diff += fabs(amplitudes[j] - amplitudes[j - 1]);
            }
            double mean_diff = sum_diff / (csi_buffer.count - 1);
            double var1 = mean_diff / mean;

            double sq_diff = 0;
            for (int j = 0; j < csi_buffer.count; j++) {
                sq_diff += pow(amplitudes[j] - mean, 2);
            }
            double std = sqrt(sq_diff / csi_buffer.count);
            double var2 = std / mean;

            variations[count++] = fmax(var1, var2);
        }

        free(amplitudes);
     }

     double median_var = get_median(variations, count);
     double max_variation = 0;
     for (int i = 0; i < count; i++) {
        if (variations[i] > max_variation) {
            max_variation = variations[i];
        }
     }

     bool raw_motion = (median_var > MOTION_THRESHOLD * 0.8 && max_variation > MOTION_THRESHOLD);
     if (raw_motion) {
        csi_buffer.consecutive_motion_count++;
        csi_buffer.consecutive_static_count = 0;
     } else {
        csi_buffer.consecutive_motion_count = 0;
        csi_buffer.consecutive_static_count++;
     }

     bool motion;
     if (csi_buffer.consecutive_motion_count >= CONFIRMATION_THRESHOLD) {
        motion = true;
    } else if (csi_buffer.consecutive_static_count >= CONFIRMATION_THRESHOLD) {
        motion = false;
    } else {
        motion = csi_buffer.motion_detected;
    }

    csi_buffer.motion_detected = motion;
    
    free(variations);
    ESP_LOGI(TAG, "Motion detection: %s (variation: %.6f)", 
             motion ? "MOTION" : "NO MOTION", max_variation);
    
    return motion;
 }
 
 int breathing_rate_estimation() {
     // TODO: Implement breathing rate estimation using CSI data in CSI_Q
     if (csi_buffer.count < 5) {
        return 0;
     }

    int *peaks = (int *)malloc(csi_buffer.count * sizeof(int));
    int *valleys = (int *)malloc(csi_buffer.count * sizeof(int));
    double *amplitudes = (double *)malloc(csi_buffer.count * sizeof(double));
    double *filtered = (double *)malloc(csi_buffer.count * sizeof(double));

    if (!peaks || !valleys || !amplitudes || !filtered) {
        ESP_LOGE(TAG, "Memory allocation failed for breathing rate calculation");
        if (peaks) free(peaks);
        if (valleys) free(valleys);
        if (amplitudes) free(amplitudes);
        if (filtered) free(filtered);
        return 0;
    }

    double b[5] = {0.00011905, 0.0, -0.00023809, 0.0, 0.00011905};
    double a[5] = {1.0, -3.96831469, 5.90601698, -3.90708066, 0.96937846};

    int subcarrier = 30;

    for (int i = 0; i < csi_buffer.count; i++) {
        int real_idx = subcarrier * 2;
        int imaginary_idx = subcarrier * 2 + 1;
        amplitudes[i] = get_amplitude(csi_buffer.data[i][real_idx], 
                                     csi_buffer.data[i][imaginary_idx]);
    }

    for (int i = 0; i < csi_buffer.count; i++) {
        filtered[i] = 0.0;
        for (int j = 0; j <= 4; j++) { // Filter order 4
            if (i - j >= 0) filtered[i] += b[j] * amplitudes[i - j];
            if (j > 0 && i - j >= 0) filtered[i] -= a[j] * filtered[i - j];
        }
    }

    int num_peaks = 0, num_valleys = 0;
    for (int i = 1; i < csi_buffer.count - 1; i++) {
        if (filtered[i] > filtered[i-1] && filtered[i] > filtered[i+1]) {
            peaks[num_peaks++] = i;
        }
        if (filtered[i] < filtered[i-1] && filtered[i] < filtered[i+1]) {
            valleys[num_valleys++] = i;
        }
    }

    int bpm = 0;
    if (num_peaks > 1) {
        // Calculate average time between peaks
        int64_t total_time_diff = 0;
        for (int i = 1; i < num_peaks; i++) {
            int peak_idx1 = peaks[i-1];
            int peak_idx2 = peaks[i];
            int64_t time_diff = csi_buffer.timestamps[peak_idx2] - csi_buffer.timestamps[peak_idx1];
            total_time_diff += time_diff;
        }
        
        if (num_peaks > 1 && total_time_diff > 0) {
            // Average time per breath in milliseconds
            double avg_breath_time = (double)total_time_diff / (num_peaks - 1);
            
            // Convert to breaths per minute
            bpm = (int)(60000.0 / avg_breath_time);
            
            // Sanity check for expected breathing rate range (6-30 breaths per minute)
            if (bpm < 6 || bpm > 30) {
                ESP_LOGW(TAG, "Calculated breathing rate out of expected range: %d BPM", bpm);
                bpm = 0; // Invalid result
            } else {
                ESP_LOGI(TAG, "Breathing rate: %d BPM (from %d peaks)", bpm, num_peaks);
            }
        }
    }
    
    // Free allocated memory
    free(peaks);
    free(valleys);
    free(amplitudes);
    free(filtered);
    
    return bpm;
 }
 
 static void mqtt_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT连接成功");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT连接断开");
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT事件错误");
            break;
        default:
            ESP_LOGI(TAG, "MQTT事件: %d", event->event_id);
            break;
    }
}
 
 void mqtt_init() {
    ESP_LOGI(TAG, "Entering mqtt_init function");
    esp_mqtt_client_config_t mqtt_config = {
         .broker.address.uri = MQTT_BROKER_URI,
         .broker.address.port = MQTT_PORT,
     };
 
    ESP_LOGI(TAG, "Before esp_mqtt_client_init");
    client = esp_mqtt_client_init(&mqtt_config);
    if (client == NULL) {
        ESP_LOGE(TAG, "esp_mqtt_client_init failed");
    } else {
        ESP_LOGI(TAG, "esp_mqtt_client_init success");
    }
    ESP_LOGI(TAG, "Before esp_mqtt_client_register_event");
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    ESP_LOGI(TAG, "After esp_mqtt_client_register_event");
    esp_mqtt_client_start(client);
    ESP_LOGI(TAG, "After esp_mqtt_client_start");
 }
 
 void mqtt_send(const char* data) {
     // TODO: Implement MQTT message sending using CSI data or Results
     // NOTE: If you implement the algorithm on-board, you can return the results to the host, else send the CSI data.
     if (!client) {
         ESP_LOGE(TAG, "MQTT客户端未初始化");
         return;
     }
 
     // 示例：发送算法处理结果
     int msg_id = esp_mqtt_client_publish(client, MQTT_TOPIC, data, 0, 0, 0);
     if (msg_id >= 0) {
         ESP_LOGI(TAG, "MQTT消息发送成功，msg_id=%d", msg_id);
     } else {
         ESP_LOGE(TAG, "MQTT消息发送失败");
     }
 }
 // [2] END OF YOUR CODE
 
 
 #define CONFIG_LESS_INTERFERENCE_CHANNEL   7
 #define CONFIG_WIFI_BAND_MODE   WIFI_BAND_MODE_2G_ONLY
 #define CONFIG_WIFI_2G_BANDWIDTHS           WIFI_BW_HT20
 #define CONFIG_WIFI_5G_BANDWIDTHS           WIFI_BW_HT20
 #define CONFIG_WIFI_2G_PROTOCOL             WIFI_PROTOCOL_11N
 #define CONFIG_WIFI_5G_PROTOCOL             WIFI_PROTOCOL_11N
 #define CONFIG_ESP_NOW_PHYMODE           WIFI_PHY_MODE_HT20
 #define CONFIG_ESP_NOW_RATE             WIFI_PHY_RATE_MCS0_LGI
 #define CONFIG_FORCE_GAIN                   1
 #define CONFIG_GAIN_CONTROL                 CONFIG_FORCE_GAIN
 
 // UPDATE: Define parameters for scan method
 #if CONFIG_EXAMPLE_WIFI_ALL_CHANNEL_SCAN
 #define DEFAULT_SCAN_METHOD WIFI_ALL_CHANNEL_SCAN
 #elif CONFIG_EXAMPLE_WIFI_FAST_SCAN
 #define DEFAULT_SCAN_METHOD WIFI_FAST_SCAN
 #else
 #define DEFAULT_SCAN_METHOD WIFI_FAST_SCAN
 #endif /*CONFIG_EXAMPLE_SCAN_METHOD*/
 //
 
 static const uint8_t CONFIG_CSI_SEND_MAC[] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x00};
 
 static const char *TAG = "csi_recv";
 typedef struct
 {
     unsigned : 32; /**< reserved */
     unsigned : 32; /**< reserved */
     unsigned : 32; /**< reserved */
     unsigned : 32; /**< reserved */
     unsigned : 32; /**< reserved */
     unsigned : 16; /**< reserved */
     unsigned fft_gain : 8;
     unsigned agc_gain : 8;
     unsigned : 32; /**< reserved */
     unsigned : 32; /**< reserved */
     unsigned : 32; /**< reserved */
     unsigned : 32; /**< reserved */
     unsigned : 32; /**< reserved */
     unsigned : 32; /**< reserved */
 } wifi_pkt_rx_ctrl_phy_t;
 
 #if CONFIG_FORCE_GAIN
     /**
      * @brief Enable/disable automatic fft gain control and set its value
      * @param[in] force_en true to disable automatic fft gain control
      * @param[in] force_value forced fft gain value
      */
     extern void phy_fft_scale_force(bool force_en, uint8_t force_value);
 
     /**
      * @brief Enable/disable automatic gain control and set its value
      * @param[in] force_en true to disable automatic gain control
      * @param[in] force_value forced gain value
      */
     extern void phy_force_rx_gain(int force_en, int force_value);
 #endif
 
 static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data);
 static bool wifi_connected = false;
 
 //------------------------------------------------------WiFi Initialize------------------------------------------------------
 static void wifi_init()
 {
     ESP_ERROR_CHECK(esp_event_loop_create_default());
     ESP_ERROR_CHECK(esp_netif_init());
     esp_netif_create_default_wifi_sta();
 
     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
     ESP_ERROR_CHECK(esp_wifi_init(&cfg));
     
     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
     ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
 
     esp_event_handler_instance_t instance_any_id;
     esp_event_handler_instance_t instance_got_ip;
     ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                       ESP_EVENT_ANY_ID,
                                                       &wifi_event_handler,
                                                       NULL,
                                                       &instance_any_id));
     ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                       IP_EVENT_STA_GOT_IP,
                                                       &wifi_event_handler,
                                                       NULL,
                                                       &instance_got_ip));
     
     // [3] YOUR CODE HERE
     // You need to modify the ssid and password to match your Wi-Fi network.
     wifi_config_t wifi_config = {
         .sta = {
             .ssid = "AIOT_WIFI",         
             .password = "Aiot6666",
             .threshold.authmode = WIFI_AUTH_WPA2_PSK,
             // UPDATES: only use this scan method when you want to connect your mobile phone's hotpot
             .scan_method = DEFAULT_SCAN_METHOD,
             //
         
             .pmf_cfg = {
                 .capable = true,
                 .required = false
             },
         },
     };
     // [3] END OF YOUR CODE
 
     ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
     ESP_ERROR_CHECK(esp_wifi_start());
     ESP_LOGI(TAG, "wifi_init finished.");
 }
 
 //------------------------------------------------------WiFi Event Handler------------------------------------------------------
 static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
 {
     if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
         ESP_LOGI(TAG, "Trying to connect to AP...");
         esp_wifi_connect();
     } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
         ESP_LOGI(TAG, "Connection failed! Retrying...");
         wifi_connected = false;
         esp_wifi_connect();
     } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
         ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
         ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
         wifi_connected = true;
         
         wifi_ap_record_t ap_info;
         if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
             ESP_LOGI(TAG, "Connected to AP - SSID: %s, Channel: %d, RSSI: %d",
                     ap_info.ssid, ap_info.primary, ap_info.rssi);
         }
     }
 }
 
 //------------------------------------------------------ESP-NOW Initialize------------------------------------------------------
 static void wifi_esp_now_init(esp_now_peer_info_t peer) 
 {
     ESP_ERROR_CHECK(esp_now_init());
     ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *)"pmk1234567890123"));
     esp_now_rate_config_t rate_config = {
         .phymode = CONFIG_ESP_NOW_PHYMODE, 
         .rate = CONFIG_ESP_NOW_RATE,//  WIFI_PHY_RATE_MCS0_LGI,    
         .ersu = false,                     
         .dcm = false                       
     };
     ESP_ERROR_CHECK(esp_now_add_peer(&peer));
     ESP_ERROR_CHECK(esp_now_set_peer_rate_config(peer.peer_addr,&rate_config));
     ESP_LOGI(TAG, "================ ESP NOW Ready ================");
     ESP_LOGI(TAG, "esp_now_init finished.");
 }
 
 //------------------------------------------------------CSI Callback------------------------------------------------------
 static void wifi_csi_rx_cb(void *ctx, wifi_csi_info_t *info)
 {
     if (!info || !info->buf) return;
 
     ESP_LOGI(TAG, "CSI callback triggered");
 
     // Applying the CSI_Q_ENABLE flag to determine the output method
     // 1: Enable, using buffer, 0: Disable, using serial output
    //  if (!CSI_Q_ENABLE) {
    //      ets_printf("CSI_DATA,%d," MACSTR ",%d,%d,%d,%d\n",
    //                 info->len, MAC2STR(info->mac), info->rx_ctrl.rssi,
    //                 info->rx_ctrl.rate, info->rx_ctrl.noise_floor,
    //                 info->rx_ctrl.channel);
    //  } else {
    //      csi_process(info->buf, info->len);
    //  }
 
     
     if (!info || !info->buf) {
         ESP_LOGW(TAG, "<%s> wifi_csi_cb", esp_err_to_name(ESP_ERR_INVALID_ARG));
         return;
     }
 
     ESP_LOGI(TAG, "Received MAC: "MACSTR", Expected MAC: "MACSTR, 
              MAC2STR(info->mac), MAC2STR(CONFIG_CSI_SEND_MAC));
     
     if (memcmp(info->mac, CONFIG_CSI_SEND_MAC, 6)) {
         ESP_LOGI(TAG, "MAC address doesn't match, skipping packet");
         return;
     }
 
     wifi_pkt_rx_ctrl_phy_t *phy_info = (wifi_pkt_rx_ctrl_phy_t *)info;
     static int s_count = 0;
 
 #if CONFIG_GAIN_CONTROL
     static uint16_t agc_gain_sum=0; 
     static uint16_t fft_gain_sum=0; 
     static uint8_t agc_gain_force_value=0; 
     static uint8_t fft_gain_force_value=0; 
     if (s_count<100) {
         agc_gain_sum += phy_info->agc_gain;
         fft_gain_sum += phy_info->fft_gain;
     }else if (s_count == 100) {
         agc_gain_force_value = agc_gain_sum/100;
         fft_gain_force_value = fft_gain_sum/100;
     #if CONFIG_FORCE_GAIN
         phy_fft_scale_force(1,fft_gain_force_value);
         phy_force_rx_gain(1,agc_gain_force_value);
     #endif
         ESP_LOGI(TAG,"fft_force %d, agc_force %d",fft_gain_force_value,agc_gain_force_value);
     }
 #endif
 
     const wifi_pkt_rx_ctrl_t *rx_ctrl = &info->rx_ctrl;
     if (CSI_Q_ENABLE == 0) {
         ESP_LOGI(TAG, "================ CSI RECV via Serial Port ================");
         ets_printf("%d," MACSTR ",%d,%d,%d,%d,%d,%d,%d,%d,%d",
             s_count++, MAC2STR(info->mac), rx_ctrl->rssi, rx_ctrl->rate,
             rx_ctrl->noise_floor, phy_info->fft_gain, phy_info->agc_gain, rx_ctrl->channel,
             rx_ctrl->timestamp, rx_ctrl->sig_len, rx_ctrl->rx_state);
         ets_printf(",%d,%d,\"[%d", info->len, info->first_word_invalid, info->buf[0]);
 
         for (int i = 1; i < info->len; i++) {
             ets_printf(",%d", info->buf[i]);
         }
         ets_printf("]\"\n");
     }
 
     else{
         ESP_LOGI(TAG, "================ CSI RECV via Buffer ================");
         csi_process(info->buf, info->len);
     }
 }
 
 //------------------------------------------------------CSI Processing & Algorithms------------------------------------------------------
 static void csi_process(const int8_t *csi_data, int length)
 {   
    static int64_t last_motion_time = 0;
    static int64_t last_breathing_time = 0;
    int64_t current_time = esp_timer_get_time() / 1000;

    int write_idx = csi_buffer.write_index;
    for (int i = 0; i < length; i++) {
        csi_buffer.data[write_idx][i] = (int16_t)csi_data[i];
    }
    csi_buffer.timestamps[write_idx] = current_time;
    
    csi_buffer.write_index = (write_idx + 1) % BUFFER_SIZE;
    if (csi_buffer.count < BUFFER_SIZE) {
        csi_buffer.count++;
    }

    // 每500ms进行一次运动检测
    if (current_time - last_motion_time >= 500) {
        bool motion = motion_detection();
        const char* str_motion = motion ? "true" : "false";
        mqtt_send(str_motion);
        last_motion_time = current_time;
    }

    if (current_time - last_breathing_time >= 10000) {
        int breathing_rate = breathing_rate_estimation();
        if (breathing_rate > 0) {
            char breath_msg[32];
            snprintf(breath_msg, sizeof(breath_msg), "{\"bpm\":%d}", breathing_rate);
            mqtt_send(breath_msg);
        }
        last_breathing_time = current_time;
    }
 
     // [4] YOUR CODE HERE
 
     // 1. Fill the information of your group members
     ESP_LOGI(TAG, "================ GROUP INFO ================");
     const char *TEAM_MEMBER[] = {"a", "b", "c", "d"};
     const char *TEAM_UID[] = {"1", "2", "3", "4"};
     ESP_LOGI(TAG, "TEAM_MEMBER: %s, %s, %s, %s | TEAM_UID: %s, %s, %s, %s",
                 TEAM_MEMBER[0], TEAM_MEMBER[1], TEAM_MEMBER[2], TEAM_MEMBER[3],
                 TEAM_UID[0], TEAM_UID[1], TEAM_UID[2], TEAM_UID[3]);
     ESP_LOGI(TAG, "================ END OF GROUP INFO ================");
 
     // 2. Call your algorithm functions here, e.g.: motion_detection(), breathing_rate_estimation(), and mqtt_send()
     // If you implement the algorithm on-board, you can return the results to the host, else send the CSI data.
    //  motion_detection();
    //  breathing_rate_estimation();
    //  mqtt_send();
     char csi_message[1024]; // 注意缓冲区大小需要足够大以存储CSI数据
     int offset = 0;
 
     // 构造CSI数据字符串
     offset += snprintf(csi_message + offset, sizeof(csi_message) - offset, "[");
     for (int i = 0; i < length; i++) {
         offset += snprintf(csi_message + offset, sizeof(csi_message) - offset, "%d,", csi_data[i]);
         if (offset >= sizeof(csi_message)) {
             ESP_LOGE(TAG, "CSI数据超出缓冲区大小");
             break;
         }
     }
     offset += snprintf(csi_message + offset, sizeof(csi_message) - offset, "]");
 
     // 移除最后一个多余的逗号（如果存在）
     if (offset > 0 && csi_message[offset - 1] == ',') {
         csi_message[offset - 1] = '\0'; // 将最后的逗号替换为字符串结束符
     }
 
     // 打印日志以调试
     ESP_LOGI(TAG, "发送的CSI数据: %s", csi_message);
 
     // 调用mqtt_send函数发送数据
     
     //mqtt_send(str_motion);
     // [4] END YOUR CODE HERE
 }
 
 
 //------------------------------------------------------CSI Config Initialize------------------------------------------------------
 static void wifi_csi_init()
 {
     ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
     wifi_csi_config_t csi_config = {
         .enable                   = true,                           
         .acquire_csi_legacy       = false,               
         .acquire_csi_force_lltf   = false,           
         .acquire_csi_ht20         = true,                  
         .acquire_csi_ht40         = true,                  
         .acquire_csi_vht          = false,                  
         .acquire_csi_su           = false,                   
         .acquire_csi_mu           = false,                   
         .acquire_csi_dcm          = false,                  
         .acquire_csi_beamformed   = false,           
         .acquire_csi_he_stbc_mode = 2,                                                                                                                                                                                                                                                                               
         .val_scale_cfg            = 0,                    
         .dump_ack_en              = false,                      
         .reserved                 = false                         
     };
     ESP_ERROR_CHECK(esp_wifi_set_csi_config(&csi_config));
     ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(wifi_csi_rx_cb, NULL));
     ESP_ERROR_CHECK(esp_wifi_set_csi(true));
 }
 
 //------------------------------------------------------Main Function------------------------------------------------------
 void app_main()
 {
     /**
      * @brief Initialize NVS
      */
     esp_err_t ret = nvs_flash_init();
     if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
         ESP_ERROR_CHECK(nvs_flash_erase());
         ret = nvs_flash_init();
     }
     ESP_ERROR_CHECK(ret);
 
     /**
      * @brief Initialize Wi-Fi
      */
     wifi_init();
 
     // Get Device MAC Address
     uint8_t mac[6];
     esp_wifi_get_mac(WIFI_IF_STA, mac);
     ESP_LOGI(TAG, "Device MAC Address: " MACSTR, MAC2STR(mac));
 
     // Try to connect to WiFi
     ESP_LOGI(TAG, "Connecting to WiFi...");
 
     // Wait for Wi-Fi connection
     int retry_count = 0;
     bool wifi_connected = false;
     while (!wifi_connected && retry_count < 20) {
         vTaskDelay(pdMS_TO_TICKS(1000));
         retry_count++;
         ESP_LOGI(TAG, "Waiting for Wi-Fi connection... (%d/20)", retry_count);
 
         wifi_ap_record_t ap_info;
         if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
             ESP_LOGI(TAG, "Connected to SSID: %s, RSSI: %d, Channel: %d",
                      ap_info.ssid, ap_info.rssi, ap_info.primary);
             wifi_connected = true;
         }
     }
 
     /**
      * @brief Initialize ESP-NOW
      */
 
     if (wifi_connected) {
         esp_now_peer_info_t peer = {
             .channel   = CONFIG_LESS_INTERFERENCE_CHANNEL,
             .ifidx     = WIFI_IF_STA,
             .encrypt   = false,
             .peer_addr = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
         };
         
         wifi_esp_now_init(peer); // Initialize ESP-NOW Communication
         wifi_csi_init(); // Initialize CSI Collection
         mqtt_init();
 
     } else {
         ESP_LOGI(TAG, "WiFi connection failed");
         return;
     }
 }
 