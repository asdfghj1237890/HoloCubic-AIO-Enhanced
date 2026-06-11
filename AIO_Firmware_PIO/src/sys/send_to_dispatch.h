#ifndef SEND_TO_DISPATCH_H
#define SEND_TO_DISPATCH_H

#include "interface.h"
#include <list>

// EVENT_OBJ + EVENT_LIST_MAX_LENGTH live in app_controller.h, but that
// header drags in driver/imu.h + common.h transitively. Forward-declare
// the bits we need so the host-side Unity unit test can compile this
// header against a slim stubs_unit set.
struct EVENT_OBJ;

// Dispatch / queue logic extracted from AppController::send_to so it
// can be unit-tested without standing up the full controller (which
// pulls in FreeRTOS timers + WiFi/network globals).
//
// Behavior contract:
//   - type <= APP_MESSAGE_MQTT_DATA:
//       queue an EVENT_OBJ into *eventList. Returns 1 if the queue
//       is full (size >= EVENT_LIST_MAX_LENGTH), else 0.
//   - type >  APP_MESSAGE_MQTT_DATA:
//       if toApp != nullptr and toApp->message_handle is set, invoke
//       it. Returns 0 either way.
//       if toApp == nullptr, return 2 — the caller (AppController::
//       send_to) decides whether to route to deal_config based on
//       whether `to` matches CTRL_NAME.
//
// fromApp may be nullptr (e.g. controller-originated events).
int send_to_dispatch(APP_OBJ *fromApp, APP_OBJ *toApp,
                     const char *from, const char *to,
                     APP_MESSAGE_TYPE type, void *message,
                     void *ext_info,
                     std::list<EVENT_OBJ> *eventList);

#endif
