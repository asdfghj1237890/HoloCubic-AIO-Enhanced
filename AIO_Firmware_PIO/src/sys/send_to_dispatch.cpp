#include "send_to_dispatch.h"
#include "app_controller.h"  // for full EVENT_OBJ + EVENT_LIST_MAX_LENGTH
#include <string.h>

int send_to_dispatch(APP_OBJ *fromApp, APP_OBJ *toApp,
                     const char *from, const char *to,
                     APP_MESSAGE_TYPE type, void *message,
                     void *ext_info,
                     std::list<EVENT_OBJ> *eventList)
{
    if (type <= APP_MESSAGE_MQTT_DATA)
    {
        if (eventList->size() >= EVENT_LIST_MAX_LENGTH) {
            return 1;
        }
        // retryMaxNum=5: ~20 s of retries (matches the original
        // AppController::send_to literal).
        EVENT_OBJ new_event = {fromApp, type, message, 5, 0, 0};
        eventList->push_back(new_event);
        return 0;
    }

    if (NULL != toApp) {
        if (NULL != toApp->message_handle) {
            toApp->message_handle(from, to, type, message, ext_info);
        }
        return 0;
    }

    // toApp not found; tell the caller so it can decide whether this
    // is a CTRL_NAME message bound for deal_config.
    return 2;
}
