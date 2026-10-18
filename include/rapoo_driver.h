#ifndef RAPOO_DRIVER_H
#define RAPOO_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* USB Identifiers */
#define RAPOO_USB_VID             0x24ae
#define RAPOO_USB_PID             0x1244

/* HID IOCTL Commands */
#define RAPOO_HIDIOCSOUTPUT_32    0xc020480b
#define RAPOO_HIDIOCSOUTPUT_64    0xc040480b

/* Report ID & Protocol Constants */
#define RAPOO_REPORT_ID_OUT       0xba
#define RAPOO_REPORT_ID_IN        0xbb
#define RAPOO_DEV_CONST           0xa5
#define RAPOO_OPCODE_WRITE        0xa5
#define RAPOO_OPCODE_READ         0xa4
#define RAPOO_OPCODE_COMMIT       0xa1

/* Hardware Register Addresses (Realtek B-Cut / VT3PRO MAX) */
#define RAPOO_REG_POLLING         0x0880
#define RAPOO_REG_SENSOR          0x0884
#define RAPOO_REG_DPI_X           0x0888
#define RAPOO_REG_DPI_Y           0x08c8
#define RAPOO_REG_ACTIVE_DPI      0x0898
#define RAPOO_REG_MACRO_BASE      0x1000
#define RAPOO_MACRO_SLOT_SIZE     0x1000

/* Physical Button Register Addresses */
#define RAPOO_REG_BTN_LEFT        0x0600
#define RAPOO_REG_BTN_MIDDLE      0x0604
#define RAPOO_REG_BTN_RIGHT       0x0608
#define RAPOO_REG_BTN_TOP_DPI     0x060c
#define RAPOO_REG_BTN_DPI_MINUS   0x0610
#define RAPOO_REG_BTN_FORWARD     0x0614
#define RAPOO_REG_BTN_BACK        0x0618
#define RAPOO_REG_BTN_PRO_DIY     0x0620
#define RAPOO_REG_BTN_WHEEL_UP    0x0624
#define RAPOO_REG_BTN_WHEEL_DOWN  0x0628
#define RAPOO_REG_BTN_VT9_DIY     0x0634
#define RAPOO_REG_BTN_EXTRA9      0x0638

/* Supported Functions */
#define RAPOO_FUNC_LEFT           9
#define RAPOO_FUNC_MIDDLE         10
#define RAPOO_FUNC_RIGHT          11
#define RAPOO_FUNC_FORWARD        12
#define RAPOO_FUNC_BACK           13
#define RAPOO_FUNC_SNIPER         26
#define RAPOO_FUNC_FIRE           27
#define RAPOO_FUNC_DPI_CYCLE      36
#define RAPOO_FUNC_WHEEL_UP       37
#define RAPOO_FUNC_WHEEL_DOWN     38
#define RAPOO_FUNC_DPI_PLUS       39
#define RAPOO_FUNC_DPI_MINUS      40
#define RAPOO_FUNC_MACRO          41

/* Polling Rate Values (Hz) */
#define RAPOO_POLL_125            125
#define RAPOO_POLL_250            250
#define RAPOO_POLL_500            500
#define RAPOO_POLL_1000           1000
#define RAPOO_POLL_2000           2000
#define RAPOO_POLL_4000           4000
#define RAPOO_POLL_8000           8000

/* Device Handle */
typedef struct {
    int fd;
    char path[128];
    bool connected;
} rapoo_device_t;

/* Telemetry Data Structure */
typedef struct {
    uint8_t battery_percent;
    bool is_charging;
    uint16_t current_dpi;
    uint16_t polling_rate_hz;
    uint8_t raw_data[64];
} rapoo_telemetry_t;

/* Profile Data Structure */
typedef struct {
    uint16_t dpi_stages[7];
    uint8_t stage_count;
    uint8_t active_dpi_stage;
    uint16_t polling_rate_hz;
    float lod_mm;
    uint8_t debounce_ms;
    uint8_t button_mappings[16];
    uint8_t button_params[16];
} rapoo_profile_t;

/* Core Driver API */
int rapoo_find_device(char *out_path, size_t max_len);
int rapoo_open(rapoo_device_t *dev, const char *path);
void rapoo_close(rapoo_device_t *dev);

int rapoo_send_report(rapoo_device_t *dev, const uint8_t *report, size_t len);
int rapoo_write_register(rapoo_device_t *dev, uint32_t address, const uint8_t *payload, size_t len);
int rapoo_commit(rapoo_device_t *dev);

/* High-level Configuration API */
int rapoo_set_dpi_stages(rapoo_device_t *dev, const uint16_t *stages, size_t count, uint8_t active_idx);
int rapoo_set_polling_rate(rapoo_device_t *dev, uint16_t rate_hz);
int rapoo_set_sensor(rapoo_device_t *dev, float lod_mm, uint8_t debounce_ms);
int rapoo_remap_button(rapoo_device_t *dev, uint8_t button_id, uint8_t func_id, uint8_t param);
int rapoo_upload_macro(rapoo_device_t *dev, uint8_t slot_idx, const uint8_t *bytecode, size_t len);

/* Telemetry & Queries */
int rapoo_get_telemetry(rapoo_device_t *dev, rapoo_telemetry_t *out_telem, double timeout_sec);

/* Profile Persistence */
int rapoo_load_profile(const char *path, rapoo_profile_t *prof);
int rapoo_save_profile(const char *path, const rapoo_profile_t *prof);
int rapoo_apply_profile(rapoo_device_t *dev, const rapoo_profile_t *prof);
void rapoo_get_default_profile(rapoo_profile_t *prof);

/* Macro Utilities */
int rapoo_encode_macro_string(const char *macro_str, int default_delay_ms, int mode, int repeat,
                              uint8_t *out_buf, size_t max_out_len, size_t *out_len);

/* Helper UI / Name Functions */
const char* rapoo_get_button_name(uint8_t button_id);
const char* rapoo_get_function_name(uint8_t func_id);
uint32_t rapoo_get_button_register(uint8_t button_id);

#ifdef __cplusplus
}
#endif

#endif /* RAPOO_DRIVER_H */
