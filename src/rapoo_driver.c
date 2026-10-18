#define _GNU_SOURCE
#include "rapoo_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <math.h>
#include <ctype.h>

/* HID Key Scancodes (Usage Page 0x07) */
typedef struct {
    const char *name;
    uint8_t code;
} key_mapping_t;

static const key_mapping_t HID_KEY_TABLE[] = {
    {"a", 0x04}, {"b", 0x05}, {"c", 0x06}, {"d", 0x07}, {"e", 0x08}, {"f", 0x09},
    {"g", 0x0a}, {"h", 0x0b}, {"i", 0x0c}, {"j", 0x0d}, {"k", 0x0e}, {"l", 0x0f},
    {"m", 0x10}, {"n", 0x11}, {"o", 0x12}, {"p", 0x13}, {"q", 0x14}, {"r", 0x15},
    {"s", 0x16}, {"t", 0x17}, {"u", 0x18}, {"v", 0x19}, {"w", 0x1a}, {"x", 0x1b},
    {"y", 0x1c}, {"z", 0x1d},
    {"1", 0x1e}, {"2", 0x1f}, {"3", 0x20}, {"4", 0x21}, {"5", 0x22}, {"6", 0x23},
    {"7", 0x24}, {"8", 0x25}, {"9", 0x26}, {"0", 0x27},
    {"enter", 0x28}, {"return", 0x28}, {"esc", 0x29}, {"escape", 0x29},
    {"backspace", 0x2a}, {"tab", 0x2b}, {"space", 0x2c}, {"spacebar", 0x2c},
    {"-", 0x2d}, {"minus", 0x2d}, {"=", 0x2e}, {"equal", 0x2e},
    {"[", 0x2f}, {"]", 0x30}, {"\\", 0x31}, {";", 0x33}, {"'", 0x34},
    {"`", 0x35}, {",", 0x36}, {".", 0x37}, {"/", 0x38}, {"capslock", 0x39},
    {"f1", 0x3a}, {"f2", 0x3b}, {"f3", 0x3c}, {"f4", 0x3d}, {"f5", 0x3e},
    {"f6", 0x3f}, {"f7", 0x40}, {"f8", 0x41}, {"f9", 0x42}, {"f10", 0x43},
    {"f11", 0x44}, {"f12", 0x45},
    {"printscreen", 0x46}, {"scrolllock", 0x47}, {"pause", 0x48}, {"insert", 0x49},
    {"home", 0x4a}, {"pageup", 0x4b}, {"delete", 0x4c}, {"end", 0x4d}, {"pagedown", 0x4e},
    {"right", 0x4f}, {"left", 0x50}, {"down", 0x51}, {"up", 0x52},
    {"ctrl", 0xe0}, {"lctrl", 0xe0}, {"leftctrl", 0xe0},
    {"shift", 0xe1}, {"lshift", 0xe1}, {"leftshift", 0xe1},
    {"alt", 0xe2}, {"lalt", 0xe2}, {"leftalt", 0xe2},
    {"win", 0xe3}, {"gui", 0xe3}, {"super", 0xe3},
    {"rctrl", 0xe4}, {"rshift", 0xe5}, {"ralt", 0xe6},
    {NULL, 0}
};

static uint8_t lookup_hid_key(const char *key) {
    if (!key) return 0;
    char lower[32];
    size_t i;
    for (i = 0; key[i] && i < sizeof(lower) - 1; ++i) {
        lower[i] = (char)tolower((unsigned char)key[i]);
    }
    lower[i] = '\0';

    for (const key_mapping_t *m = HID_KEY_TABLE; m->name; ++m) {
        if (strcmp(lower, m->name) == 0) {
            return m->code;
        }
    }
    return 0;
}

uint32_t rapoo_get_button_register(uint8_t button_id) {
    switch (button_id) {
        case 0:  return RAPOO_REG_BTN_LEFT;       /* 0x0600 */
        case 1:  return RAPOO_REG_BTN_RIGHT;      /* 0x0608 */
        case 2:  return RAPOO_REG_BTN_MIDDLE;     /* 0x0604 */
        case 5:  return RAPOO_REG_BTN_TOP_DPI;    /* 0x060c */
        case 6:  return RAPOO_REG_BTN_DPI_MINUS;  /* 0x0610 */
        case 7:  return RAPOO_REG_BTN_FORWARD;    /* 0x0614 */
        case 8:  return RAPOO_REG_BTN_BACK;       /* 0x0618 */
        case 9:  return RAPOO_REG_BTN_EXTRA9;     /* 0x0638 */
        case 10: return RAPOO_REG_BTN_PRO_DIY;    /* 0x0620 */
        case 11: return RAPOO_REG_BTN_WHEEL_UP;   /* 0x0624 */
        case 12: return RAPOO_REG_BTN_WHEEL_DOWN; /* 0x0628 */
        case 13: return RAPOO_REG_BTN_VT9_DIY;    /* 0x0634 */
        default: return 0;
    }
}

const char* rapoo_get_button_name(uint8_t button_id) {
    switch (button_id) {
        case 0:  return "Left Click (좌클릭)";
        case 1:  return "Right Click (우클릭)";
        case 2:  return "Middle Click (휠 클릭)";
        case 5:  return "Top DPI Button (상단 DPI / Key 5)";
        case 6:  return "DPI Minus (DPI 감소)";
        case 7:  return "Forward (앞으로 가기 / 측면 앞)";
        case 8:  return "Back (뒤로 가기 / 측면 뒤)";
        case 9:  return "Extra Button 9 (추가 버튼 9)";
        case 10: return "Extra Button 10 (추가 버튼 10)";
        case 11: return "Wheel Up (휠 위로 스크롤)";
        case 12: return "Wheel Down (휠 아래로 스크롤)";
        case 13: return "Bottom DIY Button (바닥면 DIY 버튼)";
        default: return "Unknown Button";
    }
}

const char* rapoo_get_function_name(uint8_t func_id) {
    switch (func_id) {
        case RAPOO_FUNC_LEFT:       return "Left Click (좌클릭)";
        case RAPOO_FUNC_MIDDLE:     return "Middle Click (휠 클릭)";
        case RAPOO_FUNC_RIGHT:      return "Right Click (우클릭)";
        case RAPOO_FUNC_FORWARD:    return "Forward (앞으로 가기)";
        case RAPOO_FUNC_BACK:       return "Back (뒤로 가기)";
        case RAPOO_FUNC_SNIPER:     return "Sniper Mode (스나이퍼 모드)";
        case RAPOO_FUNC_FIRE:       return "Fire / Rapid (연사 모드)";
        case RAPOO_FUNC_DPI_CYCLE:  return "DPI Cycle (DPI 순환)";
        case RAPOO_FUNC_WHEEL_UP:   return "Wheel Up (휠 위로 스크롤)";
        case RAPOO_FUNC_WHEEL_DOWN: return "Wheel Down (휠 아래로 스크롤)";
        case RAPOO_FUNC_DPI_PLUS:   return "DPI+ (DPI 단계 증가)";
        case RAPOO_FUNC_DPI_MINUS:  return "DPI- (DPI 단계 감소)";
        case 29:
        case RAPOO_FUNC_MACRO:      return "Hardware Macro (온보드 하드웨어 매크로)";
        default:                    return "Custom / Unknown";
    }
}

static void encode_function_payload(uint8_t func_id, uint8_t param, uint8_t out[4]) {
    memset(out, 0, 4);
    switch (func_id) {
        case RAPOO_FUNC_LEFT:       /* 9 */
            out[0] = 0x03; out[2] = 0x01; break;
        case RAPOO_FUNC_MIDDLE:     /* 10 */
            out[0] = 0x03; out[2] = 0x04; break;
        case RAPOO_FUNC_RIGHT:      /* 11 */
            out[0] = 0x03; out[2] = 0x02; break;
        case RAPOO_FUNC_FORWARD:    /* 12 */
            out[0] = 0x03; out[2] = 0x10; break;
        case RAPOO_FUNC_BACK:       /* 13 */
            out[0] = 0x03; out[2] = 0x08; break;
        case RAPOO_FUNC_SNIPER:     /* 26 */
            out[0] = 0x09; out[2] = 0x01; break;
        case RAPOO_FUNC_FIRE:       /* 27 */
            out[0] = 0x03; out[2] = 0x01; break;
        case RAPOO_FUNC_DPI_CYCLE:  /* 36 */
            out[0] = 0x08; out[2] = 0x03; break;
        case RAPOO_FUNC_WHEEL_UP:   /* 37 */
        case RAPOO_FUNC_WHEEL_DOWN: /* 38 */
            memset(out, 0xff, 4); break;
        case RAPOO_FUNC_DPI_PLUS:   /* 39 */
            out[0] = 0x08; out[2] = 0x05; break;
        case RAPOO_FUNC_DPI_MINUS:  /* 40 */
            out[0] = 0x08; out[2] = 0x06; break;
        case 29:
        case RAPOO_FUNC_MACRO:      /* 41 */
            out[0] = 0x05;
            out[2] = (param <= 5) ? param : 0;
            break;
        default:
            out[0] = 0x03; out[2] = 0x01; break;
    }
}

int rapoo_find_device(char *out_path, size_t max_len) {
    if (!out_path || max_len < 16) return -1;

    DIR *dir = opendir("/dev");
    if (!dir) return -1;

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "hidraw", 6) != 0) continue;

        char rdesc_path[512];
        snprintf(rdesc_path, sizeof(rdesc_path),
                 "/sys/class/hidraw/%s/device/report_descriptor", entry->d_name);

        int fd = open(rdesc_path, O_RDONLY);
        if (fd >= 0) {
            uint8_t rdesc[1024];
            ssize_t n = read(fd, rdesc, sizeof(rdesc));
            close(fd);

            if (n > 4) {
                /* Check for Report ID 0xba (0x85 0xba) */
                for (ssize_t i = 0; i < n - 1; ++i) {
                    if (rdesc[i] == 0x85 && rdesc[i + 1] == 0xba) {
                        snprintf(out_path, max_len, "/dev/%s", entry->d_name);
                        closedir(dir);
                        return 0;
                    }
                }
            }
        }
    }
    closedir(dir);
    return -1;
}

int rapoo_open(rapoo_device_t *dev, const char *path) {
    if (!dev) return -1;
    memset(dev, 0, sizeof(*dev));
    dev->fd = -1;

    char resolved_path[128];
    if (path && path[0] != '\0') {
        snprintf(resolved_path, sizeof(resolved_path), "%s", path);
    } else {
        if (rapoo_find_device(resolved_path, sizeof(resolved_path)) != 0) {
            return -1;
        }
    }

    int fd = open(resolved_path, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        return -1;
    }

    dev->fd = fd;
    snprintf(dev->path, sizeof(dev->path), "%s", resolved_path);
    dev->connected = true;
    return 0;
}

void rapoo_close(rapoo_device_t *dev) {
    if (!dev) return;
    if (dev->fd >= 0) {
        close(dev->fd);
        dev->fd = -1;
    }
    dev->connected = false;
}

int rapoo_send_report(rapoo_device_t *dev, const uint8_t *report, size_t len) {
    if (!dev || dev->fd < 0 || !report || len != 32) {
        return -1;
    }

    for (int attempt = 0; attempt < 3; ++attempt) {
        /* IOCTL HIDIOCSOUTPUT(32) */
        if (ioctl(dev->fd, RAPOO_HIDIOCSOUTPUT_32, report) >= 0) {
            usleep(40000); /* 40ms settling time */
            return 0;
        }

        /* Fallback: direct write() */
        ssize_t n = write(dev->fd, report, 32);
        if (n == 32) {
            usleep(40000);
            return 0;
        }

        usleep(50000); /* 50ms before retry */
    }

    return -1;
}

int rapoo_write_register(rapoo_device_t *dev, uint32_t address, const uint8_t *payload, size_t len) {
    if (!dev || dev->fd < 0 || !payload || len > 24) {
        return -1;
    }

    uint8_t packet[32];
    memset(packet, 0, sizeof(packet));

    packet[0] = RAPOO_REPORT_ID_OUT; /* 0xba */
    packet[1] = RAPOO_DEV_CONST;     /* 0xa5 */
    packet[2] = RAPOO_OPCODE_WRITE;  /* 0xa5 */
    packet[3] = (uint8_t)len;

    /* Little-endian 32-bit register address */
    packet[4] = (uint8_t)(address & 0xff);
    packet[5] = (uint8_t)((address >> 8) & 0xff);
    packet[6] = (uint8_t)((address >> 16) & 0xff);
    packet[7] = (uint8_t)((address >> 24) & 0xff);

    memcpy(&packet[8], payload, len);

    return rapoo_send_report(dev, packet, 32);
}

int rapoo_commit(rapoo_device_t *dev) {
    if (!dev || dev->fd < 0) return -1;

    uint8_t packet[32];
    memset(packet, 0, sizeof(packet));

    packet[0] = RAPOO_REPORT_ID_OUT;  /* 0xba */
    packet[1] = RAPOO_DEV_CONST;      /* 0xa5 */
    packet[2] = RAPOO_OPCODE_COMMIT; /* 0xa1 */
    packet[3] = 0x01;                 /* Value 1: Enable & Save to EEPROM */

    return rapoo_send_report(dev, packet, 32);
}

int rapoo_set_dpi_stages(rapoo_device_t *dev, const uint16_t *stages, size_t count, uint8_t active_idx) {
    if (!dev || dev->fd < 0 || !stages || count < 1 || count > 7) {
        return -1;
    }

    uint16_t padded[7];
    for (size_t i = 0; i < 7; ++i) {
        if (i < count) {
            padded[i] = stages[i];
        } else {
            padded[i] = stages[count - 1];
        }
    }

    uint8_t payload[16];
    memset(payload, 0, sizeof(payload));

    for (size_t i = 0; i < 7; ++i) {
        uint16_t d = padded[i];
        if (d < 50) d = 50;
        if (d > 45000) d = 45000;
        payload[i * 2]     = (uint8_t)(d & 0xff);
        payload[i * 2 + 1] = (uint8_t)((d >> 8) & 0xff);
    }

    /* byte 14: active stages count - 1 */
    payload[14] = (uint8_t)(count - 1);
    /* byte 15: active index */
    payload[15] = (active_idx < count) ? active_idx : (uint8_t)(count - 1);

    /* 1. X-Axis DPI (0x0888) */
    if (rapoo_write_register(dev, RAPOO_REG_DPI_X, payload, 16) != 0) return -1;

    /* 2. Y-Axis DPI (0x08c8) */
    if (rapoo_write_register(dev, RAPOO_REG_DPI_Y, payload, 16) != 0) return -1;

    /* 3. Active DPI Stage Index (0x0898) */
    uint8_t idx_payload[4] = {payload[15], 0x00, 0x00, 0x00};
    if (rapoo_write_register(dev, RAPOO_REG_ACTIVE_DPI, idx_payload, 4) != 0) return -1;

    /* 4. Hardware commit */
    return rapoo_commit(dev);
}

int rapoo_set_polling_rate(rapoo_device_t *dev, uint16_t rate_hz) {
    if (!dev || dev->fd < 0) return -1;

    uint8_t code;
    switch (rate_hz) {
        case 125:  code = 0x08; break;
        case 250:  code = 0x04; break;
        case 500:  code = 0x02; break;
        case 1000: code = 0x81; break;
        case 2000: code = 0x82; break;
        case 4000: code = 0x84; break;
        case 8000: code = 0x01; break;
        default:   return -1;
    }

    uint8_t payload[4] = {code, 0x00, 0x00, 0x00};
    if (rapoo_write_register(dev, RAPOO_REG_POLLING, payload, 4) != 0) return -1;

    return rapoo_commit(dev);
}

int rapoo_set_sensor(rapoo_device_t *dev, float lod_mm, uint8_t debounce_ms) {
    if (!dev || dev->fd < 0) return -1;

    uint8_t debounce_code = 2;
    if (debounce_ms <= 1) debounce_code = 1;
    else if (debounce_ms <= 5) debounce_code = 2;
    else debounce_code = 3;

    int lod_val = (int)roundf(lod_mm * 10.0f);
    if (lod_val < 7) lod_val = 7;
    if (lod_val > 17) lod_val = 17;
    uint8_t lod_code = (uint8_t)lod_val;

    /* Byte 0: debounce, Byte 1: LOD, Byte 2: Angle Snapping (0), Byte 3: Motion Sync (1) */
    uint8_t payload[4] = {debounce_code, lod_code, 0x00, 0x01};
    if (rapoo_write_register(dev, RAPOO_REG_SENSOR, payload, 4) != 0) return -1;

    return rapoo_commit(dev);
}

int rapoo_remap_button(rapoo_device_t *dev, uint8_t button_id, uint8_t func_id, uint8_t param) {
    if (!dev || dev->fd < 0) return -1;

    uint32_t reg_addr = rapoo_get_button_register(button_id);
    if (reg_addr == 0) return -1;

    uint8_t payload[4];
    encode_function_payload(func_id, param, payload);

    if (rapoo_write_register(dev, reg_addr, payload, 4) != 0) return -1;

    /* Mirroring for Key 13 (Bottom DIY Button) across both Pro and VT9 variants */
    if (button_id == 13) {
        rapoo_write_register(dev, 0x0620, payload, 4);
        rapoo_write_register(dev, 0x0634, payload, 4);
    }

    return rapoo_commit(dev);
}

int rapoo_upload_macro(rapoo_device_t *dev, uint8_t slot_idx, const uint8_t *bytecode, size_t len) {
    if (!dev || dev->fd < 0 || slot_idx > 5 || !bytecode || len == 0 || len > 4096) {
        return -1;
    }

    uint32_t slot_addr = RAPOO_REG_MACRO_BASE + ((uint32_t)slot_idx * RAPOO_MACRO_SLOT_SIZE);
    size_t chunk_size = 24;

    for (size_t offset = 0; offset < len; offset += chunk_size) {
        size_t this_chunk = len - offset;
        if (this_chunk > chunk_size) this_chunk = chunk_size;

        if (rapoo_write_register(dev, slot_addr + (uint32_t)offset, bytecode + offset, this_chunk) != 0) {
            return -1;
        }
        usleep(20000); /* 20ms */
    }

    /* Required hardware settling time for MCU EEPROM commit */
    usleep(500000);

    return rapoo_commit(dev);
}

int rapoo_get_telemetry(rapoo_device_t *dev, rapoo_telemetry_t *out_telem, double timeout_sec) {
    if (!dev || dev->fd < 0 || !out_telem) return -1;

    fd_set read_fds;
    struct timeval tv;
    tv.tv_sec = (time_t)timeout_sec;
    tv.tv_usec = (suseconds_t)((timeout_sec - (double)tv.tv_sec) * 1000000.0);

    FD_ZERO(&read_fds);
    FD_SET(dev->fd, &read_fds);

    int ret = select(dev->fd + 1, &read_fds, NULL, NULL, &tv);
    if (ret > 0 && FD_ISSET(dev->fd, &read_fds)) {
        uint8_t buf[64];
        ssize_t n = read(dev->fd, buf, sizeof(buf));
        if (n >= 7 && buf[0] == RAPOO_REPORT_ID_IN) {
            memset(out_telem, 0, sizeof(*out_telem));
            memcpy(out_telem->raw_data, buf, (size_t)n);

            out_telem->is_charging = (buf[5] == 0x02);
            out_telem->current_dpi = (uint16_t)(buf[3] | (buf[4] << 8));

            uint8_t bat = buf[6];
            if (bat > 100 && n >= 8) bat = buf[7];
            if (bat > 100) bat = 100;
            out_telem->battery_percent = bat;

            return 0;
        }
    }

    return -1;
}

int rapoo_encode_macro_string(const char *macro_str, int default_delay_ms, int mode, int repeat,
                              uint8_t *out_buf, size_t max_out_len, size_t *out_len) {
    if (!macro_str || !out_buf || max_out_len < 32 || !out_len) return -1;

    /* Parse tokens separated by '+' or ',' or spaces */
    char str_copy[512];
    strncpy(str_copy, macro_str, sizeof(str_copy) - 1);
    str_copy[sizeof(str_copy) - 1] = '\0';

    for (char *p = str_copy; *p; ++p) {
        if (*p == ',' || *p == '+') *p = ' ';
    }

    char *tokens[32];
    int token_count = 0;
    char *saveptr = NULL;
    char *token = strtok_r(str_copy, " \t\r\n", &saveptr);
    while (token && token_count < 32) {
        tokens[token_count++] = token;
        token = strtok_r(NULL, " \t\r\n", &saveptr);
    }

    if (token_count == 0) return -1;

    /* Check if it's a modifier combo like ctrl + c */
    bool is_combo = false;
    for (int i = 0; i < token_count; ++i) {
        char lower[16];
        size_t k;
        for (k = 0; tokens[i][k] && k < sizeof(lower) - 1; ++k) {
            lower[k] = (char)tolower((unsigned char)tokens[i][k]);
        }
        lower[k] = '\0';
        if (strcmp(lower, "ctrl") == 0 || strcmp(lower, "shift") == 0 ||
            strcmp(lower, "alt") == 0 || strcmp(lower, "win") == 0) {
            is_combo = true;
            break;
        }
    }

    uint8_t actions[2048];
    size_t act_len = 0;
    uint16_t curr_delay = (default_delay_ms < 10) ? 50 : (uint16_t)default_delay_ms;

    if (is_combo) {
        /* Press all in order */
        for (int i = 0; i < token_count; ++i) {
            uint8_t code = lookup_hid_key(tokens[i]);
            if (code && act_len + 3 < sizeof(actions)) {
                uint16_t val = (20 << 1) | 0; /* Down */
                actions[act_len++] = (uint8_t)(val & 0xff);
                actions[act_len++] = (uint8_t)((val >> 8) & 0xff);
                actions[act_len++] = code;
            }
        }
        /* Hold delay */
        if (act_len >= 3) {
            uint16_t val = (curr_delay << 1) | (actions[act_len - 3] & 0x01);
            actions[act_len - 3] = (uint8_t)(val & 0xff);
            actions[act_len - 2] = (uint8_t)((val >> 8) & 0xff);
        }
        /* Release all in reverse order */
        for (int i = token_count - 1; i >= 0; --i) {
            uint8_t code = lookup_hid_key(tokens[i]);
            if (code && act_len + 3 < sizeof(actions)) {
                uint16_t val = (20 << 1) | 1; /* Up */
                actions[act_len++] = (uint8_t)(val & 0xff);
                actions[act_len++] = (uint8_t)((val >> 8) & 0xff);
                actions[act_len++] = code;
            }
        }
    } else {
        /* Sequential execution: press -> delay -> release -> delay */
        for (int i = 0; i < token_count; ++i) {
            size_t tlen = strlen(tokens[i]);
            if (tlen > 2 && strcmp(tokens[i] + tlen - 2, "ms") == 0) {
                int ms = atoi(tokens[i]);
                if (ms > 0 && act_len >= 3) {
                    curr_delay = (uint16_t)ms;
                    uint16_t val = (curr_delay << 1) | (actions[act_len - 3] & 0x01);
                    actions[act_len - 3] = (uint8_t)(val & 0xff);
                    actions[act_len - 2] = (uint8_t)((val >> 8) & 0xff);
                }
                continue;
            }

            uint8_t code = lookup_hid_key(tokens[i]);
            if (code && act_len + 6 < sizeof(actions)) {
                /* Down */
                uint16_t val_down = (curr_delay << 1) | 0;
                actions[act_len++] = (uint8_t)(val_down & 0xff);
                actions[act_len++] = (uint8_t)((val_down >> 8) & 0xff);
                actions[act_len++] = code;

                /* Up */
                uint16_t val_up = (curr_delay << 1) | 1;
                actions[act_len++] = (uint8_t)(val_up & 0xff);
                actions[act_len++] = (uint8_t)((val_up >> 8) & 0xff);
                actions[act_len++] = code;
            }
        }
    }

    /* Terminate with 3 zero bytes */
    actions[act_len++] = 0x00;
    actions[act_len++] = 0x00;
    actions[act_len++] = 0x00;

    uint16_t stream_len = (uint16_t)(1 + act_len);
    size_t total_len = 4 + act_len;

    /* Pad to multiple of 24 */
    size_t padded_len = total_len;
    if (padded_len % 24 != 0) {
        padded_len += (24 - (padded_len % 24));
    }

    if (padded_len > max_out_len) return -1;

    memset(out_buf, 0, padded_len);

    /* stream_len uint16 LE */
    out_buf[0] = (uint8_t)(stream_len & 0xff);
    out_buf[1] = (uint8_t)((stream_len >> 8) & 0xff);

    /* repeat mode uint16 LE */
    if (mode == 2) {
        out_buf[2] = 0xfe; out_buf[3] = 0xff; /* Hold */
    } else if (mode == 3) {
        out_buf[2] = 0xff; out_buf[3] = 0xff; /* Toggle */
    } else {
        uint16_t rep = (repeat < 1) ? 1 : (uint16_t)repeat;
        out_buf[2] = (uint8_t)(rep & 0xff);
        out_buf[3] = (uint8_t)((rep >> 8) & 0xff);
    }

    memcpy(&out_buf[4], actions, act_len);

    *out_len = padded_len;
    return 0;
}

void rapoo_get_default_profile(rapoo_profile_t *prof) {
    if (!prof) return;
    memset(prof, 0, sizeof(*prof));

    prof->dpi_stages[0] = 800;
    prof->dpi_stages[1] = 1200;
    prof->dpi_stages[2] = 1600;
    prof->dpi_stages[3] = 2000;
    prof->dpi_stages[4] = 2400;
    prof->dpi_stages[5] = 3200;
    prof->dpi_stages[6] = 6400;
    prof->stage_count = 7;
    prof->active_dpi_stage = 3; /* 2000 DPI default */

    prof->polling_rate_hz = 1000;
    prof->lod_mm = 1.1f;
    prof->debounce_ms = 2;

    /* Defaults */
    prof->button_mappings[0]  = RAPOO_FUNC_LEFT;       /* 9 */
    prof->button_mappings[1]  = RAPOO_FUNC_RIGHT;      /* 11 */
    prof->button_mappings[2]  = RAPOO_FUNC_MIDDLE;     /* 10 */
    prof->button_mappings[5]  = RAPOO_FUNC_DPI_CYCLE;  /* 36 */
    prof->button_mappings[6]  = RAPOO_FUNC_DPI_MINUS;  /* 40 */
    prof->button_mappings[7]  = RAPOO_FUNC_FORWARD;    /* 12 */
    prof->button_mappings[8]  = RAPOO_FUNC_BACK;       /* 13 */
    prof->button_mappings[11] = RAPOO_FUNC_WHEEL_UP;   /* 37 */
    prof->button_mappings[12] = RAPOO_FUNC_WHEEL_DOWN; /* 38 */
    prof->button_mappings[13] = RAPOO_FUNC_DPI_CYCLE;  /* 36 */
}

/* Lightweight JSON Profile Parser & Serializer */
int rapoo_load_profile(const char *path, rapoo_profile_t *prof) {
    if (!prof) return -1;
    rapoo_get_default_profile(prof);

    char filepath[256];
    if (path && path[0] != '\0') {
        strncpy(filepath, path, sizeof(filepath) - 1);
        filepath[sizeof(filepath) - 1] = '\0';
    } else {
        const char *home = getenv("HOME");
        if (!home) return -1;
        snprintf(filepath, sizeof(filepath), "%s/.config/rapoo-mouse/active_profile.json", home);
    }

    FILE *f = fopen(filepath, "r");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0 || fsize > 65536) {
        fclose(f);
        return -1;
    }

    char *buf = (char *)malloc((size_t)fsize + 1);
    if (!buf) {
        fclose(f);
        return -1;
    }

    size_t read_bytes = fread(buf, 1, (size_t)fsize, f);
    fclose(f);
    buf[read_bytes] = '\0';

    /* Parse dpi_stages */
    char *p = strstr(buf, "\"dpi_stages\"");
    if (p) {
        char *bracket = strchr(p, '[');
        if (bracket) {
            bracket++;
            size_t count = 0;
            while (*bracket && *bracket != ']' && count < 7) {
                while (*bracket && (isspace((unsigned char)*bracket) || *bracket == ',')) bracket++;
                if (isdigit((unsigned char)*bracket)) {
                    prof->dpi_stages[count++] = (uint16_t)strtoul(bracket, &bracket, 10);
                } else {
                    break;
                }
            }
            if (count > 0) prof->stage_count = (uint8_t)count;
        }
    }

    /* Parse active_dpi_stage */
    p = strstr(buf, "\"active_dpi_stage\"");
    if (p) {
        char *colon = strchr(p, ':');
        if (colon) prof->active_dpi_stage = (uint8_t)atoi(colon + 1);
    }

    /* Parse polling_rate_hz */
    p = strstr(buf, "\"polling_rate_hz\"");
    if (p) {
        char *colon = strchr(p, ':');
        if (colon) prof->polling_rate_hz = (uint16_t)atoi(colon + 1);
    }

    /* Parse lod_mm */
    p = strstr(buf, "\"lod_mm\"");
    if (p) {
        char *colon = strchr(p, ':');
        if (colon) prof->lod_mm = (float)atof(colon + 1);
    }

    /* Parse debounce_ms */
    p = strstr(buf, "\"debounce_ms\"");
    if (p) {
        char *colon = strchr(p, ':');
        if (colon) prof->debounce_ms = (uint8_t)atoi(colon + 1);
    }

    /* Parse button_mappings */
    p = strstr(buf, "\"button_mappings\"");
    if (p) {
        char *brace = strchr(p, '{');
        if (brace) {
            char *end_brace = strchr(brace, '}');
            if (end_brace) {
                char *curr = brace + 1;
                while (curr < end_brace) {
                    char *quote1 = strchr(curr, '"');
                    if (!quote1 || quote1 >= end_brace) break;
                    char *quote2 = strchr(quote1 + 1, '"');
                    if (!quote2 || quote2 >= end_brace) break;
                    int btn_id = atoi(quote1 + 1);
                    char *colon = strchr(quote2, ':');
                    if (colon && colon < end_brace) {
                        int func_id = atoi(colon + 1);
                        if (btn_id >= 0 && btn_id < 16) {
                            prof->button_mappings[btn_id] = (uint8_t)func_id;
                        }
                    }
                    curr = quote2 + 1;
                }
            }
        }
    }

    /* Parse button_params */
    p = strstr(buf, "\"button_params\"");
    if (p) {
        char *brace = strchr(p, '{');
        if (brace) {
            char *end_brace = strchr(brace, '}');
            if (end_brace) {
                char *curr = brace + 1;
                while (curr < end_brace) {
                    char *quote1 = strchr(curr, '"');
                    if (!quote1 || quote1 >= end_brace) break;
                    char *quote2 = strchr(quote1 + 1, '"');
                    if (!quote2 || quote2 >= end_brace) break;
                    int btn_id = atoi(quote1 + 1);
                    char *colon = strchr(quote2, ':');
                    if (colon && colon < end_brace) {
                        int param = atoi(colon + 1);
                        if (btn_id >= 0 && btn_id < 16) {
                            prof->button_params[btn_id] = (uint8_t)param;
                        }
                    }
                    curr = quote2 + 1;
                }
            }
        }
    }

    free(buf);
    return 0;
}

int rapoo_save_profile(const char *path, const rapoo_profile_t *prof) {
    if (!prof) return -1;

    char filepath[256];
    if (path && path[0] != '\0') {
        strncpy(filepath, path, sizeof(filepath) - 1);
        filepath[sizeof(filepath) - 1] = '\0';
    } else {
        const char *home = getenv("HOME");
        if (!home) return -1;
        snprintf(filepath, sizeof(filepath), "%s/.config/rapoo-mouse/active_profile.json", home);
    }

    /* Ensure parent directories safely in C without spawning shell */
    char dirpath[256];
    snprintf(dirpath, sizeof(dirpath), "%s", filepath);
    char *last_slash = strrchr(dirpath, '/');
    if (last_slash) {
        *last_slash = '\0';
        for (char *p = dirpath + 1; *p; p++) {
            if (*p == '/') {
                *p = '\0';
                mkdir(dirpath, 0755);
                *p = '/';
            }
        }
        mkdir(dirpath, 0755);
    }

    FILE *f = fopen(filepath, "w");
    if (!f) return -1;

    fprintf(f, "{\n");
    fprintf(f, "  \"dpi_stages\": [\n");
    for (size_t i = 0; i < prof->stage_count; ++i) {
        fprintf(f, "    %u%s\n", prof->dpi_stages[i], (i + 1 < prof->stage_count) ? "," : "");
    }
    fprintf(f, "  ],\n");
    fprintf(f, "  \"active_dpi_stage\": %u,\n", prof->active_dpi_stage);
    fprintf(f, "  \"polling_rate_hz\": %u,\n", prof->polling_rate_hz);
    fprintf(f, "  \"lod_mm\": %.1f,\n", prof->lod_mm);
    fprintf(f, "  \"debounce_ms\": %u,\n", prof->debounce_ms);

    fprintf(f, "  \"button_mappings\": {\n");
    bool first = true;
    for (int i = 0; i < 16; ++i) {
        if (prof->button_mappings[i] != 0) {
            fprintf(f, "%s    \"%d\": %u", first ? "" : ",\n", i, prof->button_mappings[i]);
            first = false;
        }
    }
    fprintf(f, "\n  },\n");

    fprintf(f, "  \"button_params\": {\n");
    first = true;
    for (int i = 0; i < 16; ++i) {
        if (prof->button_params[i] != 0) {
            fprintf(f, "%s    \"%d\": %u", first ? "" : ",\n", i, prof->button_params[i]);
            first = false;
        }
    }
    fprintf(f, "\n  }\n");
    fprintf(f, "}\n");

    fclose(f);
    return 0;
}

int rapoo_apply_profile(rapoo_device_t *dev, const rapoo_profile_t *prof) {
    if (!dev || dev->fd < 0 || !prof) return -1;

    /* 1. DPI Stages */
    if (prof->stage_count > 0) {
        rapoo_set_dpi_stages(dev, prof->dpi_stages, prof->stage_count, prof->active_dpi_stage);
    }

    /* 2. Polling Rate */
    if (prof->polling_rate_hz > 0) {
        rapoo_set_polling_rate(dev, prof->polling_rate_hz);
    }

    /* 3. Sensor */
    rapoo_set_sensor(dev, prof->lod_mm, prof->debounce_ms);

    /* 4. Button Remappings */
    for (uint8_t i = 0; i < 16; ++i) {
        if (prof->button_mappings[i] != 0) {
            rapoo_remap_button(dev, i, prof->button_mappings[i], prof->button_params[i]);
        }
    }

    return 0;
}
