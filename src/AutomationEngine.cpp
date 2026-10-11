#include "AutomationEngine.h"
#include "DeviceTopicMap.h"
#include "Logger.h"
#include "ZigbeeSpiProxy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace {
constexpr unsigned long AUTOMATION_START_ON_BOOT_DELAY_MS = 20000UL;
}

AutomationEngine::AutomationEngine()
    : store(nullptr),
      spiProxy(nullptr),
      topicMap(nullptr),
      deviceCommand(nullptr),
      lastTickMs(0),
      actionDepth(0),
      bootStartApplied(false) {
}

void AutomationEngine::begin(
    AutomationStore *automationStore,
    ZigbeeSpiProxy *proxy,
    DeviceTopicMap *map,
    DeviceCommandFn commandFn
) {
    store = automationStore;
    spiProxy = proxy;
    topicMap = map;
    deviceCommand = commandFn;
    lastTickMs = millis();
    bootStartApplied = false;
}

void AutomationEngine::applyStartOnBootTimers() {
    if (store == nullptr || bootStartApplied) {
        return;
    }
    if (millis() < AUTOMATION_START_ON_BOOT_DELAY_MS) {
        return;
    }
    bootStartApplied = true;
    for (AutomationRecord *automation = store->first(); automation != nullptr; automation = automation->next) {
        if (!automation->active || !automation->timer.use || !automation->timer.startOnBoot) {
            continue;
        }
        if (automation->timer.counting || automation->timer.defaultTime == 0) {
            continue;
        }
        store->startTimer(automation, 0);
        LOGGER.info(
            String("Automation timer start-on-boot: ")
            + (automation->name != nullptr ? automation->name : "?")
        );
    }
}

uint16_t AutomationEngine::parseClusterFromState(const char *state) {
    if (state == nullptr) {
        return 0;
    }
    const char *clusterMark = strstr(state, "cl=0x");
    if (clusterMark == nullptr) {
        clusterMark = strstr(state, "cl=");
        if (clusterMark == nullptr) {
            return 0;
        }
        clusterMark += 3;
        return (uint16_t)strtoul(clusterMark, nullptr, 0);
    }
    clusterMark += 5;
    return (uint16_t)strtoul(clusterMark, nullptr, 16);
}

bool AutomationEngine::compareValues(AutomationCompareOp op, const char *left, const char *right) {
    if (left == nullptr) {
        left = "";
    }
    if (right == nullptr) {
        right = "";
    }
    char *leftEnd = nullptr;
    char *rightEnd = nullptr;
    const double leftNumber = strtod(left, &leftEnd);
    const double rightNumber = strtod(right, &rightEnd);
    const bool bothNumeric =
        leftEnd != left && rightEnd != right && *leftEnd == '\0' && *rightEnd == '\0';
    const bool bothTimeOfDay =
        AutomationStore::isTimeOfDayValue(left) && AutomationStore::isTimeOfDayValue(right);
    switch (op) {
        case AutomationCmpEqual:
            return strcmp(left, right) == 0;
        case AutomationCmpGreater:
            if (bothNumeric) {
                return leftNumber > rightNumber;
            }
            if (bothTimeOfDay) {
                return strcmp(left, right) > 0;
            }
            return false;
        case AutomationCmpLess:
            if (bothNumeric) {
                return leftNumber < rightNumber;
            }
            if (bothTimeOfDay) {
                return strcmp(left, right) < 0;
            }
            return false;
        case AutomationCmpContains:
            return strstr(left, right) != nullptr;
        case AutomationCmpStarts:
            return strncmp(left, right, strlen(right)) == 0;
        case AutomationCmpEnds: {
            const size_t leftLength = strlen(left);
            const size_t rightLength = strlen(right);
            if (rightLength > leftLength) {
                return false;
            }
            return strcmp(left + (leftLength - rightLength), right) == 0;
        }
        default:
            return false;
    }
}

bool AutomationEngine::resolveStatus(
    const char *ieeeText,
    uint8_t channel,
    bool hasCluster,
    uint16_t cluster,
    char *out,
    size_t outLen
) const {
    if (out == nullptr || outLen == 0 || spiProxy == nullptr || topicMap == nullptr || ieeeText == nullptr) {
        return false;
    }
    out[0] = '\0';
    uint8_t ieee[8];
    if (!topicMap->parseIeee(ieeeText, ieee)) {
        return false;
    }
    if (!spiProxy->copyEndpointState(ieee, channel, out, outLen)) {
        return false;
    }
    if (hasCluster) {
        const uint16_t parsedCluster = parseClusterFromState(out);
        if (parsedCluster != 0 && parsedCluster != cluster) {
            out[0] = '\0';
            return false;
        }
    }
    return out[0] != '\0';
}

bool AutomationEngine::formatCurrentTimeOfDay(char *out, size_t outLen) const {
    if (out == nullptr || outLen < 9) {
        return false;
    }
    const time_t wallClock = time(nullptr);
    if (wallClock < 1700000000) {
        out[0] = '\0';
        return false;
    }
    struct tm timeInfo;
    if (localtime_r(&wallClock, &timeInfo) == nullptr) {
        out[0] = '\0';
        return false;
    }
    snprintf(
        out,
        outLen,
        "%02d:%02d:%02d",
        timeInfo.tm_hour,
        timeInfo.tm_min,
        timeInfo.tm_sec
    );
    return true;
}

bool AutomationEngine::evaluateComparison(const AutomationCondition *condition) const {
    if (condition == nullptr) {
        return false;
    }
    char statusText[SPI_DEVICE_MESSAGE_MAX];
    if (AutomationStore::isCurrentTimeIeee(condition->ieeeText)) {
        if ((condition->cmp != AutomationCmpGreater && condition->cmp != AutomationCmpLess)
            || !AutomationStore::isTimeOfDayValue(condition->value)
            || !formatCurrentTimeOfDay(statusText, sizeof(statusText))) {
            return false;
        }
        return compareValues(condition->cmp, statusText, condition->value);
    }
    if (!resolveStatus(
            condition->ieeeText,
            condition->channel,
            condition->hasCluster,
            condition->cluster,
            statusText,
            sizeof(statusText)
        )) {
        return false;
    }
    return compareValues(condition->cmp, statusText, condition->value);
}

bool AutomationEngine::evaluateCondition(const AutomationCondition *condition) const {
    if (condition == nullptr) {
        return true;
    }
    bool result = false;
    if (condition->kind == AutomationConditionComparison) {
        result = evaluateComparison(condition);
    } else {
        int trueCount = 0;
        int childCount = 0;
        for (const AutomationCondition *child = condition->children; child != nullptr; child = child->nextSibling) {
            childCount++;
            if (evaluateCondition(child)) {
                trueCount++;
            }
        }
        switch (condition->containerOp) {
            case AutomationContainerOr:
                result = trueCount > 0;
                break;
            case AutomationContainerXor:
                result = trueCount == 1;
                break;
            case AutomationContainerAnd:
            default:
                result = childCount > 0 && trueCount == childCount;
                if (childCount == 0) {
                    result = true;
                }
                break;
        }
    }
    if (condition->notFlag) {
        result = !result;
    }
    return result;
}

void AutomationEngine::applyAction(const AutomationAction *action) {
    if (action == nullptr || store == nullptr) {
        return;
    }
    if (action->target == AutomationTargetDevice) {
        if (deviceCommand == nullptr || action->ieeeText == nullptr || action->payload == nullptr) {
            return;
        }
        deviceCommand(action->ieeeText, action->payload, (int)action->channel);
        return;
    }
    AutomationRecord *target = store->findByName(action->automationName);
    if (target == nullptr) {
        LOGGER.warning("Automation action target missing");
        return;
    }
    switch (action->automationOp) {
        case AutomationOpEnable:
            store->setActive(target->name, true, nullptr);
            break;
        case AutomationOpDisable:
            store->setActive(target->name, false, nullptr);
            break;
        case AutomationOpStopTimer:
            if (!target->timer.use) {
                return;
            }
            store->stopTimer(target);
            break;
        case AutomationOpStartTimer:
            if (!target->timer.use) {
                return;
            }
            store->startTimer(target, action->hasStartTime ? action->startTime : 0);
            break;
        case AutomationOpRestartTimer:
            if (!target->timer.use) {
                return;
            }
            store->restartTimer(target, action->hasStartTime ? action->startTime : 0);
            break;
        default:
            break;
    }
}

void AutomationEngine::runActions(AutomationRecord *automation) {
    if (automation == nullptr || actionDepth >= 4) {
        return;
    }
    actionDepth++;
    for (const AutomationAction *action = automation->actions; action != nullptr; action = action->next) {
        applyAction(action);
    }
    const time_t wallClock = time(nullptr);
    automation->lastActivationMs = (wallClock > 1700000000) ? (unsigned long)wallClock : 0UL;
    actionDepth--;
}

bool AutomationEngine::isMinTriggerIntervalBlocking(const AutomationRecord *automation) const {
    if (automation == nullptr || automation->minTriggerIntervalSec == 0) {
        return false;
    }
    if (automation->lastActivationMs <= 1700000000UL) {
        return false;
    }
    const time_t now = time(nullptr);
    if (now <= 1700000000) {
        return false;
    }
    const unsigned long elapsed = (unsigned long)now - automation->lastActivationMs;
    return elapsed < (unsigned long)automation->minTriggerIntervalSec;
}

void AutomationEngine::evaluateAndMaybeRun(AutomationRecord *automation) {
    if (automation == nullptr || !automation->active || store == nullptr) {
        return;
    }
    if (isMinTriggerIntervalBlocking(automation)) {
        return;
    }
    const bool hasCompleteCondition = AutomationStore::conditionIsComplete(automation->condition);
    if (!automation->timer.use && !hasCompleteCondition) {
        return;
    }
    // Timer with no complete condition tree: treat conditions as TRUE (timer-only automations).
    if (automation->timer.use && !hasCompleteCondition) {
        LOGGER.info(String("Automation ran (timer): ") + (automation->name != nullptr ? automation->name : "?"));
        automation->lastConditionTrue = true;
        runActions(automation);
        return;
    }
    const bool conditionTrue = evaluateCondition(automation->condition);
    automation->lastConditionTrue = conditionTrue;
    if (!conditionTrue) {
        return;
    }
    LOGGER.info(String("Automation ran: ") + (automation->name != nullptr ? automation->name : "?"));
    runActions(automation);
}

void AutomationEngine::evaluateCurrentTimeAutomations() {
    if (store == nullptr) {
        return;
    }
    for (AutomationRecord *automation = store->first(); automation != nullptr; automation = automation->next) {
        if (!automation->active || !AutomationStore::conditionUsesCurrentTime(automation->condition)) {
            continue;
        }
        if (isMinTriggerIntervalBlocking(automation)) {
            continue;
        }
        if (!AutomationStore::conditionIsComplete(automation->condition)) {
            continue;
        }
        const bool conditionTrue = evaluateCondition(automation->condition);
        const bool risingEdge = conditionTrue && !automation->lastConditionTrue;
        automation->lastConditionTrue = conditionTrue;
        if (!risingEdge) {
            continue;
        }
        LOGGER.info(
            String("Automation ran (time): ")
            + (automation->name != nullptr ? automation->name : "?")
        );
        runActions(automation);
    }
}

void AutomationEngine::evaluateMatching(
    const char *ieeeText,
    uint8_t endpoint,
    uint16_t clusterOr0,
    bool fromTimer,
    AutomationRecord *only
) {
    if (store == nullptr) {
        return;
    }
    for (AutomationRecord *automation = store->first(); automation != nullptr; automation = automation->next) {
        if (only != nullptr && automation != only) {
            continue;
        }
        if (!automation->active) {
            continue;
        }
        if (fromTimer) {
            evaluateAndMaybeRun(automation);
            continue;
        }
        bool matched = false;
        for (const AutomationListener *listener = automation->listen; listener != nullptr; listener = listener->next) {
            if (listener->ieeeText == nullptr || ieeeText == nullptr) {
                continue;
            }
            if (strcasecmp(listener->ieeeText, ieeeText) != 0) {
                continue;
            }
            if (listener->hasCluster && clusterOr0 != 0 && listener->cluster != clusterOr0) {
                continue;
            }
            if (listener->hasCluster && clusterOr0 == 0) {
                continue;
            }
            (void)endpoint;
            matched = true;
            break;
        }
        if (matched) {
            evaluateAndMaybeRun(automation);
        }
    }
}

void AutomationEngine::onStatusChanged(
    const uint8_t ieee[8],
    uint8_t endpoint,
    uint16_t clusterOr0,
    const char *newState
) {
    (void)newState;
    if (topicMap == nullptr || ieee == nullptr) {
        return;
    }
    const String ieeeText = topicMap->formatIeee(ieee);
    evaluateMatching(ieeeText.c_str(), endpoint, clusterOr0, false, nullptr);
}

void AutomationEngine::tick() {
    if (store == nullptr) {
        return;
    }
    applyStartOnBootTimers();
    const unsigned long now = millis();
    if (lastTickMs == 0) {
        lastTickMs = now;
        return;
    }
    if ((long)(now - lastTickMs) < 1000L) {
        return;
    }
    uint32_t elapsedSeconds = (uint32_t)((now - lastTickMs) / 1000UL);
    if (elapsedSeconds == 0) {
        return;
    }
    lastTickMs += elapsedSeconds * 1000UL;
    for (AutomationRecord *automation = store->first(); automation != nullptr; automation = automation->next) {
        if (!automation->active || !automation->timer.use || !automation->timer.counting) {
            continue;
        }
        if (automation->timer.timeLeft > elapsedSeconds) {
            automation->timer.timeLeft -= elapsedSeconds;
            continue;
        }
        automation->timer.timeLeft = 0;
        automation->timer.counting = false;
        evaluateAndMaybeRun(automation);
        if (automation->active && automation->timer.autoRestart && automation->timer.defaultTime > 0) {
            automation->timer.timeLeft = automation->timer.defaultTime;
            automation->timer.counting = true;
        }
    }
    evaluateCurrentTimeAutomations();
}
