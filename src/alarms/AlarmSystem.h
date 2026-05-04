#ifndef ALARMSYSTEM_H
#define ALARMSYSTEM_H

#include <Arduino.h>

#define MAX_ALARMS    8
#define MAX_TIMERS    8
#define ALARM_NAME_LEN 24

struct AlarmEntry {
    char     name[ALARM_NAME_LEN];
    uint8_t  hour;
    uint8_t  minute;
    bool     enabled;
    bool     active;           // slot occupied
    bool     triggered_today;  // prevents re-triggering in the same minute
};

struct TimerEntry {
    char     name[ALARM_NAME_LEN];
    uint32_t duration_seconds;
    uint32_t start_unix_time;  // unix timestamp when started (0 = not started)
    bool     running;
    bool     active;           // slot occupied
    uint16_t linked_item_id;   // shop item whose countdown this tracks (0 = none)
};

#endif // ALARMSYSTEM_H
