#pragma once

#include "AutomationStore.h"
#include <Arduino.h>
#include <stdint.h>

class ZigbeeSpiProxy;
class DeviceTopicMap;

class AutomationEngine {
public:
    using DeviceCommandFn = int (*)(const char *ieeeText, const char *payload, int channel);

    AutomationEngine();

    void begin(AutomationStore *store, ZigbeeSpiProxy *spiProxy, DeviceTopicMap *topicMap, DeviceCommandFn deviceCommand);
    void tick();
    void onStatusChanged(const uint8_t ieee[8], uint8_t endpoint, uint16_t clusterOr0, const char *newState);

private:
    AutomationStore *store;
    ZigbeeSpiProxy *spiProxy;
    DeviceTopicMap *topicMap;
    DeviceCommandFn deviceCommand;
    unsigned long lastTickMs;
    uint8_t actionDepth;
    bool bootStartApplied;

    void applyStartOnBootTimers();
    void evaluateCurrentTimeAutomations();
    void evaluateMatching(const char *ieeeText, uint8_t endpoint, uint16_t clusterOr0, bool fromTimer, AutomationRecord *only);
    void evaluateAndMaybeRun(AutomationRecord *automation, bool requireConditionRisingEdge = false);
    bool isMinTriggerIntervalBlocking(const AutomationRecord *automation) const;
    bool evaluateCondition(const AutomationCondition *condition, const AutomationRecord *automation) const;
    bool evaluateComparison(const AutomationCondition *condition, const AutomationRecord *automation) const;
    bool resolveStatus(
        const char *ieeeText,
        uint8_t channel,
        bool hasCluster,
        uint16_t cluster,
        const char *param,
        char *out,
        size_t outLen
    ) const;
    bool formatCurrentTimeOfDay(char *out, size_t outLen) const;
    void runActions(AutomationRecord *automation);
    void applyAction(const AutomationAction *action);
    static bool compareValues(AutomationCompareOp op, const char *left, const char *right);
    static uint16_t parseClusterFromState(const char *state);
    static bool listenerMatchesEvent(
        const AutomationListener *listener,
        uint8_t endpoint,
        uint16_t clusterOr0,
        const char *newState
    );
};
