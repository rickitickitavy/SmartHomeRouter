#pragma once

#include "Defines.h"
#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

struct ConsoleGaugeRange {
    float top;
    char *color;
    ConsoleGaugeRange *next;
};

struct ConsoleBinding {
    char *ieeeText;
    char *automationName;
    uint8_t channel;
    char *title;
    char *unit;
    int8_t decimals;
    char *param;
    uint16_t cluster;
    int32_t attrId;
    uint8_t viewStyle;
    char *icon;
    float iconScale;
    uint8_t iconAlign;
    float fontScale;
    uint8_t alignH;
    uint8_t alignV;
    bool showTitle;
    uint8_t titlePosition;
    uint8_t gaugeType;
    float gaugeMin;
    float gaugeMax;
    bool hasGauge;
    bool isSpacer;
    ConsoleGaugeRange *gaugeRanges;
    ConsoleBinding *next;
};

struct ConsoleCompositeCell {
    float width;
    uint8_t widthUnit;
    bool contour;
    uint8_t content;
    uint8_t alignH;
    uint8_t alignV;
    char *text;
    ConsoleBinding *binding;
    ConsoleCompositeCell *next;
};

struct ConsoleCompositeRow {
    float height;
    uint8_t heightUnit;
    ConsoleCompositeCell *cells;
    ConsoleCompositeRow *next;
};

struct ConsoleWidget {
    char *title;
    bool showTitle;
    bool showGroupOnOff;
    bool isSpacer;
    bool fullWidth;
    uint8_t viewport;
    uint8_t gridColumns;
    uint8_t gridRows;
    ConsoleBinding *bindings;
    ConsoleCompositeRow *compositeRows;
    ConsoleWidget *next;
};

struct ConsoleRecord {
    char id[CONSOLE_ID_BUF];
    char *name;
    bool active;
    uint8_t columns;
    ConsoleWidget *widgets;
    ConsoleRecord *next;
};

enum ConsoleWriteResult {
    ConsoleWriteOk = 0,
    ConsoleWriteNotFound,
    ConsoleWriteFull,
    ConsoleWriteBadId,
    ConsoleWriteBadName,
    ConsoleWriteOutOfMemory,
    ConsoleWriteParseError
};

class ConsoleStore {
public:
    ConsoleStore();
    ~ConsoleStore();

    bool begin(bool persistToLittleFs);
    void requestPersist();
    void persistIfDue();
    int consoleCount() const;
    ConsoleRecord *first();
    const ConsoleRecord *first() const;
    ConsoleRecord *findById(const char *consoleId);
    const ConsoleRecord *findById(const char *consoleId) const;
    ConsoleWriteResult createConsole(const char *name, bool active, ConsoleRecord **createdOut);
    ConsoleWriteResult replaceConsoleFromJson(const char *consoleId, const char *json, bool keepId);
    ConsoleWriteResult deleteConsole(const char *consoleId);
    bool replaceAllFromJson(const String &json);
    String listSummaryJson(class UserStore *userStore) const;
    String listFullJson() const;
    String consoleFullJson(const ConsoleRecord *console) const;
    String mineJson(const class UserRecord *user) const;
    static bool heapAllowsAllocation(size_t bytes);
    static char *duplicateBoundedString(const char *source, size_t maxLength);
    static void freeGaugeRanges(ConsoleGaugeRange *&head);
    static void freeBindingList(ConsoleBinding *&head);
    static void freeCompositeCellList(ConsoleCompositeCell *&head);
    static void freeCompositeRowList(ConsoleCompositeRow *&head);
    static void freeWidgetList(ConsoleWidget *&head);
    static void freeConsole(ConsoleRecord *console);
    static void freeConsoleList(ConsoleRecord *&head);

private:
    ConsoleRecord *head;
    int storedCount;
    bool persistEnabled;
    bool persistPending;

    bool loadFromFile();
    bool persistNow();
    bool saveToFile() const;
    void clearAll();
    ConsoleWriteResult parseOneConsole(const char *objectJson, ConsoleRecord **outConsole, bool requireId);
    static bool parseWidgets(const char *objectJson, ConsoleWidget **outWidgets);
    static bool parseBindings(const char *widgetJson, ConsoleBinding **outBindings);
    static bool parseOneBindingObject(const char *bindingObjectJson, ConsoleBinding **outBinding);
    static bool parseCompositeRows(const char *widgetJson, ConsoleCompositeRow **outRows);
    static bool parseGaugeConfig(const char *bindingJson, ConsoleBinding *binding);
    static void appendConsoleFull(String &json, const ConsoleRecord *console);
    static void appendOneBindingJson(String &json, const ConsoleBinding *binding);
    static void appendCompositeRowsJson(String &json, const ConsoleCompositeRow *rows);
    static void appendGaugeJson(String &json, const ConsoleBinding *binding);
    static void makeConsoleId(char *outId);
    static const char *findJsonObjectEnd(const char *objectStart);
    static uint8_t parseViewportId(const char *text);
    static const char *viewportJsonId(uint8_t viewport);
    static uint8_t parseSizeUnitId(const char *text);
    static const char *sizeUnitJsonId(uint8_t sizeUnit);
    static uint8_t parseCellContentId(const char *text);
    static const char *cellContentJsonId(uint8_t content);
    static uint8_t parseViewStyleId(const char *text);
    static const char *viewStyleJsonId(uint8_t viewStyle);
    static uint8_t parseTitlePositionId(const char *text);
    static const char *titlePositionJsonId(uint8_t titlePosition);
    static uint8_t parseIconAlignId(const char *text);
    static const char *iconAlignJsonId(uint8_t iconAlign);
    static uint8_t parseAlignVId(const char *text);
    static const char *alignVJsonId(uint8_t alignV);
    static uint8_t parseGaugeTypeId(const char *text);
    static const char *gaugeTypeJsonId(uint8_t gaugeType);
    static void appendBindingLayoutJson(String &json, const ConsoleBinding *binding);
};
