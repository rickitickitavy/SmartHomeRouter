#pragma once

#include "Defines.h"
#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

enum AutomationConditionKind : uint8_t {
    AutomationConditionComparison = 0,
    AutomationConditionContainer = 1
};

enum AutomationContainerOp : uint8_t {
    AutomationContainerAnd = 0,
    AutomationContainerOr = 1,
    AutomationContainerXor = 2
};

enum AutomationCompareOp : uint8_t {
    AutomationCmpEqual = 0,
    AutomationCmpGreater = 1,
    AutomationCmpLess = 2,
    AutomationCmpContains = 3,
    AutomationCmpStarts = 4,
    AutomationCmpEnds = 5
};

enum AutomationActionTarget : uint8_t {
    AutomationTargetDevice = 0,
    AutomationTargetAutomation = 1
};

enum AutomationAutomationOp : uint8_t {
    AutomationOpEnable = 0,
    AutomationOpDisable = 1,
    AutomationOpStopTimer = 2,
    AutomationOpStartTimer = 3,
    AutomationOpRestartTimer = 4
};

struct AutomationTimer {
    bool use;
    bool autoRestart;
    bool startOnBoot;
    uint32_t defaultTime;
    bool counting;
    uint32_t timeLeft;
};

struct AutomationListener {
    char *ieeeText;
    bool hasCluster;
    uint16_t cluster;
    AutomationListener *next;
};

struct AutomationCondition {
    AutomationConditionKind kind;
    bool notFlag;
    char *ieeeText;
    uint8_t channel;
    bool hasCluster;
    uint16_t cluster;
    AutomationCompareOp cmp;
    char *value;
    AutomationContainerOp containerOp;
    AutomationCondition *children;
    AutomationCondition *nextSibling;
};

struct AutomationAction {
    AutomationActionTarget target;
    char *ieeeText;
    uint8_t channel;
    char *payload;
    char *automationName;
    AutomationAutomationOp automationOp;
    bool hasStartTime;
    uint32_t startTime;
    AutomationAction *next;
};

struct AutomationRecord {
    char *name;
    char *title;
    char *group;
    bool active;
    bool useCondition;
    bool debug;
    uint32_t minTriggerIntervalSec;
    AutomationTimer timer;
    AutomationListener *listen;
    AutomationCondition *condition;
    AutomationAction *actions;
    unsigned long lastActivationMs;
    bool lastConditionTrue;
    AutomationRecord *next;
};

enum AutomationWriteResult {
    AutomationWriteOk = 0,
    AutomationWriteNotFound,
    AutomationWriteFull,
    AutomationWriteBadName,
    AutomationWriteOutOfMemory,
    AutomationWriteParseError,
    AutomationWriteValidation
};

class AutomationStore {
public:
    AutomationStore();
    ~AutomationStore();

    bool begin(bool persistToLittleFs);
    void requestPersist();
    void persistIfDue();
    int automationCount() const;
    AutomationRecord *first();
    const AutomationRecord *first() const;
    AutomationRecord *findByName(const char *name);
    const AutomationRecord *findByName(const char *name) const;
    AutomationWriteResult createAutomation(const char *name, const char *title, bool active, AutomationRecord **createdOut);
    AutomationWriteResult replaceFromJson(const char *name, const char *json, bool keepName);
    AutomationWriteResult deleteAutomation(const char *name);
    bool replaceAllFromJson(const String &json);
    String listSummaryJson() const;
    String listFullJson() const;
    String automationFullJson(const AutomationRecord *automation, bool includeRuntime) const;
    bool setActive(const char *name, bool active, bool *becameActiveOut);
    void stopTimer(AutomationRecord *automation);
    bool startTimer(AutomationRecord *automation, uint32_t secondsOrZero);
    void restartTimer(AutomationRecord *automation, uint32_t secondsOrZero);
    static bool heapAllowsAllocation(size_t bytes);
    static char *duplicateBoundedString(const char *source, size_t maxLength);
    static void freeListenerList(AutomationListener *&head);
    static void freeConditionTree(AutomationCondition *&root);
    static void freeActionList(AutomationAction *&head);
    static void freeAutomation(AutomationRecord *automation);
    static void freeAutomationList(AutomationRecord *&head);
    static bool isNameSafe(const char *name);
    static bool isCurrentTimeIeee(const char *ieeeText);
    static bool isTimeOfDayValue(const char *value);
    static bool conditionUsesCurrentTime(const AutomationCondition *node);
    static int countConditionNodes(const AutomationCondition *node);
    static bool conditionIsComplete(const AutomationCondition *node);
    static const char *compareOpToText(AutomationCompareOp op);

private:
    AutomationRecord *head;
    int storedCount;
    bool persistEnabled;
    bool persistPending;

    bool loadFromFile();
    bool saveToFile() const;
    void clearAll();
    AutomationWriteResult parseOneAutomation(const char *objectJson, AutomationRecord **outAutomation, bool requireName);
    static bool parseTimer(const char *objectJson, AutomationTimer &timer);
    static bool parseListeners(const char *objectJson, AutomationListener **outListeners);
    static bool parseCondition(const char *conditionJson, AutomationCondition **outCondition, int depth);
    static bool parseActions(const char *objectJson, AutomationAction **outActions);
    static bool validateAutomation(const AutomationRecord *automation);
    static void appendAutomationFull(String &json, const AutomationRecord *automation, bool includeRuntime);
    static void appendConditionJson(String &json, const AutomationCondition *condition);
    static void appendActionJson(String &json, const AutomationAction *action);
    static bool compareOpFromText(const char *text, AutomationCompareOp &op);
    static const char *containerOpToText(AutomationContainerOp op);
    static bool containerOpFromText(const char *text, AutomationContainerOp &op);
    static const char *automationOpToText(AutomationAutomationOp op);
    static bool automationOpFromText(const char *text, AutomationAutomationOp &op);
};
