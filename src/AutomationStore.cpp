#include "AutomationStore.h"
#include "AutomationParam.h"
#include "JsonField.h"
#include "Logger.h"

#include <LittleFS.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

namespace {

const char *automationWriteResultText(AutomationWriteResult result) {
    switch (result) {
        case AutomationWriteOk:
            return "ok";
        case AutomationWriteNotFound:
            return "not found";
        case AutomationWriteFull:
            return "store full";
        case AutomationWriteBadName:
            return "bad name";
        case AutomationWriteOutOfMemory:
            return "out of memory";
        case AutomationWriteParseError:
            return "parse error";
        case AutomationWriteValidation:
            return "validation failed";
        default:
            return "unknown error";
    }
}

}  // namespace

AutomationStore::AutomationStore()
    : head(nullptr), storedCount(0), persistEnabled(false), persistPending(false) {
}

AutomationStore::~AutomationStore() {
    clearAll();
}

bool AutomationStore::heapAllowsAllocation(size_t bytes) {
    if (bytes == 0) {
        return true;
    }
    const size_t freeHeap = ESP.getFreeHeap();
    const size_t maxBlock = ESP.getMaxAllocHeap();
    if (maxBlock < bytes || freeHeap < bytes) {
        return false;
    }
    if (freeHeap - bytes < AUTOMATION_HEAP_RESERVE_BYTES) {
        return false;
    }
    return true;
}

char *AutomationStore::duplicateBoundedString(const char *source, size_t maxLength) {
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

bool AutomationStore::isNameSafe(const char *name) {
    if (name == nullptr || name[0] == '\0' || strlen(name) >= AUTOMATION_NAME_MAX) {
        return false;
    }
    for (const char *cursor = name; *cursor != '\0'; cursor++) {
        const unsigned char character = (unsigned char)*cursor;
        if (isalnum(character) || character == '-' || character == '_' || character == '.'
            || character == ' ') {
            continue;
        }
        return false;
    }
    return true;
}

bool AutomationStore::isCurrentTimeIeee(const char *ieeeText) {
    return ieeeText != nullptr && strcmp(ieeeText, AUTOMATION_IEEE_CURRENT_TIME) == 0;
}

bool AutomationStore::isTimeOfDayValue(const char *value) {
    if (value == nullptr || strlen(value) != 8) {
        return false;
    }
    if (value[2] != ':' || value[5] != ':') {
        return false;
    }
    for (int index = 0; index < 8; index++) {
        if (index == 2 || index == 5) {
            continue;
        }
        if (value[index] < '0' || value[index] > '9') {
            return false;
        }
    }
    const int hours = (value[0] - '0') * 10 + (value[1] - '0');
    const int minutes = (value[3] - '0') * 10 + (value[4] - '0');
    const int seconds = (value[6] - '0') * 10 + (value[7] - '0');
    return hours >= 0 && hours <= 23 && minutes >= 0 && minutes <= 59 && seconds >= 0 && seconds <= 59;
}

bool AutomationStore::conditionUsesCurrentTime(const AutomationCondition *node) {
    if (node == nullptr) {
        return false;
    }
    for (const AutomationCondition *sibling = node; sibling != nullptr; sibling = sibling->nextSibling) {
        if (sibling->kind == AutomationConditionComparison) {
            if (isCurrentTimeIeee(sibling->ieeeText)) {
                return true;
            }
            continue;
        }
        if (conditionUsesCurrentTime(sibling->children)) {
            return true;
        }
    }
    return false;
}

void AutomationStore::freeListenerList(AutomationListener *&listHead) {
    while (listHead != nullptr) {
        AutomationListener *next = listHead->next;
        free(listHead->ieeeText);
        free(listHead->param);
        free(listHead);
        listHead = next;
    }
    listHead = nullptr;
}

void AutomationStore::freeConditionTree(AutomationCondition *&root) {
    if (root == nullptr) {
        return;
    }
    freeConditionTree(root->children);
    freeConditionTree(root->nextSibling);
    free(root->ieeeText);
    free(root->param);
    free(root->value);
    free(root);
    root = nullptr;
}

void AutomationStore::freeActionList(AutomationAction *&listHead) {
    while (listHead != nullptr) {
        AutomationAction *next = listHead->next;
        free(listHead->ieeeText);
        free(listHead->param);
        free(listHead->payload);
        free(listHead->customValue);
        free(listHead->automationName);
        free(listHead);
        listHead = next;
    }
    listHead = nullptr;
}

void AutomationStore::freeAutomation(AutomationRecord *automation) {
    if (automation == nullptr) {
        return;
    }
    free(automation->name);
    free(automation->title);
    free(automation->group);
    freeListenerList(automation->listen);
    freeConditionTree(automation->condition);
    freeActionList(automation->actions);
    free(automation);
}

void AutomationStore::freeAutomationList(AutomationRecord *&listHead) {
    while (listHead != nullptr) {
        AutomationRecord *next = listHead->next;
        freeAutomation(listHead);
        listHead = next;
    }
    listHead = nullptr;
}

void AutomationStore::clearAll() {
    freeAutomationList(head);
    storedCount = 0;
}

int AutomationStore::automationCount() const {
    return storedCount;
}

AutomationRecord *AutomationStore::first() {
    return head;
}

const AutomationRecord *AutomationStore::first() const {
    return head;
}

AutomationRecord *AutomationStore::findByName(const char *name) {
    return const_cast<AutomationRecord *>(static_cast<const AutomationStore *>(this)->findByName(name));
}

const AutomationRecord *AutomationStore::findByName(const char *name) const {
    if (name == nullptr || name[0] == '\0') {
        return nullptr;
    }
    for (const AutomationRecord *automation = head; automation != nullptr; automation = automation->next) {
        if (automation->name != nullptr && strcmp(automation->name, name) == 0) {
            return automation;
        }
    }
    return nullptr;
}

bool AutomationStore::begin(bool persistToLittleFs) {
    persistEnabled = persistToLittleFs;
    if (!persistEnabled) {
        return true;
    }
    if (loadFromFile()) {
        LOGGER.info("Loaded " + String(storedCount) + " automation(s)");
        return true;
    }
    clearAll();
    return true;
}

void AutomationStore::requestPersist() {
    persistPending = true;
}

void AutomationStore::persistIfDue() {
    if (!persistPending) {
        return;
    }
    if (!persistEnabled) {
        persistPending = false;
        return;
    }
    persistPending = false;
    if (!saveToFile()) {
        LOGGER.error("Automation store write failed");
        persistPending = true;
        return;
    }
    LOGGER.info("Automation store saved " + String(storedCount) + " automation(s)");
}

bool AutomationStore::saveToFile() const {
    File file = LittleFS.open(AUTOMATIONS_STORE_PATH, "w");
    if (!file) {
        LOGGER.error("Automations file write failed");
        return false;
    }
    const String json = listFullJson();
    const size_t written = file.print(json);
    file.close();
    return written == json.length();
}

bool AutomationStore::loadFromFile() {
    File file = LittleFS.open(AUTOMATIONS_STORE_PATH, "r");
    if (!file) {
        return false;
    }
    String json = file.readString();
    file.close();
    json.trim();
    if (json.length() == 0 || json.charAt(0) != '[') {
        LOGGER.error("Automations file invalid (empty or not a JSON array)");
        return false;
    }
    if (!replaceAllFromJson(json)) {
        LOGGER.error("Automations file load/import rejected");
        return false;
    }
    return true;
}

int AutomationStore::countConditionNodes(const AutomationCondition *node) {
    if (node == nullptr) {
        return 0;
    }
    int count = 0;
    for (const AutomationCondition *sibling = node; sibling != nullptr; sibling = sibling->nextSibling) {
        if (sibling->kind == AutomationConditionComparison) {
            count++;
        } else {
            count += countConditionNodes(sibling->children);
        }
    }
    return count;
}

bool AutomationStore::conditionIsComplete(const AutomationCondition *node) {
    if (node == nullptr) {
        return false;
    }
    for (const AutomationCondition *sibling = node; sibling != nullptr; sibling = sibling->nextSibling) {
        if (sibling->kind == AutomationConditionComparison) {
            if (sibling->ieeeText != nullptr && sibling->ieeeText[0] != '\0'
                && sibling->value != nullptr && sibling->value[0] != '\0') {
                return true;
            }
            continue;
        }
        if (conditionIsComplete(sibling->children)) {
            return true;
        }
    }
    return false;
}

static bool validateConditionTree(const AutomationCondition *node) {
    if (node == nullptr) {
        return true;
    }
    for (const AutomationCondition *sibling = node; sibling != nullptr; sibling = sibling->nextSibling) {
        if (sibling->kind == AutomationConditionComparison) {
            if (AutomationStore::isCurrentTimeIeee(sibling->ieeeText)) {
                if ((sibling->cmp != AutomationCmpGreater && sibling->cmp != AutomationCmpLess)
                    || !AutomationStore::isTimeOfDayValue(sibling->value)) {
                    return false;
                }
            }
            continue;
        }
        if (!validateConditionTree(sibling->children)) {
            return false;
        }
    }
    return true;
}

static bool validateTypedListener(const AutomationListener *listener) {
    if (listener == nullptr) {
        return false;
    }
    if (listener->param == nullptr || listener->param[0] == '\0') {
        return true;
    }
    if (automationParamIsCustom(listener->param)) {
        return listener->hasCluster && listener->hasAttrId;
    }
    return automationParamIsKnownKey(listener->param);
}

static bool validateTypedConditionNode(const AutomationCondition *node) {
    if (node == nullptr) {
        return true;
    }
    for (const AutomationCondition *sibling = node; sibling != nullptr; sibling = sibling->nextSibling) {
        if (sibling->kind == AutomationConditionComparison) {
            if (sibling->param != nullptr && sibling->param[0] != '\0') {
                if (automationParamIsCustom(sibling->param)) {
                    if (!sibling->hasCluster || !sibling->hasAttrId) {
                        return false;
                    }
                    if (sibling->cmp != AutomationCmpEqual
                        && sibling->cmp != AutomationCmpGreater
                        && sibling->cmp != AutomationCmpLess) {
                        return false;
                    }
                    if (sibling->value == nullptr || sibling->value[0] == '\0') {
                        return false;
                    }
                    // GREATER/LESS need numbers; EQUAL may be a status string (e.g. DRY).
                    if (sibling->cmp == AutomationCmpGreater || sibling->cmp == AutomationCmpLess) {
                        double unused = 0;
                        if (!automationParseNumericVal(sibling->value, unused)) {
                            return false;
                        }
                    }
                } else if (!automationParamIsKnownKey(sibling->param)) {
                    return false;
                }
            }
            continue;
        }
        if (!validateTypedConditionNode(sibling->children)) {
            return false;
        }
    }
    return true;
}

static bool validateTypedActions(const AutomationAction *actions) {
    for (const AutomationAction *action = actions; action != nullptr; action = action->next) {
        if (action->target != AutomationTargetDevice) {
            continue;
        }
        if (action->hasCustomCommand || automationParamIsCustom(action->param)) {
            if (!action->hasCustomCommand || action->customCluster == 0) {
                return false;
            }
            continue;
        }
        // Catalog commands, typed command/value text, or any non-empty wire payload.
        if (!automationActionPayloadOk(action->payload)) {
            return false;
        }
    }
    return true;
}

bool AutomationStore::validateAutomation(const AutomationRecord *automation) {
    if (automation == nullptr || !isNameSafe(automation->name)) {
        return false;
    }
    if (!validateConditionTree(automation->condition)) {
        return false;
    }
    if (!validateTypedConditionNode(automation->condition)) {
        return false;
    }
    for (const AutomationListener *listener = automation->listen; listener != nullptr; listener = listener->next) {
        if (!validateTypedListener(listener)) {
            return false;
        }
    }
    if (!validateTypedActions(automation->actions)) {
        return false;
    }
    if (automation->timer.use
        && (automation->timer.autoRestart || automation->timer.startOnBoot)
        && automation->timer.defaultTime == 0) {
        return false;
    }
    if (automation->timer.use) {
        return true;
    }
    if (!automation->useCondition) {
        return automation->listen != nullptr;
    }
    if (conditionIsComplete(automation->condition) && conditionUsesCurrentTime(automation->condition)) {
        return true;
    }
    if (automation->listen == nullptr) {
        return false;
    }
    return conditionIsComplete(automation->condition);
}

const char *AutomationStore::compareOpToText(AutomationCompareOp op) {
    switch (op) {
        case AutomationCmpGreater: return "GREATER";
        case AutomationCmpLess: return "LESS";
        case AutomationCmpContains: return "CONTAINS";
        case AutomationCmpStarts: return "STARTS";
        case AutomationCmpEnds: return "ENDS";
        case AutomationCmpEqual:
        default: return "EQUAL";
    }
}

bool AutomationStore::compareOpFromText(const char *text, AutomationCompareOp &op) {
    if (text == nullptr) {
        return false;
    }
    if (strcmp(text, "EQUAL") == 0) {
        op = AutomationCmpEqual;
        return true;
    }
    if (strcmp(text, "GREATER") == 0) {
        op = AutomationCmpGreater;
        return true;
    }
    if (strcmp(text, "LESS") == 0) {
        op = AutomationCmpLess;
        return true;
    }
    if (strcmp(text, "CONTAINS") == 0) {
        op = AutomationCmpContains;
        return true;
    }
    if (strcmp(text, "STARTS") == 0) {
        op = AutomationCmpStarts;
        return true;
    }
    if (strcmp(text, "ENDS") == 0) {
        op = AutomationCmpEnds;
        return true;
    }
    return false;
}

const char *AutomationStore::containerOpToText(AutomationContainerOp op) {
    switch (op) {
        case AutomationContainerOr: return "OR";
        case AutomationContainerXor: return "XOR";
        case AutomationContainerAnd:
        default: return "AND";
    }
}

bool AutomationStore::containerOpFromText(const char *text, AutomationContainerOp &op) {
    if (text == nullptr) {
        return false;
    }
    if (strcmp(text, "AND") == 0) {
        op = AutomationContainerAnd;
        return true;
    }
    if (strcmp(text, "OR") == 0) {
        op = AutomationContainerOr;
        return true;
    }
    if (strcmp(text, "XOR") == 0) {
        op = AutomationContainerXor;
        return true;
    }
    return false;
}

const char *AutomationStore::automationOpToText(AutomationAutomationOp op) {
    switch (op) {
        case AutomationOpDisable: return "DISABLE";
        case AutomationOpStopTimer: return "STOP TIMER";
        case AutomationOpStartTimer: return "START TIMER";
        case AutomationOpRestartTimer: return "RESTART TIMER";
        case AutomationOpEnable:
        default: return "ENABLE";
    }
}

bool AutomationStore::automationOpFromText(const char *text, AutomationAutomationOp &op) {
    if (text == nullptr) {
        return false;
    }
    if (strcmp(text, "ENABLE") == 0) {
        op = AutomationOpEnable;
        return true;
    }
    if (strcmp(text, "DISABLE") == 0) {
        op = AutomationOpDisable;
        return true;
    }
    if (strcmp(text, "STOP TIMER") == 0) {
        op = AutomationOpStopTimer;
        return true;
    }
    if (strcmp(text, "START TIMER") == 0) {
        op = AutomationOpStartTimer;
        return true;
    }
    if (strcmp(text, "RESTART TIMER") == 0) {
        op = AutomationOpRestartTimer;
        return true;
    }
    return false;
}

bool AutomationStore::parseTimer(const char *objectJson, AutomationTimer &timer) {
    memset(&timer, 0, sizeof(timer));
    String timerJson;
    if (!extractJsonKeyedSlice(objectJson, "timer", '{', timerJson)) {
        return true;
    }
    extractJsonBool(timerJson.c_str(), "use", timer.use);
    extractJsonBool(timerJson.c_str(), "autoRestart", timer.autoRestart);
    extractJsonBool(timerJson.c_str(), "startOnBoot", timer.startOnBoot);
    int defaultTime = 0;
    if (extractJsonInt(timerJson.c_str(), "defaultTime", defaultTime) && defaultTime > 0) {
        timer.defaultTime = (uint32_t)defaultTime;
    }
    return true;
}

bool AutomationStore::parseListeners(const char *objectJson, AutomationListener **outListeners) {
    if (outListeners == nullptr) {
        return false;
    }
    *outListeners = nullptr;
    String listenJson;
    if (!extractJsonKeyedSlice(objectJson, "listen", '[', listenJson)) {
        return true;
    }
    AutomationListener *tail = nullptr;
    const char *cursor = listenJson.c_str();
    while (cursor != nullptr && *cursor != '\0') {
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
            freeListenerList(*outListeners);
            return false;
        }
        String object = String(objectStart).substring(0, (unsigned int)(objectEnd - objectStart + 1));
        String ieeeText;
        if (!extractJsonString(object.c_str(), "ieee", ieeeText) || ieeeText.length() == 0) {
            freeListenerList(*outListeners);
            return false;
        }
        if (!heapAllowsAllocation(sizeof(AutomationListener))) {
            freeListenerList(*outListeners);
            return false;
        }
        AutomationListener *listener = (AutomationListener *)malloc(sizeof(AutomationListener));
        if (listener == nullptr) {
            freeListenerList(*outListeners);
            return false;
        }
        memset(listener, 0, sizeof(*listener));
        listener->ieeeText = duplicateBoundedString(ieeeText.c_str(), IEEE_TEXT_MAX - 1);
        if (listener->ieeeText == nullptr) {
            free(listener);
            freeListenerList(*outListeners);
            return false;
        }
        listener->channel = 0;
        if (strstr(object.c_str(), "\"channel\"") != nullptr) {
            int channel = 1;
            if (extractJsonInt(object.c_str(), "channel", channel) && channel > 0) {
                if (channel > DEVICE_CHANNEL_COUNT_MAX) {
                    channel = DEVICE_CHANNEL_COUNT_MAX;
                }
                listener->channel = (uint8_t)channel;
            }
        }
        String paramText;
        if (extractJsonString(object.c_str(), "param", paramText) && paramText.length() > 0) {
            listener->param = duplicateBoundedString(paramText.c_str(), AUTOMATION_PARAM_KEY_MAX - 1);
            if (listener->param == nullptr) {
                freeListenerList(*outListeners);
                free(listener->ieeeText);
                free(listener);
                return false;
            }
        }
        int cluster = 0;
        if (extractJsonInt(object.c_str(), "cluster", cluster) && cluster > 0) {
            listener->hasCluster = true;
            listener->cluster = (uint16_t)cluster;
        }
        if (strstr(object.c_str(), "\"attrId\"") != nullptr) {
            int attrId = 0;
            if (extractJsonInt(object.c_str(), "attrId", attrId)) {
                listener->hasAttrId = true;
                listener->attrId = (uint16_t)attrId;
            }
        }
        listener->eventType = AutomationListenEventChange;
        String eventTypeText;
        if (extractJsonString(object.c_str(), "eventType", eventTypeText)) {
            if (eventTypeText.equalsIgnoreCase("any")) {
                listener->eventType = AutomationListenEventAny;
            } else {
                listener->eventType = AutomationListenEventChange;
            }
        }
        if (tail == nullptr) {
            *outListeners = listener;
        } else {
            tail->next = listener;
        }
        tail = listener;
        cursor = objectEnd + 1;
    }
    return true;
}

bool AutomationStore::parseCondition(const char *conditionJson, AutomationCondition **outCondition, int depth) {
    if (outCondition == nullptr) {
        return false;
    }
    *outCondition = nullptr;
    if (conditionJson == nullptr || conditionJson[0] == '\0') {
        return true;
    }
    if (depth > AUTOMATION_CONDITION_DEPTH_MAX) {
        return false;
    }
    if (!heapAllowsAllocation(sizeof(AutomationCondition))) {
        return false;
    }
    AutomationCondition *condition = (AutomationCondition *)malloc(sizeof(AutomationCondition));
    if (condition == nullptr) {
        return false;
    }
    memset(condition, 0, sizeof(*condition));
    String kindText;
    if (!extractJsonString(conditionJson, "kind", kindText)) {
        free(condition);
        return false;
    }
    extractJsonBool(conditionJson, "not", condition->notFlag);
    if (kindText == "comparison") {
        condition->kind = AutomationConditionComparison;
        String ieeeText;
        String cmpText;
        String valueText;
        if (!extractJsonString(conditionJson, "ieee", ieeeText)
            || !extractJsonString(conditionJson, "cmp", cmpText)
            || !extractJsonString(conditionJson, "value", valueText)
            || !compareOpFromText(cmpText.c_str(), condition->cmp)) {
            free(condition);
            return false;
        }
        condition->ieeeText = duplicateBoundedString(ieeeText.c_str(), IEEE_TEXT_MAX - 1);
        condition->value = duplicateBoundedString(valueText.c_str(), AUTOMATION_VALUE_MAX - 1);
        if (condition->ieeeText == nullptr || condition->value == nullptr) {
            freeConditionTree(condition);
            return false;
        }
        int channel = 1;
        extractJsonInt(conditionJson, "channel", channel);
        if (channel < 1) {
            channel = 1;
        }
        if (channel > DEVICE_CHANNEL_COUNT_MAX) {
            channel = DEVICE_CHANNEL_COUNT_MAX;
        }
        condition->channel = (uint8_t)channel;
        String paramText;
        if (extractJsonString(conditionJson, "param", paramText) && paramText.length() > 0) {
            condition->param = duplicateBoundedString(paramText.c_str(), AUTOMATION_PARAM_KEY_MAX - 1);
            if (condition->param == nullptr) {
                freeConditionTree(condition);
                return false;
            }
        }
        int cluster = 0;
        if (extractJsonInt(conditionJson, "cluster", cluster) && cluster > 0) {
            condition->hasCluster = true;
            condition->cluster = (uint16_t)cluster;
        }
        if (strstr(conditionJson, "\"attrId\"") != nullptr) {
            int attrId = 0;
            if (extractJsonInt(conditionJson, "attrId", attrId)) {
                condition->hasAttrId = true;
                condition->attrId = (uint16_t)attrId;
            }
        }
        *outCondition = condition;
        return true;
    }
    if (kindText != "container") {
        free(condition);
        return false;
    }
    condition->kind = AutomationConditionContainer;
    String opText;
    if (!extractJsonString(conditionJson, "op", opText) || !containerOpFromText(opText.c_str(), condition->containerOp)) {
        free(condition);
        return false;
    }
    String childrenJson;
    if (extractJsonKeyedSlice(conditionJson, "children", '[', childrenJson)) {
        AutomationCondition *tail = nullptr;
        int childCount = 0;
        const char *cursor = childrenJson.c_str();
        while (cursor != nullptr && *cursor != '\0') {
            const char *objectStart = strchr(cursor, '{');
            if (objectStart == nullptr) {
                break;
            }
            int nest = 0;
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
                    nest++;
                    continue;
                }
                if (character == '}') {
                    nest--;
                    if (nest == 0) {
                        objectEnd = scan;
                        break;
                    }
                }
            }
            if (objectEnd == nullptr || childCount >= AUTOMATION_CONTAINER_CHILDREN_MAX) {
                freeConditionTree(condition);
                return false;
            }
            String childObject = String(objectStart).substring(0, (unsigned int)(objectEnd - objectStart + 1));
            AutomationCondition *child = nullptr;
            if (!parseCondition(childObject.c_str(), &child, depth + 1) || child == nullptr) {
                freeConditionTree(condition);
                return false;
            }
            if (tail == nullptr) {
                condition->children = child;
            } else {
                tail->nextSibling = child;
            }
            tail = child;
            childCount++;
            cursor = objectEnd + 1;
        }
    }
    *outCondition = condition;
    return true;
}

bool AutomationStore::parseActions(const char *objectJson, AutomationAction **outActions) {
    if (outActions == nullptr) {
        return false;
    }
    *outActions = nullptr;
    String actionsJson;
    if (!extractJsonKeyedSlice(objectJson, "actions", '[', actionsJson)) {
        return true;
    }
    AutomationAction *tail = nullptr;
    const char *cursor = actionsJson.c_str();
    while (cursor != nullptr && *cursor != '\0') {
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
            freeActionList(*outActions);
            return false;
        }
        String object = String(objectStart).substring(0, (unsigned int)(objectEnd - objectStart + 1));
        if (!heapAllowsAllocation(sizeof(AutomationAction))) {
            freeActionList(*outActions);
            return false;
        }
        AutomationAction *action = (AutomationAction *)malloc(sizeof(AutomationAction));
        if (action == nullptr) {
            freeActionList(*outActions);
            return false;
        }
        memset(action, 0, sizeof(*action));
        String targetText;
        extractJsonString(object.c_str(), "target", targetText);
        if (targetText.length() == 0 || targetText == "device") {
            action->target = AutomationTargetDevice;
            String ieeeText;
            String payload;
            const bool haveIeee = extractJsonString(object.c_str(), "ieee", ieeeText) && ieeeText.length() > 0;
            const bool havePayload = extractJsonString(object.c_str(), "payload", payload);
            int customCluster = 0;
            int customCommand = 0;
            String customValue;
            const bool haveCustomCluster =
                extractJsonInt(object.c_str(), "cluster", customCluster) && customCluster > 0;
            const bool haveCustomCommand =
                strstr(object.c_str(), "\"command\"") != nullptr
                && extractJsonInt(object.c_str(), "command", customCommand);
            extractJsonString(object.c_str(), "value", customValue);
            String paramText;
            extractJsonString(object.c_str(), "param", paramText);
            const bool isCustomParam = automationParamIsCustom(paramText.c_str());
            if (!haveIeee) {
                free(action);
                freeActionList(*outActions);
                return false;
            }
            if (isCustomParam || (haveCustomCluster && haveCustomCommand)) {
                if (!haveCustomCluster || !haveCustomCommand) {
                    free(action);
                    freeActionList(*outActions);
                    return false;
                }
                action->hasCustomCommand = true;
                action->customCluster = (uint16_t)customCluster;
                action->customCommand = (uint16_t)customCommand;
                action->customValue = duplicateBoundedString(
                    customValue.c_str(),
                    AUTOMATION_VALUE_MAX - 1
                );
                if (action->customValue == nullptr) {
                    free(action);
                    freeActionList(*outActions);
                    return false;
                }
                char builtPayload[AUTOMATION_VALUE_MAX];
                if (!automationBuildCustomPayload(
                        action->customCluster,
                        action->customCommand,
                        action->customValue,
                        builtPayload,
                        sizeof(builtPayload)
                    )) {
                    free(action->customValue);
                    free(action);
                    freeActionList(*outActions);
                    return false;
                }
                action->payload = duplicateBoundedString(builtPayload, AUTOMATION_VALUE_MAX - 1);
            } else {
                if (!havePayload) {
                    free(action);
                    freeActionList(*outActions);
                    return false;
                }
                action->payload = duplicateBoundedString(payload.c_str(), AUTOMATION_VALUE_MAX - 1);
            }
            action->ieeeText = duplicateBoundedString(ieeeText.c_str(), IEEE_TEXT_MAX - 1);
            if (action->ieeeText == nullptr || action->payload == nullptr) {
                free(action->ieeeText);
                free(action->payload);
                free(action->customValue);
                free(action);
                freeActionList(*outActions);
                return false;
            }
            if (paramText.length() > 0) {
                action->param = duplicateBoundedString(paramText.c_str(), AUTOMATION_PARAM_KEY_MAX - 1);
                if (action->param == nullptr) {
                    free(action->ieeeText);
                    free(action->payload);
                    free(action->customValue);
                    free(action);
                    freeActionList(*outActions);
                    return false;
                }
            }
            int channel = 1;
            extractJsonInt(object.c_str(), "channel", channel);
            if (channel < 1) {
                channel = 1;
            }
            action->channel = (uint8_t)channel;
        } else if (targetText == "automation") {
            action->target = AutomationTargetAutomation;
            String nameText;
            String opText;
            if (!extractJsonString(object.c_str(), "name", nameText)
                || !extractJsonString(object.c_str(), "op", opText)
                || !automationOpFromText(opText.c_str(), action->automationOp)
                || !isNameSafe(nameText.c_str())) {
                free(action);
                freeActionList(*outActions);
                return false;
            }
            action->automationName = duplicateBoundedString(nameText.c_str(), AUTOMATION_NAME_MAX - 1);
            if (action->automationName == nullptr) {
                free(action);
                freeActionList(*outActions);
                return false;
            }
            int startTime = 0;
            if (extractJsonInt(object.c_str(), "time", startTime) && startTime > 0) {
                action->hasStartTime = true;
                action->startTime = (uint32_t)startTime;
            }
        } else {
            free(action);
            freeActionList(*outActions);
            return false;
        }
        if (tail == nullptr) {
            *outActions = action;
        } else {
            tail->next = action;
        }
        tail = action;
        cursor = objectEnd + 1;
    }
    return true;
}

AutomationWriteResult AutomationStore::parseOneAutomation(
    const char *objectJson,
    AutomationRecord **outAutomation,
    bool requireName
) {
    if (outAutomation == nullptr || objectJson == nullptr) {
        return AutomationWriteParseError;
    }
    *outAutomation = nullptr;
    String nameText;
    String titleText;
    String groupText;
    const bool haveName = extractJsonString(objectJson, "name", nameText);
    extractJsonString(objectJson, "title", titleText);
    extractJsonString(objectJson, "group", groupText);
    if (requireName && (!haveName || !isNameSafe(nameText.c_str()))) {
        return AutomationWriteBadName;
    }
    if (haveName && !isNameSafe(nameText.c_str())) {
        return AutomationWriteBadName;
    }
    if (groupText.length() > 0 && !isNameSafe(groupText.c_str())) {
        return AutomationWriteBadName;
    }
    if (!heapAllowsAllocation(sizeof(AutomationRecord))) {
        return AutomationWriteOutOfMemory;
    }
    AutomationRecord *automation = (AutomationRecord *)malloc(sizeof(AutomationRecord));
    if (automation == nullptr) {
        return AutomationWriteOutOfMemory;
    }
    memset(automation, 0, sizeof(*automation));
    if (haveName) {
        automation->name = duplicateBoundedString(nameText.c_str(), AUTOMATION_NAME_MAX - 1);
        if (automation->name == nullptr) {
            free(automation);
            return AutomationWriteOutOfMemory;
        }
    }
    automation->title = duplicateBoundedString(titleText.c_str(), AUTOMATION_TITLE_MAX - 1);
    automation->group = duplicateBoundedString(groupText.c_str(), AUTOMATION_GROUP_MAX - 1);
    if (automation->title == nullptr || automation->group == nullptr) {
        freeAutomation(automation);
        return AutomationWriteOutOfMemory;
    }
    bool active = false;
    extractJsonBool(objectJson, "active", active);
    automation->active = active;
    bool useCondition = true;
    if (!extractJsonBool(objectJson, "useCondition", useCondition)) {
        useCondition = true;
    }
    automation->useCondition = useCondition;
    bool debugEnabled = false;
    if (!extractJsonBool(objectJson, "debug", debugEnabled)) {
        debugEnabled = false;
    }
    automation->debug = debugEnabled;
    int minTriggerIntervalSec = 0;
    if (extractJsonInt(objectJson, "minTriggerIntervalSec", minTriggerIntervalSec) && minTriggerIntervalSec > 0) {
        automation->minTriggerIntervalSec = (uint32_t)minTriggerIntervalSec;
    } else {
        automation->minTriggerIntervalSec = 0;
    }
    if (!parseTimer(objectJson, automation->timer)
        || !parseListeners(objectJson, &automation->listen)
        || !parseActions(objectJson, &automation->actions)) {
        freeAutomation(automation);
        return AutomationWriteParseError;
    }
    String conditionJson;
    if (extractJsonKeyedSlice(objectJson, "condition", '{', conditionJson)) {
        if (!parseCondition(conditionJson.c_str(), &automation->condition, 1)) {
            freeAutomation(automation);
            return AutomationWriteParseError;
        }
    }
    if (!validateAutomation(automation)) {
        freeAutomation(automation);
        return AutomationWriteValidation;
    }
    *outAutomation = automation;
    return AutomationWriteOk;
}

bool AutomationStore::replaceAllFromJson(const String &json) {
    AutomationRecord *newHead = nullptr;
    AutomationRecord *tail = nullptr;
    int count = 0;
    const char *cursor = json.c_str();
    while (cursor != nullptr && *cursor != '\0' && count < AUTOMATION_STORE_MAX) {
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
            LOGGER.error(
                "Automation import failed: truncated JSON object at index " + String(count)
            );
            freeAutomationList(newHead);
            return false;
        }
        String object = String(objectStart).substring(0, (unsigned int)(objectEnd - objectStart + 1));
        AutomationRecord *automation = nullptr;
        const AutomationWriteResult parseResult = parseOneAutomation(object.c_str(), &automation, true);
        if (parseResult != AutomationWriteOk || automation == nullptr) {
            String nameHint;
            extractJsonString(object.c_str(), "name", nameHint);
            String message = "Automation import failed at index " + String(count);
            if (nameHint.length() > 0) {
                message += " name=";
                message += nameHint;
            }
            message += ": ";
            message += automationWriteResultText(parseResult);
            LOGGER.error(message);
            freeAutomationList(newHead);
            return false;
        }
        if (tail == nullptr) {
            newHead = automation;
        } else {
            tail->next = automation;
        }
        tail = automation;
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

AutomationWriteResult AutomationStore::createAutomation(
    const char *name,
    const char *title,
    bool active,
    AutomationRecord **createdOut
) {
    if (storedCount >= AUTOMATION_STORE_MAX) {
        return AutomationWriteFull;
    }
    if (!isNameSafe(name) || findByName(name) != nullptr) {
        return AutomationWriteBadName;
    }
    if (!heapAllowsAllocation(sizeof(AutomationRecord))) {
        return AutomationWriteOutOfMemory;
    }
    AutomationRecord *automation = (AutomationRecord *)malloc(sizeof(AutomationRecord));
    if (automation == nullptr) {
        return AutomationWriteOutOfMemory;
    }
    memset(automation, 0, sizeof(*automation));
    automation->name = duplicateBoundedString(name, AUTOMATION_NAME_MAX - 1);
    automation->title = duplicateBoundedString(title != nullptr ? title : "", AUTOMATION_TITLE_MAX - 1);
    automation->group = duplicateBoundedString("", AUTOMATION_GROUP_MAX - 1);
    if (automation->name == nullptr || automation->title == nullptr || automation->group == nullptr) {
        freeAutomation(automation);
        return AutomationWriteOutOfMemory;
    }
    automation->active = active;
    automation->useCondition = false;
    automation->debug = false;
    automation->timer.use = false;
    automation->timer.startOnBoot = false;
    automation->next = nullptr;
    if (head == nullptr) {
        head = automation;
    } else {
        AutomationRecord *tail = head;
        while (tail->next != nullptr) {
            tail = tail->next;
        }
        tail->next = automation;
    }
    storedCount++;
    if (persistEnabled) {
        requestPersist();
    }
    if (createdOut != nullptr) {
        *createdOut = automation;
    }
    return AutomationWriteOk;
}

AutomationWriteResult AutomationStore::replaceFromJson(const char *name, const char *json, bool keepName) {
    AutomationRecord *parsed = nullptr;
    const AutomationWriteResult parseResult = parseOneAutomation(json, &parsed, false);
    if (parseResult != AutomationWriteOk || parsed == nullptr) {
        return parseResult;
    }
    AutomationRecord *existing = nullptr;
    if (name != nullptr && name[0] != '\0') {
        existing = findByName(name);
    }
    if (existing == nullptr && keepName) {
        freeAutomation(parsed);
        return AutomationWriteNotFound;
    }
    if (parsed->name == nullptr) {
        if (name != nullptr && isNameSafe(name)) {
            parsed->name = duplicateBoundedString(name, AUTOMATION_NAME_MAX - 1);
        }
        if (parsed->name == nullptr) {
            freeAutomation(parsed);
            return AutomationWriteBadName;
        }
    }
    if (!validateAutomation(parsed)) {
        freeAutomation(parsed);
        return AutomationWriteValidation;
    }
    if (existing != nullptr) {
        if (parsed->name != nullptr && existing->name != nullptr
            && strcmp(existing->name, parsed->name) != 0
            && findByName(parsed->name) != nullptr) {
            freeAutomation(parsed);
            return AutomationWriteBadName;
        }
        const bool wasActive = existing->active;
        free(existing->name);
        free(existing->title);
        free(existing->group);
        freeListenerList(existing->listen);
        freeConditionTree(existing->condition);
        freeActionList(existing->actions);
        existing->name = parsed->name;
        existing->title = parsed->title;
        existing->group = parsed->group;
        existing->active = parsed->active;
        existing->useCondition = parsed->useCondition;
        existing->debug = parsed->debug;
        existing->minTriggerIntervalSec = parsed->minTriggerIntervalSec;
        existing->timer.use = parsed->timer.use;
        existing->timer.autoRestart = parsed->timer.autoRestart;
        existing->timer.startOnBoot = parsed->timer.startOnBoot;
        existing->timer.defaultTime = parsed->timer.defaultTime;
        existing->listen = parsed->listen;
        existing->condition = parsed->condition;
        existing->actions = parsed->actions;
        parsed->name = nullptr;
        parsed->title = nullptr;
        parsed->group = nullptr;
        parsed->listen = nullptr;
        parsed->condition = nullptr;
        parsed->actions = nullptr;
        freeAutomation(parsed);
        if (!wasActive && existing->active && existing->timer.use && existing->timer.defaultTime > 0
            && (existing->timer.autoRestart || existing->timer.startOnBoot)) {
            existing->timer.timeLeft = existing->timer.defaultTime;
            existing->timer.counting = true;
        }
        if (!existing->active) {
            stopTimer(existing);
        }
        if (persistEnabled) {
            requestPersist();
        }
        return AutomationWriteOk;
    }
    if (storedCount >= AUTOMATION_STORE_MAX) {
        freeAutomation(parsed);
        return AutomationWriteFull;
    }
    if (findByName(parsed->name) != nullptr) {
        freeAutomation(parsed);
        return AutomationWriteBadName;
    }
    parsed->next = nullptr;
    if (head == nullptr) {
        head = parsed;
    } else {
        AutomationRecord *tail = head;
        while (tail->next != nullptr) {
            tail = tail->next;
        }
        tail->next = parsed;
    }
    storedCount++;
    if (parsed->active && parsed->timer.use && parsed->timer.defaultTime > 0
        && (parsed->timer.autoRestart || parsed->timer.startOnBoot)) {
        parsed->timer.timeLeft = parsed->timer.defaultTime;
        parsed->timer.counting = true;
    }
    if (persistEnabled) {
        requestPersist();
    }
    return AutomationWriteOk;
}

AutomationWriteResult AutomationStore::deleteAutomation(const char *name) {
    if (name == nullptr || name[0] == '\0') {
        return AutomationWriteBadName;
    }
    AutomationRecord *previous = nullptr;
    AutomationRecord *current = head;
    while (current != nullptr) {
        if (current->name != nullptr && strcmp(current->name, name) == 0) {
            if (previous == nullptr) {
                head = current->next;
            } else {
                previous->next = current->next;
            }
            freeAutomation(current);
            storedCount--;
            if (persistEnabled) {
                requestPersist();
            }
            return AutomationWriteOk;
        }
        previous = current;
        current = current->next;
    }
    return AutomationWriteNotFound;
}

bool AutomationStore::setActive(const char *name, bool active, bool *becameActiveOut) {
    if (becameActiveOut != nullptr) {
        *becameActiveOut = false;
    }
    AutomationRecord *automation = findByName(name);
    if (automation == nullptr) {
        return false;
    }
    const bool wasActive = automation->active;
    automation->active = active;
    if (!wasActive && active) {
        if (becameActiveOut != nullptr) {
            *becameActiveOut = true;
        }
        automation->lastConditionTrue = false;
        if (automation->timer.use && automation->timer.defaultTime > 0
            && (automation->timer.autoRestart || automation->timer.startOnBoot)) {
            automation->timer.timeLeft = automation->timer.defaultTime;
            automation->timer.counting = true;
        }
    }
    if (!active) {
        stopTimer(automation);
        automation->lastConditionTrue = false;
    }
    if (persistEnabled) {
        requestPersist();
    }
    return true;
}

void AutomationStore::stopTimer(AutomationRecord *automation) {
    if (automation == nullptr) {
        return;
    }
    automation->timer.counting = false;
    automation->timer.timeLeft = 0;
}

bool AutomationStore::startTimer(AutomationRecord *automation, uint32_t secondsOrZero) {
    if (automation == nullptr || !automation->timer.use) {
        return false;
    }
    uint32_t seconds = secondsOrZero;
    if (seconds == 0) {
        seconds = automation->timer.defaultTime;
    }
    if (seconds == 0) {
        return false;
    }
    automation->timer.timeLeft = seconds;
    automation->timer.counting = true;
    return true;
}

void AutomationStore::restartTimer(AutomationRecord *automation, uint32_t secondsOrZero) {
    stopTimer(automation);
    startTimer(automation, secondsOrZero);
}

void AutomationStore::appendConditionJson(String &json, const AutomationCondition *condition) {
    if (condition == nullptr) {
        json += "null";
        return;
    }
    json += "{\"kind\":\"";
    json += condition->kind == AutomationConditionComparison ? "comparison" : "container";
    json += "\",\"not\":";
    json += condition->notFlag ? "true" : "false";
    if (condition->kind == AutomationConditionComparison) {
        json += ",\"ieee\":\"";
        appendJsonEscaped(json, condition->ieeeText, IEEE_TEXT_MAX);
        json += "\",\"channel\":";
        json += String((unsigned)condition->channel);
        if (condition->param != nullptr && condition->param[0] != '\0') {
            json += ",\"param\":\"";
            appendJsonEscaped(json, condition->param, AUTOMATION_PARAM_KEY_MAX);
            json += "\"";
        }
        if (condition->hasCluster) {
            json += ",\"cluster\":";
            json += String((unsigned)condition->cluster);
        }
        if (condition->hasAttrId) {
            json += ",\"attrId\":";
            json += String((unsigned)condition->attrId);
        }
        json += ",\"cmp\":\"";
        json += compareOpToText(condition->cmp);
        json += "\",\"value\":\"";
        appendJsonEscaped(json, condition->value, AUTOMATION_VALUE_MAX);
        json += "\"}";
        return;
    }
    json += ",\"op\":\"";
    json += containerOpToText(condition->containerOp);
    json += "\",\"children\":[";
    bool first = true;
    for (const AutomationCondition *child = condition->children; child != nullptr; child = child->nextSibling) {
        if (!first) {
            json += ",";
        }
        first = false;
        appendConditionJson(json, child);
    }
    json += "]}";
}

void AutomationStore::appendActionJson(String &json, const AutomationAction *action) {
    if (action == nullptr) {
        return;
    }
    if (action->target == AutomationTargetDevice) {
        json += "{\"target\":\"device\",\"ieee\":\"";
        appendJsonEscaped(json, action->ieeeText, IEEE_TEXT_MAX);
        json += "\",\"channel\":";
        json += String((unsigned)action->channel);
        if (action->param != nullptr && action->param[0] != '\0') {
            json += ",\"param\":\"";
            appendJsonEscaped(json, action->param, AUTOMATION_PARAM_KEY_MAX);
            json += "\"";
        }
        if (action->hasCustomCommand) {
            json += ",\"cluster\":";
            json += String((unsigned)action->customCluster);
            json += ",\"command\":";
            json += String((unsigned)action->customCommand);
            json += ",\"value\":\"";
            appendJsonEscaped(
                json,
                action->customValue != nullptr ? action->customValue : "",
                AUTOMATION_VALUE_MAX
            );
            json += "\"";
        }
        json += ",\"payload\":\"";
        appendJsonEscaped(json, action->payload, AUTOMATION_VALUE_MAX);
        json += "\"}";
        return;
    }
    json += "{\"target\":\"automation\",\"name\":\"";
    appendJsonEscaped(json, action->automationName, AUTOMATION_NAME_MAX);
    json += "\",\"op\":\"";
    json += automationOpToText(action->automationOp);
    json += "\"";
    if (action->hasStartTime) {
        json += ",\"time\":";
        json += String((unsigned long)action->startTime);
    }
    json += "}";
}

void AutomationStore::appendAutomationFull(String &json, const AutomationRecord *automation, bool includeRuntime) {
    if (automation == nullptr) {
        return;
    }
    json += "{\"name\":\"";
    appendJsonEscaped(json, automation->name, AUTOMATION_NAME_MAX);
    json += "\",\"title\":\"";
    appendJsonEscaped(json, automation->title, AUTOMATION_TITLE_MAX);
    json += "\",\"group\":\"";
    appendJsonEscaped(json, automation->group != nullptr ? automation->group : "", AUTOMATION_GROUP_MAX);
    json += "\",\"active\":";
    json += automation->active ? "true" : "false";
    json += ",\"useCondition\":";
    json += automation->useCondition ? "true" : "false";
    json += ",\"debug\":";
    json += automation->debug ? "true" : "false";
    json += ",\"minTriggerIntervalSec\":";
    json += String((unsigned long)automation->minTriggerIntervalSec);
    json += ",\"timer\":{\"use\":";
    json += automation->timer.use ? "true" : "false";
    json += ",\"autoRestart\":";
    json += automation->timer.autoRestart ? "true" : "false";
    json += ",\"startOnBoot\":";
    json += automation->timer.startOnBoot ? "true" : "false";
    json += ",\"defaultTime\":";
    json += String((unsigned long)automation->timer.defaultTime);
    if (includeRuntime) {
        json += ",\"counting\":";
        json += automation->timer.counting ? "true" : "false";
        json += ",\"timeLeft\":";
        json += String((unsigned long)automation->timer.timeLeft);
    }
    json += "},\"listen\":[";
    bool firstListen = true;
    for (const AutomationListener *listener = automation->listen; listener != nullptr; listener = listener->next) {
        if (!firstListen) {
            json += ",";
        }
        firstListen = false;
        json += "{\"ieee\":\"";
        appendJsonEscaped(json, listener->ieeeText, IEEE_TEXT_MAX);
        json += "\"";
        if (listener->channel > 0) {
            json += ",\"channel\":";
            json += String((unsigned)listener->channel);
        }
        if (listener->param != nullptr && listener->param[0] != '\0') {
            json += ",\"param\":\"";
            appendJsonEscaped(json, listener->param, AUTOMATION_PARAM_KEY_MAX);
            json += "\"";
        }
        if (listener->hasCluster) {
            json += ",\"cluster\":";
            json += String((unsigned)listener->cluster);
        }
        if (listener->hasAttrId) {
            json += ",\"attrId\":";
            json += String((unsigned)listener->attrId);
        }
        json += ",\"eventType\":\"";
        json += listener->eventType == AutomationListenEventAny ? "any" : "change";
        json += "\"";
        json += "}";
    }
    json += "],\"condition\":";
    appendConditionJson(json, automation->condition);
    json += ",\"actions\":[";
    bool firstAction = true;
    for (const AutomationAction *action = automation->actions; action != nullptr; action = action->next) {
        if (!firstAction) {
            json += ",";
        }
        firstAction = false;
        appendActionJson(json, action);
    }
    json += "]}";
}

String AutomationStore::automationFullJson(const AutomationRecord *automation, bool includeRuntime) const {
    String json;
    appendAutomationFull(json, automation, includeRuntime);
    return json;
}

String AutomationStore::listFullJson() const {
    String json = "[";
    bool first = true;
    for (const AutomationRecord *automation = head; automation != nullptr; automation = automation->next) {
        if (!first) {
            json += ",";
        }
        first = false;
        appendAutomationFull(json, automation, false);
    }
    json += "]";
    return json;
}

String AutomationStore::listSummaryJson() const {
    String json = "[";
    bool first = true;
    for (const AutomationRecord *automation = head; automation != nullptr; automation = automation->next) {
        if (!first) {
            json += ",";
        }
        first = false;
        json += "{\"name\":\"";
        appendJsonEscaped(json, automation->name, AUTOMATION_NAME_MAX);
        json += "\",\"title\":\"";
        appendJsonEscaped(json, automation->title, AUTOMATION_TITLE_MAX);
        json += "\",\"group\":\"";
        appendJsonEscaped(json, automation->group != nullptr ? automation->group : "", AUTOMATION_GROUP_MAX);
        json += "\",\"active\":";
        json += automation->active ? "true" : "false";
        json += ",\"minTriggerIntervalSec\":";
        json += String((unsigned long)automation->minTriggerIntervalSec);
        json += ",\"timer\":{\"use\":";
        json += automation->timer.use ? "true" : "false";
        json += ",\"startOnBoot\":";
        json += automation->timer.startOnBoot ? "true" : "false";
        json += ",\"defaultTime\":";
        json += String((unsigned long)automation->timer.defaultTime);
        json += ",\"counting\":";
        json += automation->timer.counting ? "true" : "false";
        json += ",\"timeLeft\":";
        json += String((unsigned long)automation->timer.timeLeft);
        json += "}";
        if (automation->lastActivationMs > 1700000000UL) {
            const time_t stamp = (time_t)automation->lastActivationMs;
            struct tm timeInfo;
            char stampText[32];
            stampText[0] = '\0';
            if (localtime_r(&stamp, &timeInfo) != nullptr) {
                snprintf(
                    stampText,
                    sizeof(stampText),
                    "%04d-%02d-%02d %02d:%02d:%02d",
                    timeInfo.tm_year + 1900,
                    timeInfo.tm_mon + 1,
                    timeInfo.tm_mday,
                    timeInfo.tm_hour,
                    timeInfo.tm_min,
                    timeInfo.tm_sec
                );
            }
            if (stampText[0] != '\0') {
                json += ",\"lastActivation\":\"";
                appendJsonEscaped(json, stampText, sizeof(stampText));
                json += "\"";
            } else {
                json += ",\"lastActivation\":null";
            }
        } else {
            json += ",\"lastActivation\":null";
        }
        json += "}";
    }
    json += "]";
    return json;
}
