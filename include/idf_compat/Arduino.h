#pragma once

// Minimal Arduino API shim for ESP-IDF host builds (stores / Logger / PsramJsonBuffer).

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class __FlashStringHelper;
#define F(string_literal) (reinterpret_cast<const __FlashStringHelper *>(string_literal))
#define FPSTR(pstr_pointer) (reinterpret_cast<const __FlashStringHelper *>(pstr_pointer))

#ifndef HEX
#define HEX 16
#endif
#ifndef DEC
#define DEC 10
#endif
#ifndef byte
typedef uint8_t byte;
#endif
#ifdef __cplusplus
using boolean = bool;
#endif

#ifndef HIGH
#define HIGH 0x1
#endif
#ifndef LOW
#define LOW 0x0
#endif
#ifndef INPUT
#define INPUT 0x01
#endif
#ifndef OUTPUT
#define OUTPUT 0x03
#endif
#ifndef INPUT_PULLUP
#define INPUT_PULLUP 0x05
#endif

inline void pinMode(int pin, int mode) {
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << pin;
    config.mode = (mode == OUTPUT) ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT;
    config.pull_up_en = (mode == INPUT_PULLUP) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&config);
}

inline void digitalWrite(int pin, int level) {
    gpio_set_level(static_cast<gpio_num_t>(pin), level ? 1 : 0);
}

inline int digitalRead(int pin) {
    return gpio_get_level(static_cast<gpio_num_t>(pin));
}

inline uint32_t millis(void) {
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

inline uint32_t micros(void) {
    return millis() * 1000UL;
}

inline void delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms == 0 ? 1 : ms));
}

inline void yield(void) {
    taskYIELD();
}

inline bool ledcAttach(int /*pin*/, int /*freq*/, int /*resolution*/) {
    return true;
}

inline void ledcWrite(int /*pin*/, int /*duty*/) {}

inline void rgbLedWrite(int /*pin*/, uint8_t /*r*/, uint8_t /*g*/, uint8_t /*b*/) {}

class String {
public:
    String() : buf(nullptr), len(0), cap(0) {}

    String(const char *cstr) : buf(nullptr), len(0), cap(0) {
        if (cstr != nullptr) {
            copyFrom(cstr, strlen(cstr));
        }
    }

    String(const __FlashStringHelper *flash) : String(reinterpret_cast<const char *>(flash)) {}

    String(const String &other) : buf(nullptr), len(0), cap(0) {
        copyFrom(other.buf, other.len);
    }

    String(String &&other) noexcept : buf(other.buf), len(other.len), cap(other.cap) {
        other.buf = nullptr;
        other.len = 0;
        other.cap = 0;
    }

    explicit String(char character) : buf(nullptr), len(0), cap(0) {
        char tmp[2] = {character, '\0'};
        copyFrom(tmp, 1);
    }

    explicit String(int value) : buf(nullptr), len(0), cap(0) {
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "%d", value);
        copyFrom(tmp, strlen(tmp));
    }

    explicit String(unsigned int value) : buf(nullptr), len(0), cap(0) {
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "%u", value);
        copyFrom(tmp, strlen(tmp));
    }

    explicit String(long value) : buf(nullptr), len(0), cap(0) {
        char tmp[24];
        snprintf(tmp, sizeof(tmp), "%ld", value);
        copyFrom(tmp, strlen(tmp));
    }

    explicit String(unsigned long value) : buf(nullptr), len(0), cap(0) {
        char tmp[24];
        snprintf(tmp, sizeof(tmp), "%lu", value);
        copyFrom(tmp, strlen(tmp));
    }

    String(unsigned long value, unsigned char base) : buf(nullptr), len(0), cap(0) {
        char tmp[34];
        if (base == 16) {
            snprintf(tmp, sizeof(tmp), "%lX", value);
        } else if (base == 8) {
            snprintf(tmp, sizeof(tmp), "%lo", value);
        } else {
            snprintf(tmp, sizeof(tmp), "%lu", value);
        }
        copyFrom(tmp, strlen(tmp));
    }

    String(int value, unsigned char base) : String((unsigned long)(value < 0 ? -value : value), base) {
        if (value < 0 && base == 10) {
            String neg = "-";
            neg += *this;
            *this = neg;
        }
    }

    explicit String(float value, unsigned int decimals = 2) : buf(nullptr), len(0), cap(0) {
        char tmp[48];
        snprintf(tmp, sizeof(tmp), "%.*f", (int)decimals, (double)value);
        copyFrom(tmp, strlen(tmp));
    }

    ~String() {
        free(buf);
    }

    String &operator=(const String &other) {
        if (this != &other) {
            copyFrom(other.buf, other.len);
        }
        return *this;
    }

    String &operator=(String &&other) noexcept {
        if (this != &other) {
            free(buf);
            buf = other.buf;
            len = other.len;
            cap = other.cap;
            other.buf = nullptr;
            other.len = 0;
            other.cap = 0;
        }
        return *this;
    }

    String &operator=(const char *cstr) {
        if (cstr == nullptr) {
            clear();
        } else {
            copyFrom(cstr, strlen(cstr));
        }
        return *this;
    }

    String &operator=(char character) {
        char tmp[2] = {character, '\0'};
        copyFrom(tmp, 1);
        return *this;
    }

    bool reserve(unsigned int size) {
        return ensureCap((size_t)size);
    }

    unsigned int length() const {
        return (unsigned int)len;
    }

    const char *c_str() const {
        return buf != nullptr ? buf : "";
    }

    bool isEmpty() const {
        return len == 0;
    }

    void clear() {
        len = 0;
        if (buf != nullptr) {
            buf[0] = '\0';
        }
    }

    char charAt(unsigned int index) const {
        if (buf == nullptr || index >= len) {
            return '\0';
        }
        return buf[index];
    }

    char operator[](unsigned int index) const {
        return charAt(index);
    }

    char &operator[](unsigned int index) {
        static char sink = '\0';
        if (buf == nullptr || index >= len) {
            return sink;
        }
        return buf[index];
    }

    bool concat(const char *cstr) {
        if (cstr == nullptr) {
            return true;
        }
        return concat(cstr, (unsigned int)strlen(cstr));
    }

    bool concat(const char *cstr, unsigned int length) {
        if (cstr == nullptr || length == 0) {
            return true;
        }
        if (!ensureCap(len + length)) {
            return false;
        }
        memcpy(buf + len, cstr, length);
        len += length;
        buf[len] = '\0';
        return true;
    }

    bool concat(const String &other) {
        return concat(other.buf, (unsigned int)other.len);
    }

    bool concat(char character) {
        char tmp[2] = {character, '\0'};
        return concat(tmp, 1);
    }

    String &operator+=(const char *cstr) {
        concat(cstr);
        return *this;
    }

    String &operator+=(const String &other) {
        concat(other);
        return *this;
    }

    String &operator+=(char character) {
        concat(character);
        return *this;
    }

    String &operator+=(int value) {
        concat(String(value));
        return *this;
    }

    String &operator+=(unsigned long value) {
        concat(String(value));
        return *this;
    }

    friend String operator+(const String &a, const String &b) {
        String out = a;
        out.concat(b);
        return out;
    }

    friend String operator+(const String &a, const char *b) {
        String out = a;
        out.concat(b);
        return out;
    }

    friend String operator+(const char *a, const String &b) {
        String out(a);
        out.concat(b);
        return out;
    }

    bool equals(const char *cstr) const {
        if (cstr == nullptr) {
            return len == 0;
        }
        return strcmp(c_str(), cstr) == 0;
    }

    bool equals(const String &other) const {
        return equals(other.c_str());
    }

    bool equalsIgnoreCase(const char *cstr) const {
        if (cstr == nullptr) {
            return len == 0;
        }
        const char *a = c_str();
        while (*a != '\0' && *cstr != '\0') {
            if (tolower((unsigned char)*a) != tolower((unsigned char)*cstr)) {
                return false;
            }
            a++;
            cstr++;
        }
        return *a == '\0' && *cstr == '\0';
    }

    bool operator==(const char *cstr) const {
        return equals(cstr);
    }

    bool operator==(const String &other) const {
        return equals(other);
    }

    bool operator!=(const char *cstr) const {
        return !equals(cstr);
    }

    bool startsWith(const char *prefix) const {
        if (prefix == nullptr) {
            return true;
        }
        const size_t prefixLen = strlen(prefix);
        if (prefixLen > len) {
            return false;
        }
        return memcmp(c_str(), prefix, prefixLen) == 0;
    }

    bool endsWith(const char *suffix) const {
        if (suffix == nullptr) {
            return true;
        }
        const size_t suffixLen = strlen(suffix);
        if (suffixLen > len) {
            return false;
        }
        return memcmp(c_str() + (len - suffixLen), suffix, suffixLen) == 0;
    }

    int indexOf(char character) const {
        const char *found = strchr(c_str(), character);
        if (found == nullptr) {
            return -1;
        }
        return (int)(found - c_str());
    }

    int indexOf(const char *substr) const {
        if (substr == nullptr) {
            return -1;
        }
        const char *found = strstr(c_str(), substr);
        if (found == nullptr) {
            return -1;
        }
        return (int)(found - c_str());
    }

    String substring(unsigned int beginIndex) const {
        if (beginIndex >= len) {
            return String();
        }
        return String(c_str() + beginIndex);
    }

    String substring(unsigned int beginIndex, unsigned int endIndex) const {
        if (beginIndex >= len || endIndex <= beginIndex) {
            return String();
        }
        if (endIndex > len) {
            endIndex = (unsigned int)len;
        }
        String out;
        out.concat(c_str() + beginIndex, endIndex - beginIndex);
        return out;
    }

    void toLowerCase() {
        for (size_t i = 0; i < len; ++i) {
            buf[i] = (char)tolower((unsigned char)buf[i]);
        }
    }

    void toUpperCase() {
        for (size_t i = 0; i < len; ++i) {
            buf[i] = (char)toupper((unsigned char)buf[i]);
        }
    }

    void trim() {
        if (buf == nullptr || len == 0) {
            return;
        }
        size_t start = 0;
        while (start < len && isspace((unsigned char)buf[start])) {
            start++;
        }
        size_t end = len;
        while (end > start && isspace((unsigned char)buf[end - 1])) {
            end--;
        }
        if (start > 0 && end > start) {
            memmove(buf, buf + start, end - start);
        }
        len = end - start;
        buf[len] = '\0';
    }

    int toInt() const {
        return atoi(c_str());
    }

    float toFloat() const {
        return (float)atof(c_str());
    }

    char *begin() {
        return buf != nullptr ? buf : (char *)"";
    }

    char *end() {
        return begin() + len;
    }

private:
    char *buf;
    size_t len;
    size_t cap;

    bool ensureCap(size_t need) {
        const size_t want = need + 1;
        if (cap >= want) {
            if (buf == nullptr) {
                buf = static_cast<char *>(malloc(want));
                if (buf == nullptr) {
                    return false;
                }
                buf[0] = '\0';
                cap = want;
            }
            return true;
        }
        size_t newCap = cap > 0 ? cap : 16;
        while (newCap < want) {
            newCap *= 2;
        }
        char *grown = static_cast<char *>(realloc(buf, newCap));
        if (grown == nullptr) {
            return false;
        }
        buf = grown;
        cap = newCap;
        return true;
    }

    void copyFrom(const char *cstr, size_t length) {
        if (!ensureCap(length)) {
            clear();
            return;
        }
        if (length > 0 && cstr != nullptr) {
            memcpy(buf, cstr, length);
        }
        len = length;
        buf[len] = '\0';
    }
};

class Print {
public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t character) = 0;
    virtual size_t write(const uint8_t *buffer, size_t size) {
        size_t written = 0;
        if (buffer == nullptr) {
            return 0;
        }
        while (written < size) {
            if (write(buffer[written]) == 0) {
                break;
            }
            written++;
        }
        return written;
    }
    size_t print(const char *cstr) {
        if (cstr == nullptr) {
            return 0;
        }
        return write(reinterpret_cast<const uint8_t *>(cstr), strlen(cstr));
    }
    size_t print(const String &text) {
        return print(text.c_str());
    }
    size_t print(char character) {
        return write((uint8_t)character);
    }
    size_t print(int value) {
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "%d", value);
        return print(tmp);
    }
    size_t print(unsigned int value) {
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "%u", value);
        return print(tmp);
    }
    size_t print(unsigned short value) {
        return print((unsigned int)value);
    }
    size_t print(long value) {
        char tmp[24];
        snprintf(tmp, sizeof(tmp), "%ld", value);
        return print(tmp);
    }
    size_t print(unsigned long value) {
        char tmp[24];
        snprintf(tmp, sizeof(tmp), "%lu", value);
        return print(tmp);
    }
    size_t println(const char *cstr) {
        size_t n = print(cstr);
        n += write((uint8_t)'\n');
        return n;
    }
    size_t println(const String &text) {
        return println(text.c_str());
    }
    size_t printf(const char *fmt, ...) {
        if (fmt == nullptr) {
            return 0;
        }
        char buffer[256];
        va_list args;
        va_start(args, fmt);
        const int n = vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);
        if (n <= 0) {
            return 0;
        }
        return print(buffer);
    }
};

class Stream : public Print {
public:
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;
    virtual void flush() {}
};

class EspClass {
public:
    size_t getFreeHeap() {
        return heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    size_t getMaxAllocHeap() {
        return heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    size_t getFreePsram() {
        return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    }
    uint32_t getChipId() {
        return 0;
    }
    void restart() {
        esp_restart();
    }
};

inline EspClass ESP;

class HardwareSerial : public Stream {
public:
    void begin(unsigned long) {}
    void setTxTimeoutMs(uint32_t) {}
    int available() override {
        return 0;
    }
    int read() override {
        return -1;
    }
    int peek() override {
        return -1;
    }
    size_t write(uint8_t character) override {
        putchar(static_cast<char>(character));
        return 1;
    }
};

inline HardwareSerial Serial;
inline HardwareSerial Serial0;
