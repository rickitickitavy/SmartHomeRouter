#include "ConsoleStore.h"
#include "AutomationStore.h"
#include "UserStore.h"
#include "JsonField.h"
#include "Logger.h"

#include <LittleFS.h>
#include <esp_random.h>
#include <string.h>
#include <stdio.h>

ConsoleStore::ConsoleStore()
    : head(nullptr), storedCount(0), persistEnabled(false), persistPending(false) {
}

ConsoleStore::~ConsoleStore() {
    clearAll();
}

bool ConsoleStore::heapAllowsAllocation(size_t bytes) {
    if (bytes == 0) {
        return true;
    }
    const size_t freeHeap = ESP.getFreeHeap();
    const size_t maxBlock = ESP.getMaxAllocHeap();
    if (maxBlock < bytes) {
        return false;
    }
    if (freeHeap < bytes) {
        return false;
    }
    if (freeHeap - bytes < CONSOLE_HEAP_RESERVE_BYTES) {
        return false;
    }
    return true;
}

bool ConsoleStore::assignJsonSlice(const char *start, const char *endInclusive, String &out) {
    out = "";
    if (start == nullptr || endInclusive == nullptr || endInclusive < start) {
        return false;
    }
    const size_t length = (size_t)(endInclusive - start + 1);
    if (!heapAllowsAllocation(length + 1)) {
        return false;
    }
    if (!out.concat(start, (unsigned int)length)) {
        out = "";
        return false;
    }
    return true;
}

char *ConsoleStore::duplicateBoundedString(const char *source, size_t maxLength) {
    const char *text = source != nullptr ? source : "";
    size_t length = strlen(text);
    if (length > maxLength) {
        length = maxLength;
    }
    if (!heapAllowsAllocation(length + 1)) {
        return nullptr;
    }
    char *copy = (char *)malloc(length + 1);
    if (copy == nullptr) {
        return nullptr;
    }
    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

void ConsoleStore::freeGaugeRanges(ConsoleGaugeRange *&listHead) {
    while (listHead != nullptr) {
        ConsoleGaugeRange *next = listHead->next;
        free(listHead->color);
        free(listHead);
        listHead = next;
    }
    listHead = nullptr;
}

void ConsoleStore::freeBindingList(ConsoleBinding *&listHead) {
    while (listHead != nullptr) {
        ConsoleBinding *next = listHead->next;
        free(listHead->ieeeText);
        free(listHead->automationName);
        free(listHead->title);
        free(listHead->unit);
        free(listHead->param);
        free(listHead->icon);
        freeGaugeRanges(listHead->gaugeRanges);
        free(listHead);
        listHead = next;
    }
    listHead = nullptr;
}

const char *ConsoleStore::findJsonObjectEnd(const char *objectStart) {
    if (objectStart == nullptr || *objectStart != '{') {
        return nullptr;
    }
    int depth = 0;
    bool inString = false;
    bool escape = false;
    for (const char *scan = objectStart; *scan != '\0'; scan++) {
        const char character = *scan;
        if (inString) {
            if (escape) {
                escape = false;
                continue;
            }
            if (character == '\\') {
                escape = true;
                continue;
            }
            if (character == '"') {
                inString = false;
            }
            continue;
        }
        if (character == '"') {
            inString = true;
            continue;
        }
        if (character == '{') {
            depth++;
            continue;
        }
        if (character == '}') {
            depth--;
            if (depth == 0) {
                return scan;
            }
        }
    }
    return nullptr;
}

uint8_t ConsoleStore::parseViewportId(const char *text) {
    if (text != nullptr && strcmp(text, "grid") == 0) {
        return CONSOLE_VIEWPORT_GRID;
    }
    if (text != nullptr && strcmp(text, "composite") == 0) {
        return CONSOLE_VIEWPORT_COMPOSITE;
    }
    // Legacy "horizontal" and unknown values load as vertical.
    return CONSOLE_VIEWPORT_VERTICAL;
}

const char *ConsoleStore::viewportJsonId(uint8_t viewport) {
    if (viewport == CONSOLE_VIEWPORT_GRID) {
        return "grid";
    }
    if (viewport == CONSOLE_VIEWPORT_COMPOSITE) {
        return "composite";
    }
    return "vertical";
}

uint8_t ConsoleStore::parseSizeUnitId(const char *text) {
    if (text != nullptr && strcmp(text, "cm") == 0) {
        return CONSOLE_SIZE_UNIT_CM;
    }
    return CONSOLE_SIZE_UNIT_U;
}

const char *ConsoleStore::sizeUnitJsonId(uint8_t sizeUnit) {
    if (sizeUnit == CONSOLE_SIZE_UNIT_CM) {
        return "cm";
    }
    return "u";
}

uint8_t ConsoleStore::parseCellContentId(const char *text) {
    if (text != nullptr && strcmp(text, "text") == 0) {
        return CONSOLE_CELL_CONTENT_TEXT;
    }
    if (text != nullptr && strcmp(text, "device") == 0) {
        return CONSOLE_CELL_CONTENT_DEVICE;
    }
    return CONSOLE_CELL_CONTENT_EMPTY;
}

const char *ConsoleStore::cellContentJsonId(uint8_t content) {
    if (content == CONSOLE_CELL_CONTENT_TEXT) {
        return "text";
    }
    if (content == CONSOLE_CELL_CONTENT_DEVICE) {
        return "device";
    }
    return "empty";
}

uint8_t ConsoleStore::parseViewStyleId(const char *text) {
    if (text != nullptr && strcmp(text, "gauge") == 0) {
        return CONSOLE_VIEW_STYLE_GAUGE;
    }
    if (text != nullptr && strcmp(text, "icon") == 0) {
        return CONSOLE_VIEW_STYLE_ICON;
    }
    return CONSOLE_VIEW_STYLE_DEFAULT;
}

const char *ConsoleStore::viewStyleJsonId(uint8_t viewStyle) {
    if (viewStyle == CONSOLE_VIEW_STYLE_GAUGE) {
        return "gauge";
    }
    if (viewStyle == CONSOLE_VIEW_STYLE_ICON) {
        return "icon";
    }
    return "default";
}

uint8_t ConsoleStore::parseTitlePositionId(const char *text) {
    if (text != nullptr && strcmp(text, "right") == 0) {
        return CONSOLE_TITLE_POS_RIGHT;
    }
    if (text != nullptr && strcmp(text, "top") == 0) {
        return CONSOLE_TITLE_POS_TOP;
    }
    if (text != nullptr && strcmp(text, "bottom") == 0) {
        return CONSOLE_TITLE_POS_BOTTOM;
    }
    return CONSOLE_TITLE_POS_LEFT;
}

const char *ConsoleStore::titlePositionJsonId(uint8_t titlePosition) {
    if (titlePosition == CONSOLE_TITLE_POS_RIGHT) {
        return "right";
    }
    if (titlePosition == CONSOLE_TITLE_POS_TOP) {
        return "top";
    }
    if (titlePosition == CONSOLE_TITLE_POS_BOTTOM) {
        return "bottom";
    }
    return "left";
}

uint8_t ConsoleStore::parseIconAlignId(const char *text) {
    if (text != nullptr && strcmp(text, "left") == 0) {
        return CONSOLE_ICON_ALIGN_LEFT;
    }
    if (text != nullptr && strcmp(text, "right") == 0) {
        return CONSOLE_ICON_ALIGN_RIGHT;
    }
    return CONSOLE_ICON_ALIGN_CENTER;
}

const char *ConsoleStore::iconAlignJsonId(uint8_t iconAlign) {
    if (iconAlign == CONSOLE_ICON_ALIGN_LEFT) {
        return "left";
    }
    if (iconAlign == CONSOLE_ICON_ALIGN_RIGHT) {
        return "right";
    }
    return "center";
}

uint8_t ConsoleStore::parseAlignVId(const char *text) {
    if (text != nullptr && strcmp(text, "top") == 0) {
        return CONSOLE_ALIGN_V_TOP;
    }
    if (text != nullptr && strcmp(text, "bottom") == 0) {
        return CONSOLE_ALIGN_V_BOTTOM;
    }
    return CONSOLE_ALIGN_V_CENTER;
}

const char *ConsoleStore::alignVJsonId(uint8_t alignV) {
    if (alignV == CONSOLE_ALIGN_V_TOP) {
        return "top";
    }
    if (alignV == CONSOLE_ALIGN_V_BOTTOM) {
        return "bottom";
    }
    return "center";
}

void ConsoleStore::appendBindingLayoutJson(String &json, const ConsoleBinding *binding) {
    if (binding == nullptr) {
        return;
    }
    if (!binding->showTitle) {
        json += ",\"showTitle\":false";
    }
    if (binding->titlePosition != CONSOLE_TITLE_POS_LEFT) {
        json += ",\"titlePosition\":\"";
        json += titlePositionJsonId(binding->titlePosition);
        json += "\"";
    }
    if (binding->alignH != CONSOLE_ICON_ALIGN_CENTER) {
        json += ",\"alignH\":\"";
        json += iconAlignJsonId(binding->alignH);
        json += "\"";
    }
    if (binding->alignV != CONSOLE_ALIGN_V_CENTER) {
        json += ",\"alignV\":\"";
        json += alignVJsonId(binding->alignV);
        json += "\"";
    }
    if (binding->viewStyle == CONSOLE_VIEW_STYLE_ICON) {
        if (binding->iconScale > 0.0f && binding->iconScale != 1.0f) {
            json += ",\"iconScale\":";
            json += String(binding->iconScale, 4);
        }
        if (binding->alignH != CONSOLE_ICON_ALIGN_CENTER) {
            json += ",\"iconAlign\":\"";
            json += iconAlignJsonId(binding->alignH);
            json += "\"";
        }
    }
}

uint8_t ConsoleStore::parseGaugeTypeId(const char *text) {
    if (text != nullptr && strcmp(text, "linear") == 0) {
        return CONSOLE_GAUGE_TYPE_LINEAR;
    }
    return CONSOLE_GAUGE_TYPE_ARC;
}

const char *ConsoleStore::gaugeTypeJsonId(uint8_t gaugeType) {
    if (gaugeType == CONSOLE_GAUGE_TYPE_LINEAR) {
        return "linear";
    }
    return "arc";
}

void ConsoleStore::freeCompositeCellList(ConsoleCompositeCell *&listHead) {
    while (listHead != nullptr) {
        ConsoleCompositeCell *next = listHead->next;
        free(listHead->text);
        if (listHead->binding != nullptr) {
            ConsoleBinding *binding = listHead->binding;
            listHead->binding = nullptr;
            freeBindingList(binding);
        }
        free(listHead);
        listHead = next;
    }
    listHead = nullptr;
}

void ConsoleStore::freeCompositeRowList(ConsoleCompositeRow *&listHead) {
    while (listHead != nullptr) {
        ConsoleCompositeRow *next = listHead->next;
        freeCompositeCellList(listHead->cells);
        free(listHead);
        listHead = next;
    }
    listHead = nullptr;
}

void ConsoleStore::freeWidgetList(ConsoleWidget *&listHead) {
    while (listHead != nullptr) {
        ConsoleWidget *next = listHead->next;
        free(listHead->title);
        freeBindingList(listHead->bindings);
        freeCompositeRowList(listHead->compositeRows);
        free(listHead);
        listHead = next;
    }
    listHead = nullptr;
}

void ConsoleStore::freeConsole(ConsoleRecord *console) {
    if (console == nullptr) {
        return;
    }
    free(console->name);
    freeWidgetList(console->widgets);
    free(console);
}

void ConsoleStore::freeConsoleList(ConsoleRecord *&listHead) {
    while (listHead != nullptr) {
        ConsoleRecord *next = listHead->next;
        freeConsole(listHead);
        listHead = next;
    }
    listHead = nullptr;
}

void ConsoleStore::clearAll() {
    freeConsoleList(head);
    storedCount = 0;
}

void ConsoleStore::makeConsoleId(char *outId) {
    if (outId == nullptr) {
        return;
    }
    uint32_t randomValue = esp_random();
    snprintf(outId, CONSOLE_ID_BUF, "%08x", (unsigned int)randomValue);
}

int ConsoleStore::consoleCount() const {
    return storedCount;
}

ConsoleRecord *ConsoleStore::first() {
    return head;
}

const ConsoleRecord *ConsoleStore::first() const {
    return head;
}

ConsoleRecord *ConsoleStore::findById(const char *consoleId) {
    return const_cast<ConsoleRecord *>(static_cast<const ConsoleStore *>(this)->findById(consoleId));
}

const ConsoleRecord *ConsoleStore::findById(const char *consoleId) const {
    if (consoleId == nullptr || consoleId[0] == '\0') {
        return nullptr;
    }
    for (const ConsoleRecord *console = head; console != nullptr; console = console->next) {
        if (strcmp(console->id, consoleId) == 0) {
            return console;
        }
    }
    return nullptr;
}

bool ConsoleStore::begin(bool persistToLittleFs) {
    persistEnabled = persistToLittleFs;
    if (!persistEnabled) {
        return true;
    }
    if (loadFromFile()) {
        LOGGER.info("Loaded " + String(storedCount) + " control console(s)");
        return true;
    }
    clearAll();
    return true;
}

void ConsoleStore::requestPersist() {
    persistPending = true;
}

void ConsoleStore::persistIfDue() {
    if (!persistPending) {
        return;
    }
    if (!persistEnabled) {
        persistPending = false;
        return;
    }
    persistPending = false;
    if (!saveToFile()) {
        LOGGER.error("Console store write failed");
        persistPending = true;
        return;
    }
    LOGGER.info("Console store saved " + String(storedCount) + " console(s)");
}

bool ConsoleStore::saveToFile() const {
    File file = LittleFS.open(CONSOLES_STORE_PATH, "w");
    if (!file) {
        LOGGER.error("Consoles file write failed");
        return false;
    }
    const String json = listFullJson();
    const size_t written = file.print(json);
    file.close();
    return written == json.length();
}

bool ConsoleStore::persistNow() {
    if (!persistEnabled) {
        return true;
    }
    if (!saveToFile()) {
        LOGGER.error("Console store write failed");
        return false;
    }
    return true;
}

bool ConsoleStore::parseGaugeConfig(const char *bindingJson, ConsoleBinding *binding) {
    if (bindingJson == nullptr || binding == nullptr) {
        return false;
    }
    binding->hasGauge = false;
    binding->gaugeType = CONSOLE_GAUGE_TYPE_ARC;
    binding->gaugeMin = 0.0f;
    binding->gaugeMax = 100.0f;
    binding->gaugeRanges = nullptr;
    String gaugeJson;
    if (!extractJsonKeyedSlice(bindingJson, "gauge", '{', gaugeJson)) {
        return true;
    }
    String typeText;
    double minValue = 0.0;
    double maxValue = 100.0;
    extractJsonString(gaugeJson.c_str(), "type", typeText);
    extractJsonDouble(gaugeJson.c_str(), "min", minValue);
    extractJsonDouble(gaugeJson.c_str(), "max", maxValue);
    binding->gaugeType = parseGaugeTypeId(typeText.c_str());
    binding->gaugeMin = (float)minValue;
    binding->gaugeMax = (float)maxValue;
    if (binding->gaugeMax < binding->gaugeMin) {
        const float swap = binding->gaugeMin;
        binding->gaugeMin = binding->gaugeMax;
        binding->gaugeMax = swap;
    }
    String rangesJson;
    if (extractJsonKeyedSlice(gaugeJson.c_str(), "ranges", '[', rangesJson)) {
        ConsoleGaugeRange *tail = nullptr;
        const char *cursor = rangesJson.c_str();
        int rangeCount = 0;
        while (cursor != nullptr && *cursor != '\0' && rangeCount < CONSOLE_GAUGE_RANGES_MAX) {
            const char *objectStart = strchr(cursor, '{');
            if (objectStart == nullptr) {
                break;
            }
            const char *objectEnd = findJsonObjectEnd(objectStart);
            if (objectEnd == nullptr) {
                freeGaugeRanges(binding->gaugeRanges);
                binding->gaugeRanges = nullptr;
                return false;
            }
            String object;
            if (!assignJsonSlice(objectStart, objectEnd, object)) {
                freeGaugeRanges(binding->gaugeRanges);
                binding->gaugeRanges = nullptr;
                return false;
            }
            double topValue = 0.0;
            String colorText;
            extractJsonDouble(object.c_str(), "top", topValue);
            extractJsonString(object.c_str(), "color", colorText);
            if (!heapAllowsAllocation(sizeof(ConsoleGaugeRange))) {
                freeGaugeRanges(binding->gaugeRanges);
                binding->gaugeRanges = nullptr;
                return false;
            }
            ConsoleGaugeRange *range = (ConsoleGaugeRange *)malloc(sizeof(ConsoleGaugeRange));
            if (range == nullptr) {
                freeGaugeRanges(binding->gaugeRanges);
                binding->gaugeRanges = nullptr;
                return false;
            }
            memset(range, 0, sizeof(*range));
            range->top = (float)topValue;
            range->color = duplicateBoundedString(colorText.c_str(), CONSOLE_GAUGE_COLOR_MAX - 1);
            if (range->color == nullptr) {
                free(range);
                freeGaugeRanges(binding->gaugeRanges);
                binding->gaugeRanges = nullptr;
                return false;
            }
            range->next = nullptr;
            if (tail == nullptr) {
                binding->gaugeRanges = range;
            } else {
                tail->next = range;
            }
            tail = range;
            rangeCount++;
            cursor = objectEnd + 1;
        }
    }
    binding->hasGauge = true;
    return true;
}

bool ConsoleStore::parseBindings(const char *widgetJson, ConsoleBinding **outBindings) {
    if (outBindings == nullptr) {
        return false;
    }
    *outBindings = nullptr;
    String bindingsJson;
    if (!extractJsonKeyedSlice(widgetJson, "devices", '[', bindingsJson)
        && !extractJsonKeyedSlice(widgetJson, "bindings", '[', bindingsJson)) {
        return true;
    }
    ConsoleBinding *tail = nullptr;
    const char *cursor = bindingsJson.c_str();
    while (cursor != nullptr && *cursor != '\0') {
        const char *objectStart = strchr(cursor, '{');
        if (objectStart == nullptr) {
            break;
        }
        const char *objectEnd = findJsonObjectEnd(objectStart);
        if (objectEnd == nullptr) {
            freeBindingList(*outBindings);
            return false;
        }
        String object;
        if (!assignJsonSlice(objectStart, objectEnd, object)) {
            freeBindingList(*outBindings);
            return false;
        }
        if (!heapAllowsAllocation(sizeof(ConsoleBinding))) {
            freeBindingList(*outBindings);
            return false;
        }
        ConsoleBinding *binding = (ConsoleBinding *)malloc(sizeof(ConsoleBinding));
        if (binding == nullptr) {
            freeBindingList(*outBindings);
            return false;
        }
        memset(binding, 0, sizeof(*binding));
        binding->attrId = -1;
        binding->viewStyle = CONSOLE_VIEW_STYLE_DEFAULT;
        binding->iconScale = 1.0f;
        binding->iconAlign = CONSOLE_ICON_ALIGN_CENTER;
        binding->fontScale = 1.0f;
        binding->alignH = CONSOLE_ICON_ALIGN_CENTER;
        binding->alignV = CONSOLE_ALIGN_V_CENTER;
        binding->showTitle = true;
        binding->titlePosition = CONSOLE_TITLE_POS_LEFT;
        bool isSpacer = false;
        extractJsonBool(object.c_str(), "spacer", isSpacer);
        if (isSpacer) {
            binding->isSpacer = true;
            String spacerText;
            String alignHText;
            String alignVText;
            double fontScaleValue = 1.0;
            extractJsonString(object.c_str(), "text", spacerText);
            extractJsonString(object.c_str(), "alignH", alignHText);
            extractJsonString(object.c_str(), "alignV", alignVText);
            if (!extractJsonDouble(object.c_str(), "fontScale", fontScaleValue)
                || fontScaleValue <= 0.0
                || fontScaleValue != fontScaleValue) {
                fontScaleValue = 1.0;
            }
            binding->ieeeText = duplicateBoundedString("", IEEE_TEXT_MAX - 1);
            binding->title = duplicateBoundedString(spacerText.c_str(), CONSOLE_BINDING_TITLE_MAX - 1);
            binding->unit = duplicateBoundedString("", CONSOLE_UNIT_MAX - 1);
            binding->channel = 1;
            binding->decimals = -1;
            binding->attrId = -1;
            binding->iconScale = 1.0f;
            binding->iconAlign = CONSOLE_ICON_ALIGN_CENTER;
            binding->fontScale = (float)fontScaleValue;
            binding->alignH = parseIconAlignId(alignHText.c_str());
            binding->alignV = parseAlignVId(alignVText.c_str());
            binding->showTitle = true;
            binding->titlePosition = CONSOLE_TITLE_POS_LEFT;
            if (binding->ieeeText == nullptr || binding->title == nullptr || binding->unit == nullptr) {
                free(binding->ieeeText);
                free(binding->title);
                free(binding->unit);
                free(binding);
                freeBindingList(*outBindings);
                return false;
            }
            binding->next = nullptr;
            if (tail == nullptr) {
                *outBindings = binding;
            } else {
                tail->next = binding;
            }
            tail = binding;
            cursor = objectEnd + 1;
            continue;
        }
        String ieeeText;
        String automationName;
        String title;
        String unit;
        String paramText;
        String viewStyleText;
        String iconText;
        String titlePositionText;
        String iconAlignText;
        String alignHText;
        String alignVText;
        int channel = 1;
        int decimals = -1;
        int clusterValue = 0;
        int attrValue = -1;
        double iconScaleValue = 1.0;
        bool showTitleValue = true;
        extractJsonString(object.c_str(), "automation", automationName);
        if (automationName.length() == 0) {
            extractJsonString(object.c_str(), "automationName", automationName);
        }
        extractJsonString(object.c_str(), "ieee", ieeeText);
        if (ieeeText.length() == 0) {
            extractJsonString(object.c_str(), "deviceIeee", ieeeText);
        }
        extractJsonString(object.c_str(), "title", title);
        extractJsonString(object.c_str(), "unit", unit);
        extractJsonString(object.c_str(), "param", paramText);
        extractJsonString(object.c_str(), "viewStyle", viewStyleText);
        extractJsonString(object.c_str(), "icon", iconText);
        extractJsonString(object.c_str(), "titlePosition", titlePositionText);
        extractJsonString(object.c_str(), "iconAlign", iconAlignText);
        extractJsonString(object.c_str(), "alignH", alignHText);
        extractJsonString(object.c_str(), "alignV", alignVText);
        extractJsonInt(object.c_str(), "channel", channel);
        extractJsonInt(object.c_str(), "decimals", decimals);
        extractJsonInt(object.c_str(), "cluster", clusterValue);
        if (!extractJsonInt(object.c_str(), "attrId", attrValue)) {
            attrValue = -1;
        }
        if (!extractJsonDouble(object.c_str(), "iconScale", iconScaleValue)
            || iconScaleValue <= 0.0
            || iconScaleValue != iconScaleValue) {
            iconScaleValue = 1.0;
        }
        if (!extractJsonBool(object.c_str(), "showTitle", showTitleValue)) {
            showTitleValue = true;
        }
        if (channel < 1) {
            channel = 1;
        }
        if (channel > DEVICE_CHANNEL_COUNT_MAX) {
            channel = DEVICE_CHANNEL_COUNT_MAX;
        }
        if (decimals < -1) {
            decimals = -1;
        }
        if (decimals > 3) {
            decimals = 3;
        }
        if (clusterValue < 0) {
            clusterValue = 0;
        }
        if (clusterValue > 0xFFFF) {
            clusterValue = 0xFFFF;
        }
        const bool isAutomation = automationName.length() > 0;
        if (isAutomation) {
            if (!AutomationStore::isNameSafe(automationName.c_str())) {
                free(binding);
                freeBindingList(*outBindings);
                return false;
            }
            ieeeText = "";
            unit = "";
            paramText = "";
            decimals = -1;
            channel = 1;
            clusterValue = 0;
            attrValue = -1;
        } else if (ieeeText.length() == 0) {
            free(binding);
            freeBindingList(*outBindings);
            return false;
        }
        binding->ieeeText = duplicateBoundedString(ieeeText.c_str(), IEEE_TEXT_MAX - 1);
        binding->automationName = isAutomation
            ? duplicateBoundedString(automationName.c_str(), AUTOMATION_NAME_MAX - 1)
            : nullptr;
        binding->title = duplicateBoundedString(title.c_str(), CONSOLE_BINDING_TITLE_MAX - 1);
        binding->unit = duplicateBoundedString(unit.c_str(), CONSOLE_UNIT_MAX - 1);
        binding->param = (!isAutomation && paramText.length() > 0)
            ? duplicateBoundedString(paramText.c_str(), CONSOLE_PARAM_MAX - 1)
            : nullptr;
        binding->icon = (iconText.length() > 0)
            ? duplicateBoundedString(iconText.c_str(), CONSOLE_ICON_MAX - 1)
            : nullptr;
        if (binding->ieeeText == nullptr || binding->title == nullptr || binding->unit == nullptr
            || (isAutomation && binding->automationName == nullptr)
            || (!isAutomation && paramText.length() > 0 && binding->param == nullptr)
            || (iconText.length() > 0 && binding->icon == nullptr)) {
            free(binding->ieeeText);
            free(binding->automationName);
            free(binding->title);
            free(binding->unit);
            free(binding->param);
            free(binding->icon);
            free(binding);
            freeBindingList(*outBindings);
            return false;
        }
        binding->channel = (uint8_t)channel;
        binding->decimals = (int8_t)decimals;
        binding->cluster = isAutomation ? 0 : (uint16_t)clusterValue;
        binding->attrId = isAutomation ? -1 : (int32_t)attrValue;
        binding->viewStyle = parseViewStyleId(viewStyleText.c_str());
        if (isAutomation && binding->viewStyle == CONSOLE_VIEW_STYLE_GAUGE) {
            binding->viewStyle = CONSOLE_VIEW_STYLE_DEFAULT;
        }
        binding->showTitle = showTitleValue;
        binding->titlePosition = parseTitlePositionId(titlePositionText.c_str());
        binding->iconScale = (float)iconScaleValue;
        if (alignHText.length() > 0) {
            binding->alignH = parseIconAlignId(alignHText.c_str());
        } else {
            binding->alignH = parseIconAlignId(iconAlignText.c_str());
        }
        binding->alignV = parseAlignVId(alignVText.c_str());
        binding->iconAlign = binding->alignH;
        if (!isAutomation) {
            if (!parseGaugeConfig(object.c_str(), binding)) {
                free(binding->ieeeText);
                free(binding->automationName);
                free(binding->title);
                free(binding->unit);
                free(binding->param);
                free(binding->icon);
                free(binding);
                freeBindingList(*outBindings);
                return false;
            }
            if (binding->viewStyle == CONSOLE_VIEW_STYLE_GAUGE) {
                binding->hasGauge = true;
            } else {
                freeGaugeRanges(binding->gaugeRanges);
                binding->hasGauge = false;
            }
        } else {
            freeGaugeRanges(binding->gaugeRanges);
            binding->hasGauge = false;
        }
        if (binding->viewStyle != CONSOLE_VIEW_STYLE_ICON) {
            free(binding->icon);
            binding->icon = nullptr;
            binding->iconScale = 1.0f;
        }
        binding->next = nullptr;
        if (tail == nullptr) {
            *outBindings = binding;
        } else {
            tail->next = binding;
        }
        tail = binding;
        cursor = objectEnd + 1;
    }
    return true;
}

bool ConsoleStore::parseOneBindingObject(const char *bindingObjectJson, ConsoleBinding **outBinding) {
    if (outBinding == nullptr || bindingObjectJson == nullptr) {
        return false;
    }
    *outBinding = nullptr;
    String wrapper = "{\"devices\":[";
    wrapper += bindingObjectJson;
    wrapper += "]}";
    ConsoleBinding *list = nullptr;
    if (!parseBindings(wrapper.c_str(), &list)) {
        return false;
    }
    if (list == nullptr) {
        return false;
    }
    *outBinding = list;
    if (list->next != nullptr) {
        freeBindingList(list->next);
        list->next = nullptr;
    }
    return true;
}

bool ConsoleStore::parseCompositeRows(const char *widgetJson, ConsoleCompositeRow **outRows) {
    if (outRows == nullptr) {
        return false;
    }
    *outRows = nullptr;
    String rowsJson;
    if (!extractJsonKeyedSlice(widgetJson, "compositeRows", '[', rowsJson)) {
        return true;
    }
    ConsoleCompositeRow *rowTail = nullptr;
    int rowCount = 0;
    const char *cursor = rowsJson.c_str();
    while (cursor != nullptr && *cursor != '\0') {
        const char *objectStart = strchr(cursor, '{');
        if (objectStart == nullptr) {
            break;
        }
        const char *objectEnd = findJsonObjectEnd(objectStart);
        if (objectEnd == nullptr) {
            freeCompositeRowList(*outRows);
            return false;
        }
        if (rowCount >= CONSOLE_COMPOSITE_ROWS_MAX) {
            freeCompositeRowList(*outRows);
            return false;
        }
        String object;
        if (!assignJsonSlice(objectStart, objectEnd, object)) {
            freeCompositeRowList(*outRows);
            return false;
        }
        if (!heapAllowsAllocation(sizeof(ConsoleCompositeRow))) {
            freeCompositeRowList(*outRows);
            return false;
        }
        ConsoleCompositeRow *row = (ConsoleCompositeRow *)malloc(sizeof(ConsoleCompositeRow));
        if (row == nullptr) {
            freeCompositeRowList(*outRows);
            return false;
        }
        memset(row, 0, sizeof(*row));
        row->height = 1.0f;
        row->heightUnit = CONSOLE_SIZE_UNIT_U;
        double heightValue = 1.0;
        String heightUnitText;
        extractJsonDouble(object.c_str(), "height", heightValue);
        extractJsonString(object.c_str(), "heightUnit", heightUnitText);
        if (heightValue <= 0.0 || heightValue != heightValue) {
            heightValue = 1.0;
        }
        row->height = (float)heightValue;
        row->heightUnit = parseSizeUnitId(heightUnitText.c_str());
        String cellsJson;
        ConsoleCompositeCell *cellTail = nullptr;
        int cellCount = 0;
        if (extractJsonKeyedSlice(object.c_str(), "cells", '[', cellsJson)) {
            const char *cellCursor = cellsJson.c_str();
            while (cellCursor != nullptr && *cellCursor != '\0') {
                const char *cellStart = strchr(cellCursor, '{');
                if (cellStart == nullptr) {
                    break;
                }
                const char *cellEnd = findJsonObjectEnd(cellStart);
                if (cellEnd == nullptr) {
                    freeCompositeCellList(row->cells);
                    free(row);
                    freeCompositeRowList(*outRows);
                    return false;
                }
                if (cellCount >= CONSOLE_COMPOSITE_CELLS_MAX) {
                    freeCompositeCellList(row->cells);
                    free(row);
                    freeCompositeRowList(*outRows);
                    return false;
                }
                String cellObject;
                if (!assignJsonSlice(cellStart, cellEnd, cellObject)) {
                    freeCompositeCellList(row->cells);
                    free(row);
                    freeCompositeRowList(*outRows);
                    return false;
                }
                if (!heapAllowsAllocation(sizeof(ConsoleCompositeCell))) {
                    freeCompositeCellList(row->cells);
                    free(row);
                    freeCompositeRowList(*outRows);
                    return false;
                }
                ConsoleCompositeCell *cell = (ConsoleCompositeCell *)malloc(sizeof(ConsoleCompositeCell));
                if (cell == nullptr) {
                    freeCompositeCellList(row->cells);
                    free(row);
                    freeCompositeRowList(*outRows);
                    return false;
                }
                memset(cell, 0, sizeof(*cell));
                cell->width = 1.0f;
                cell->widthUnit = CONSOLE_SIZE_UNIT_U;
                cell->content = CONSOLE_CELL_CONTENT_EMPTY;
                cell->alignH = CONSOLE_ICON_ALIGN_CENTER;
                cell->alignV = CONSOLE_ALIGN_V_CENTER;
                double widthValue = 1.0;
                String widthUnitText;
                String contentText;
                String cellText;
                String alignHText;
                String alignVText;
                bool contour = false;
                extractJsonDouble(cellObject.c_str(), "width", widthValue);
                extractJsonString(cellObject.c_str(), "widthUnit", widthUnitText);
                extractJsonString(cellObject.c_str(), "content", contentText);
                extractJsonString(cellObject.c_str(), "text", cellText);
                extractJsonString(cellObject.c_str(), "alignH", alignHText);
                extractJsonString(cellObject.c_str(), "alignV", alignVText);
                extractJsonBool(cellObject.c_str(), "contour", contour);
                if (widthValue <= 0.0 || widthValue != widthValue) {
                    widthValue = 1.0;
                }
                cell->width = (float)widthValue;
                cell->widthUnit = parseSizeUnitId(widthUnitText.c_str());
                cell->contour = contour;
                cell->content = parseCellContentId(contentText.c_str());
                cell->alignH = parseIconAlignId(alignHText.c_str());
                cell->alignV = parseAlignVId(alignVText.c_str());
                if (cell->content == CONSOLE_CELL_CONTENT_TEXT) {
                    cell->text = duplicateBoundedString(cellText.c_str(), CONSOLE_COMPOSITE_TEXT_MAX - 1);
                    if (cell->text == nullptr) {
                        free(cell);
                        freeCompositeCellList(row->cells);
                        free(row);
                        freeCompositeRowList(*outRows);
                        return false;
                    }
                } else if (cell->content == CONSOLE_CELL_CONTENT_DEVICE) {
                    String bindingJson;
                    if (!extractJsonKeyedSlice(cellObject.c_str(), "binding", '{', bindingJson)) {
                        free(cell);
                        freeCompositeCellList(row->cells);
                        free(row);
                        freeCompositeRowList(*outRows);
                        return false;
                    }
                    if (!parseOneBindingObject(bindingJson.c_str(), &cell->binding) || cell->binding == nullptr) {
                        free(cell);
                        freeCompositeCellList(row->cells);
                        free(row);
                        freeCompositeRowList(*outRows);
                        return false;
                    }
                }
                cell->next = nullptr;
                if (cellTail == nullptr) {
                    row->cells = cell;
                } else {
                    cellTail->next = cell;
                }
                cellTail = cell;
                cellCount++;
                cellCursor = cellEnd + 1;
            }
        }
        row->next = nullptr;
        if (rowTail == nullptr) {
            *outRows = row;
        } else {
            rowTail->next = row;
        }
        rowTail = row;
        rowCount++;
        cursor = objectEnd + 1;
    }
    return true;
}

bool ConsoleStore::parseWidgets(const char *objectJson, ConsoleWidget **outWidgets) {
    if (outWidgets == nullptr) {
        return false;
    }
    *outWidgets = nullptr;
    String widgetsJson;
    if (!extractJsonKeyedSlice(objectJson, "widgets", '[', widgetsJson)) {
        return true;
    }
    ConsoleWidget *tail = nullptr;
    const char *cursor = widgetsJson.c_str();
    while (cursor != nullptr && *cursor != '\0') {
        const char *objectStart = strchr(cursor, '{');
        if (objectStart == nullptr) {
            break;
        }
        const char *objectEnd = findJsonObjectEnd(objectStart);
        if (objectEnd == nullptr) {
            freeWidgetList(*outWidgets);
            return false;
        }
        String object;
        if (!assignJsonSlice(objectStart, objectEnd, object)) {
            freeWidgetList(*outWidgets);
            return false;
        }
        // Root-depth only: nested devices[].spacer must not mark the whole widget a spacer.
        bool isSpacer = false;
        extractJsonBoolRoot(object.c_str(), "spacer", isSpacer);
        if (isSpacer) {
            cursor = objectEnd + 1;
            continue;
        }
        if (!heapAllowsAllocation(sizeof(ConsoleWidget))) {
            freeWidgetList(*outWidgets);
            return false;
        }
        ConsoleWidget *widget = (ConsoleWidget *)malloc(sizeof(ConsoleWidget));
        if (widget == nullptr) {
            freeWidgetList(*outWidgets);
            return false;
        }
        memset(widget, 0, sizeof(*widget));
        widget->viewport = CONSOLE_VIEWPORT_VERTICAL;
        widget->gridColumns = CONSOLE_WIDGET_GRID_DEFAULT;
        widget->gridRows = CONSOLE_WIDGET_GRID_DEFAULT;
        widget->fullWidth = false;
        widget->showTitle = true;
        String title;
        String viewportText;
        int gridColumns = CONSOLE_WIDGET_GRID_DEFAULT;
        int gridRows = CONSOLE_WIDGET_GRID_DEFAULT;
        bool fullWidth = false;
        bool showTitle = true;
        extractJsonStringRoot(object.c_str(), "title", title);
        extractJsonBoolRoot(object.c_str(), "showGroupOnOff", widget->showGroupOnOff);
        extractJsonBoolRoot(object.c_str(), "fullWidth", fullWidth);
        if (!extractJsonBoolRoot(object.c_str(), "showTitle", showTitle)) {
            showTitle = true;
        }
        extractJsonStringRoot(object.c_str(), "viewport", viewportText);
        extractJsonIntRoot(object.c_str(), "gridColumns", gridColumns);
        extractJsonIntRoot(object.c_str(), "gridRows", gridRows);
        if (gridColumns < 1) {
            gridColumns = CONSOLE_WIDGET_GRID_DEFAULT;
        }
        if (gridColumns > CONSOLE_GRID_COLUMNS_MAX) {
            gridColumns = CONSOLE_GRID_COLUMNS_MAX;
        }
        if (gridRows < 1) {
            gridRows = CONSOLE_WIDGET_GRID_DEFAULT;
        }
        if (gridRows > CONSOLE_GRID_COLUMNS_MAX) {
            gridRows = CONSOLE_GRID_COLUMNS_MAX;
        }
        widget->viewport = parseViewportId(viewportText.c_str());
        widget->gridColumns = (uint8_t)gridColumns;
        widget->gridRows = (uint8_t)gridRows;
        widget->fullWidth = fullWidth;
        widget->showTitle = showTitle;
        widget->isSpacer = false;
        widget->title = duplicateBoundedString(title.c_str(), CONSOLE_WIDGET_TITLE_MAX - 1);
        if (widget->title == nullptr) {
            free(widget);
            freeWidgetList(*outWidgets);
            return false;
        }
        if (widget->viewport == CONSOLE_VIEWPORT_COMPOSITE) {
            if (!parseCompositeRows(object.c_str(), &widget->compositeRows)) {
                free(widget->title);
                free(widget);
                freeWidgetList(*outWidgets);
                return false;
            }
            widget->bindings = nullptr;
        } else if (!parseBindings(object.c_str(), &widget->bindings)) {
            free(widget->title);
            free(widget);
            freeWidgetList(*outWidgets);
            return false;
        }
        widget->next = nullptr;
        if (tail == nullptr) {
            *outWidgets = widget;
        } else {
            tail->next = widget;
        }
        tail = widget;
        cursor = objectEnd + 1;
    }
    return true;
}

ConsoleWriteResult ConsoleStore::parseOneConsole(const char *objectJson, ConsoleRecord **outConsole, bool requireId) {
    if (objectJson == nullptr || outConsole == nullptr) {
        return ConsoleWriteParseError;
    }
    *outConsole = nullptr;
    String idText;
    String nameText;
    bool active = true;
    int columns = CONSOLE_GRID_COLUMNS_DEFAULT;
    extractJsonString(objectJson, "id", idText);
    extractJsonString(objectJson, "name", nameText);
    extractJsonBool(objectJson, "active", active);
    extractJsonInt(objectJson, "columns", columns);
    if (columns < 1) {
        columns = CONSOLE_GRID_COLUMNS_DEFAULT;
    }
    if (columns > CONSOLE_GRID_COLUMNS_MAX) {
        columns = CONSOLE_GRID_COLUMNS_MAX;
    }
    if (requireId && (idText.length() == 0 || idText.length() >= CONSOLE_ID_BUF)) {
        return ConsoleWriteBadId;
    }
    if (nameText.length() == 0 || nameText.length() >= CONSOLE_NAME_MAX) {
        return ConsoleWriteBadName;
    }
    if (!heapAllowsAllocation(sizeof(ConsoleRecord))) {
        return ConsoleWriteOutOfMemory;
    }
    ConsoleRecord *console = (ConsoleRecord *)malloc(sizeof(ConsoleRecord));
    if (console == nullptr) {
        return ConsoleWriteOutOfMemory;
    }
    memset(console, 0, sizeof(*console));
    if (idText.length() > 0) {
        strncpy(console->id, idText.c_str(), sizeof(console->id) - 1);
    } else {
        makeConsoleId(console->id);
    }
    console->name = duplicateBoundedString(nameText.c_str(), CONSOLE_NAME_MAX - 1);
    if (console->name == nullptr) {
        free(console);
        return ConsoleWriteOutOfMemory;
    }
    console->active = active;
    console->columns = (uint8_t)columns;
    if (!parseWidgets(objectJson, &console->widgets)) {
        freeConsole(console);
        const size_t freeHeap = ESP.getFreeHeap();
        const size_t maxBlock = ESP.getMaxAllocHeap();
        LOGGER.error(
            String("Console widgets parse failed free=")
            + String((unsigned long)freeHeap)
            + " maxBlock="
            + String((unsigned long)maxBlock)
        );
        // Contiguous heap too small → OOM; otherwise treat as malformed console JSON.
        if (maxBlock < 2048UL || freeHeap < CONSOLE_HEAP_RESERVE_BYTES + 2048UL) {
            return ConsoleWriteOutOfMemory;
        }
        return ConsoleWriteParseError;
    }
    *outConsole = console;
    return ConsoleWriteOk;
}

bool ConsoleStore::loadFromFile() {
    File file = LittleFS.open(CONSOLES_STORE_PATH, "r");
    if (!file) {
        return false;
    }
    String json = file.readString();
    file.close();
    json.trim();
    if (json.length() == 0 || json.charAt(0) != '[') {
        return false;
    }
    return replaceAllFromJson(json);
}

bool ConsoleStore::replaceAllFromJson(const String &json) {
    ConsoleRecord *newHead = nullptr;
    ConsoleRecord *tail = nullptr;
    int count = 0;
    const char *cursor = json.c_str();
    while (cursor != nullptr && *cursor != '\0' && count < CONSOLE_STORE_MAX) {
        const char *objectStart = strchr(cursor, '{');
        if (objectStart == nullptr) {
            break;
        }
        int depth = 0;
        bool inString = false;
        bool escape = false;
        const char *objectEnd = nullptr;
        for (const char *scan = objectStart; *scan != '\0'; scan++) {
            const char character = *scan;
            if (inString) {
                if (escape) {
                    escape = false;
                    continue;
                }
                if (character == '\\') {
                    escape = true;
                    continue;
                }
                if (character == '"') {
                    inString = false;
                }
                continue;
            }
            if (character == '"') {
                inString = true;
                continue;
            }
            if (character == '{') {
                depth++;
                continue;
            }
            if (character == '}') {
                depth--;
                if (depth == 0) {
                    objectEnd = scan;
                    break;
                }
            }
        }
        if (objectEnd == nullptr) {
            freeConsoleList(newHead);
            return false;
        }
        String object;
        if (!assignJsonSlice(objectStart, objectEnd, object)) {
            freeConsoleList(newHead);
            return false;
        }
        ConsoleRecord *console = nullptr;
        if (parseOneConsole(object.c_str(), &console, true) != ConsoleWriteOk || console == nullptr) {
            freeConsoleList(newHead);
            return false;
        }
        if (tail == nullptr) {
            newHead = console;
        } else {
            tail->next = console;
        }
        tail = console;
        count++;
        cursor = objectEnd + 1;
    }
    clearAll();
    head = newHead;
    storedCount = count;
    if (persistEnabled) {
        requestPersist();
    }
    return true;
}

ConsoleWriteResult ConsoleStore::createConsole(const char *name, bool active, ConsoleRecord **createdOut) {
    if (storedCount >= CONSOLE_STORE_MAX) {
        return ConsoleWriteFull;
    }
    if (name == nullptr || name[0] == '\0' || strlen(name) >= CONSOLE_NAME_MAX) {
        return ConsoleWriteBadName;
    }
    if (!heapAllowsAllocation(sizeof(ConsoleRecord))) {
        return ConsoleWriteOutOfMemory;
    }
    ConsoleRecord *console = (ConsoleRecord *)malloc(sizeof(ConsoleRecord));
    if (console == nullptr) {
        return ConsoleWriteOutOfMemory;
    }
    memset(console, 0, sizeof(*console));
    makeConsoleId(console->id);
    while (findById(console->id) != nullptr) {
        makeConsoleId(console->id);
    }
    console->name = duplicateBoundedString(name, CONSOLE_NAME_MAX - 1);
    if (console->name == nullptr) {
        free(console);
        return ConsoleWriteOutOfMemory;
    }
    console->active = active;
    console->columns = CONSOLE_GRID_COLUMNS_DEFAULT;
    console->widgets = nullptr;
    console->next = nullptr;
    if (head == nullptr) {
        head = console;
    } else {
        ConsoleRecord *tail = head;
        while (tail->next != nullptr) {
            tail = tail->next;
        }
        tail->next = console;
    }
    storedCount++;
    if (persistEnabled) {
        requestPersist();
    }
    if (createdOut != nullptr) {
        *createdOut = console;
    }
    return ConsoleWriteOk;
}

ConsoleWriteResult ConsoleStore::replaceConsoleFromJson(const char *consoleId, const char *json, bool keepId) {
    ConsoleRecord *parsed = nullptr;
    const ConsoleWriteResult parseResult = parseOneConsole(json, &parsed, false);
    if (parseResult != ConsoleWriteOk || parsed == nullptr) {
        return parseResult;
    }
    ConsoleRecord *existing = nullptr;
    if (consoleId != nullptr && consoleId[0] != '\0') {
        existing = findById(consoleId);
    }
    if (existing == nullptr && keepId) {
        freeConsole(parsed);
        return ConsoleWriteNotFound;
    }
    if (existing != nullptr) {
        if (keepId) {
            strncpy(parsed->id, existing->id, sizeof(parsed->id) - 1);
        }
        free(existing->name);
        freeWidgetList(existing->widgets);
        existing->name = parsed->name;
        existing->active = parsed->active;
        existing->columns = parsed->columns;
        existing->widgets = parsed->widgets;
        parsed->name = nullptr;
        parsed->widgets = nullptr;
        freeConsole(parsed);
        if (persistEnabled) {
            requestPersist();
        }
        return ConsoleWriteOk;
    }
    if (storedCount >= CONSOLE_STORE_MAX) {
        freeConsole(parsed);
        return ConsoleWriteFull;
    }
    if (keepId && consoleId != nullptr && consoleId[0] != '\0') {
        strncpy(parsed->id, consoleId, sizeof(parsed->id) - 1);
        parsed->id[sizeof(parsed->id) - 1] = '\0';
    }
    while (findById(parsed->id) != nullptr) {
        makeConsoleId(parsed->id);
    }
    parsed->next = nullptr;
    if (head == nullptr) {
        head = parsed;
    } else {
        ConsoleRecord *tail = head;
        while (tail->next != nullptr) {
            tail = tail->next;
        }
        tail->next = parsed;
    }
    storedCount++;
    if (persistEnabled) {
        requestPersist();
    }
    return ConsoleWriteOk;
}

ConsoleWriteResult ConsoleStore::deleteConsole(const char *consoleId) {
    if (consoleId == nullptr || consoleId[0] == '\0') {
        return ConsoleWriteBadId;
    }
    ConsoleRecord *previous = nullptr;
    ConsoleRecord *current = head;
    while (current != nullptr) {
        if (strcmp(current->id, consoleId) == 0) {
            if (previous == nullptr) {
                head = current->next;
            } else {
                previous->next = current->next;
            }
            freeConsole(current);
            storedCount--;
            if (persistEnabled) {
                requestPersist();
            }
            return ConsoleWriteOk;
        }
        previous = current;
        current = current->next;
    }
    return ConsoleWriteNotFound;
}

void ConsoleStore::appendOneBindingJson(String &json, const ConsoleBinding *binding) {
    if (binding == nullptr) {
        return;
    }
    json += "{";
    if (binding->isSpacer) {
        json += "\"spacer\":true";
        if (binding->title != nullptr && binding->title[0] != '\0') {
            json += ",\"text\":\"";
            appendJsonEscaped(json, binding->title, CONSOLE_BINDING_TITLE_MAX);
            json += "\"";
        }
        if (binding->alignH != CONSOLE_ICON_ALIGN_CENTER) {
            json += ",\"alignH\":\"";
            json += iconAlignJsonId(binding->alignH);
            json += "\"";
        }
        if (binding->alignV != CONSOLE_ALIGN_V_CENTER) {
            json += ",\"alignV\":\"";
            json += alignVJsonId(binding->alignV);
            json += "\"";
        }
        if (binding->fontScale > 0.0f && binding->fontScale != 1.0f) {
            json += ",\"fontScale\":";
            json += String(binding->fontScale, 4);
        }
    } else if (binding->automationName != nullptr && binding->automationName[0] != '\0') {
        json += "\"automation\":\"";
        appendJsonEscaped(json, binding->automationName, AUTOMATION_NAME_MAX);
        json += "\",\"title\":\"";
        appendJsonEscaped(json, binding->title != nullptr ? binding->title : "", CONSOLE_BINDING_TITLE_MAX);
        json += "\"";
        if (binding->viewStyle == CONSOLE_VIEW_STYLE_ICON) {
            json += ",\"viewStyle\":\"icon\"";
            if (binding->icon != nullptr && binding->icon[0] != '\0') {
                json += ",\"icon\":\"";
                appendJsonEscaped(json, binding->icon, CONSOLE_ICON_MAX);
                json += "\"";
            }
        }
        appendBindingLayoutJson(json, binding);
    } else {
        json += "\"ieee\":\"";
        appendJsonEscaped(json, binding->ieeeText != nullptr ? binding->ieeeText : "", IEEE_TEXT_MAX);
        json += "\",\"channel\":";
        json += String(binding->channel);
        json += ",\"title\":\"";
        appendJsonEscaped(json, binding->title != nullptr ? binding->title : "", CONSOLE_BINDING_TITLE_MAX);
        json += "\",\"unit\":\"";
        appendJsonEscaped(json, binding->unit != nullptr ? binding->unit : "", CONSOLE_UNIT_MAX);
        json += "\",\"decimals\":";
        json += String((int)binding->decimals);
        if (binding->param != nullptr && binding->param[0] != '\0') {
            json += ",\"param\":\"";
            appendJsonEscaped(json, binding->param, CONSOLE_PARAM_MAX);
            json += "\"";
        }
        if (binding->cluster > 0) {
            json += ",\"cluster\":";
            json += String((unsigned int)binding->cluster);
        }
        if (binding->attrId >= 0) {
            json += ",\"attrId\":";
            json += String((long)binding->attrId);
        }
        if (binding->viewStyle == CONSOLE_VIEW_STYLE_GAUGE) {
            json += ",\"viewStyle\":\"gauge\"";
            appendGaugeJson(json, binding);
        } else if (binding->viewStyle == CONSOLE_VIEW_STYLE_ICON) {
            json += ",\"viewStyle\":\"icon\"";
            if (binding->icon != nullptr && binding->icon[0] != '\0') {
                json += ",\"icon\":\"";
                appendJsonEscaped(json, binding->icon, CONSOLE_ICON_MAX);
                json += "\"";
            }
        }
        appendBindingLayoutJson(json, binding);
    }
    json += "}";
}

void ConsoleStore::appendCompositeRowsJson(String &json, const ConsoleCompositeRow *rows) {
    json += ",\"compositeRows\":[";
    bool firstRow = true;
    for (const ConsoleCompositeRow *row = rows; row != nullptr; row = row->next) {
        if (!firstRow) {
            json += ",";
        }
        firstRow = false;
        json += "{\"height\":";
        json += String(row->height, 4);
        json += ",\"heightUnit\":\"";
        json += sizeUnitJsonId(row->heightUnit);
        json += "\",\"cells\":[";
        bool firstCell = true;
        for (const ConsoleCompositeCell *cell = row->cells; cell != nullptr; cell = cell->next) {
            if (!firstCell) {
                json += ",";
            }
            firstCell = false;
            json += "{\"width\":";
            json += String(cell->width, 4);
            json += ",\"widthUnit\":\"";
            json += sizeUnitJsonId(cell->widthUnit);
            json += "\",\"contour\":";
            json += cell->contour ? "true" : "false";
            json += ",\"alignH\":\"";
            json += iconAlignJsonId(cell->alignH);
            json += "\",\"alignV\":\"";
            json += alignVJsonId(cell->alignV);
            json += "\",\"content\":\"";
            json += cellContentJsonId(cell->content);
            json += "\"";
            if (cell->content == CONSOLE_CELL_CONTENT_TEXT) {
                json += ",\"text\":\"";
                appendJsonEscaped(json, cell->text != nullptr ? cell->text : "", CONSOLE_COMPOSITE_TEXT_MAX);
                json += "\"";
            } else if (cell->content == CONSOLE_CELL_CONTENT_DEVICE && cell->binding != nullptr) {
                json += ",\"binding\":";
                appendOneBindingJson(json, cell->binding);
            }
            json += "}";
        }
        json += "]}";
    }
    json += "]";
}

void ConsoleStore::appendConsoleFull(String &json, const ConsoleRecord *console) {
    json += "{\"id\":\"";
    appendJsonEscaped(json, console->id, sizeof(console->id));
    json += "\",\"name\":\"";
    appendJsonEscaped(json, console->name != nullptr ? console->name : "", CONSOLE_NAME_MAX);
    json += "\",\"active\":";
    json += console->active ? "true" : "false";
    json += ",\"columns\":";
    json += String((unsigned int)(console->columns > 0 ? console->columns : CONSOLE_GRID_COLUMNS_DEFAULT));
    json += ",\"widgets\":[";
    bool firstWidget = true;
    for (const ConsoleWidget *widget = console->widgets; widget != nullptr; widget = widget->next) {
        if (widget->isSpacer) {
            continue;
        }
        if (!firstWidget) {
            json += ",";
        }
        firstWidget = false;
        json += "{\"title\":\"";
        appendJsonEscaped(json, widget->title != nullptr ? widget->title : "", CONSOLE_WIDGET_TITLE_MAX);
        json += "\",\"showGroupOnOff\":";
        json += widget->showGroupOnOff ? "true" : "false";
        if (!widget->showTitle) {
            json += ",\"showTitle\":false";
        }
        json += ",\"fullWidth\":";
        json += widget->fullWidth ? "true" : "false";
        json += ",\"viewport\":\"";
        json += viewportJsonId(widget->viewport);
        json += "\"";
        if (widget->viewport == CONSOLE_VIEWPORT_GRID) {
            json += ",\"gridColumns\":";
            json += String((unsigned int)(widget->gridColumns > 0 ? widget->gridColumns : CONSOLE_WIDGET_GRID_DEFAULT));
            json += ",\"gridRows\":";
            json += String((unsigned int)(widget->gridRows > 0 ? widget->gridRows : CONSOLE_WIDGET_GRID_DEFAULT));
        }
        if (widget->viewport == CONSOLE_VIEWPORT_COMPOSITE) {
            appendCompositeRowsJson(json, widget->compositeRows);
            json += ",\"devices\":[]";
        } else {
            json += ",\"devices\":[";
            bool firstBinding = true;
            for (const ConsoleBinding *binding = widget->bindings; binding != nullptr; binding = binding->next) {
                if (!firstBinding) {
                    json += ",";
                }
                firstBinding = false;
                appendOneBindingJson(json, binding);
            }
            json += "]";
        }
        json += "}";
    }
    json += "]}";
}

void ConsoleStore::appendGaugeJson(String &json, const ConsoleBinding *binding) {
    if (binding == nullptr || !binding->hasGauge) {
        return;
    }
    json += ",\"gauge\":{\"type\":\"";
    json += gaugeTypeJsonId(binding->gaugeType);
    json += "\",\"min\":";
    json += String(binding->gaugeMin, 4);
    json += ",\"max\":";
    json += String(binding->gaugeMax, 4);
    json += ",\"ranges\":[";
    bool firstRange = true;
    for (const ConsoleGaugeRange *range = binding->gaugeRanges; range != nullptr; range = range->next) {
        if (!firstRange) {
            json += ",";
        }
        firstRange = false;
        json += "{\"top\":";
        json += String(range->top, 4);
        json += ",\"color\":\"";
        appendJsonEscaped(json, range->color != nullptr ? range->color : "", CONSOLE_GAUGE_COLOR_MAX);
        json += "\"}";
    }
    json += "]}";
}

String ConsoleStore::consoleFullJson(const ConsoleRecord *console) const {
    String json;
    if (console == nullptr) {
        return String("null");
    }
    appendConsoleFull(json, console);
    return json;
}

String ConsoleStore::listFullJson() const {
    String json = "[";
    bool first = true;
    for (const ConsoleRecord *console = head; console != nullptr; console = console->next) {
        if (!first) {
            json += ",";
        }
        first = false;
        appendConsoleFull(json, console);
    }
    json += "]";
    return json;
}

String ConsoleStore::listSummaryJson(UserStore *userStore) const {
    String json = "[";
    bool first = true;
    for (const ConsoleRecord *console = head; console != nullptr; console = console->next) {
        if (!first) {
            json += ",";
        }
        first = false;
        json += "{\"id\":\"";
        appendJsonEscaped(json, console->id, sizeof(console->id));
        json += "\",\"name\":\"";
        appendJsonEscaped(json, console->name != nullptr ? console->name : "", CONSOLE_NAME_MAX);
        json += "\",\"active\":";
        json += console->active ? "true" : "false";
        json += ",\"users\":[";
        bool firstUser = true;
        if (userStore != nullptr) {
            for (int index = 0; index < userStore->userCount(); index++) {
                const UserRecord *user = userStore->userAt(index);
                if (user == nullptr || !UserStore::userHasConsole(user, console->id)) {
                    continue;
                }
                if (!firstUser) {
                    json += ",";
                }
                firstUser = false;
                json += "\"";
                appendJsonEscaped(json, user->userName, sizeof(user->userName));
                json += "\"";
            }
        }
        json += "]}";
    }
    json += "]";
    return json;
}

String ConsoleStore::mineJson(const UserRecord *user) const {
    String json = "[";
    bool first = true;
    if (user != nullptr) {
        for (const UserConsoleIdNode *node = user->consoleIds; node != nullptr; node = node->next) {
            const ConsoleRecord *console = findById(node->id);
            if (console == nullptr || !console->active) {
                continue;
            }
            if (!first) {
                json += ",";
            }
            first = false;
            appendConsoleFull(json, console);
        }
    }
    json += "]";
    return json;
}
