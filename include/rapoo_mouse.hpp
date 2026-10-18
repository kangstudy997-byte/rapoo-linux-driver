#ifndef RAPOO_MOUSE_HPP
#define RAPOO_MOUSE_HPP

#include "rapoo_driver.h"
#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <stdexcept>

namespace rapoo {

struct Telemetry {
    uint8_t battery;
    bool charging;
    uint16_t dpi;
    uint16_t pollingHz;
};

class Mouse {
public:
    Mouse() {
        m_dev.fd = -1;
        m_dev.connected = false;
        m_dev.path[0] = '\0';
    }

    explicit Mouse(const std::string& customPath) : Mouse() {
        open(customPath);
    }

    ~Mouse() {
        close();
    }

    // Non-copyable, movable
    Mouse(const Mouse&) = delete;
    Mouse& operator=(const Mouse&) = delete;
    Mouse(Mouse&& other) noexcept {
        m_dev = other.m_dev;
        other.m_dev.fd = -1;
        other.m_dev.connected = false;
    }
    Mouse& operator=(Mouse&& other) noexcept {
        if (this != &other) {
            close();
            m_dev = other.m_dev;
            other.m_dev.fd = -1;
            other.m_dev.connected = false;
        }
        return *this;
    }

    bool open(const std::string& path = "") {
        if (isOpen()) close();
        int rc = rapoo_open(&m_dev, path.empty() ? nullptr : path.c_str());
        return (rc == 0);
    }

    void close() {
        if (isOpen()) {
            rapoo_close(&m_dev);
        }
    }

    bool isOpen() const {
        return m_dev.connected && m_dev.fd >= 0;
    }

    const char* getDevicePath() const {
        return m_dev.path;
    }

    bool setDpiStages(const std::vector<uint16_t>& stages, uint8_t activeIdx = 0) {
        if (stages.empty() || stages.size() > 7) {
            throw std::invalid_argument("DPI stages count must be between 1 and 7");
        }
        return rapoo_set_dpi_stages(&m_dev, stages.data(), stages.size(), activeIdx) == 0;
    }

    bool setPollingRate(uint16_t rateHz) {
        return rapoo_set_polling_rate(&m_dev, rateHz) == 0;
    }

    bool setSensor(float lodMm = 1.1f, uint8_t debounceMs = 2) {
        return rapoo_set_sensor(&m_dev, lodMm, debounceMs) == 0;
    }

    bool remapButton(uint8_t buttonId, uint8_t funcId, uint8_t param = 0) {
        return rapoo_remap_button(&m_dev, buttonId, funcId, param) == 0;
    }

    bool uploadMacro(uint8_t slotIdx, const std::vector<uint8_t>& bytecode) {
        return rapoo_upload_macro(&m_dev, slotIdx, bytecode.data(), bytecode.size()) == 0;
    }

    bool commit() {
        return rapoo_commit(&m_dev) == 0;
    }

    std::optional<Telemetry> getTelemetry(double timeoutSec = 1.5) {
        rapoo_telemetry_t raw;
        if (rapoo_get_telemetry(&m_dev, &raw, timeoutSec) == 0) {
            return Telemetry{
                raw.battery_percent,
                raw.is_charging,
                raw.current_dpi,
                raw.polling_rate_hz
            };
        }
        return std::nullopt;
    }

private:
    rapoo_device_t m_dev;
};

} // namespace rapoo

#endif // RAPOO_MOUSE_HPP
