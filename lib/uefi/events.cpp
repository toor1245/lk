/*
 * Copyright (C) 2025 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "events.h"

#include <kernel/event.h>
#include <kernel/mutex.h>
#include <kernel/thread.h>
#include <lk/err.h>
#include <lk/list.h>
#include <lk/trace.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uefi/types.h>

#define LOCAL_TRACE 0

namespace {

lk_time_t backoff_wait_time = 1;

Mutex event_list_mutex;

struct list_node pending_events = LIST_INITIAL_VALUE(pending_events);

bool invoke_callback(EfiEventImpl *ev) {
  DEBUG_ASSERT(ev->ready());
  DEBUG_ASSERT(ev->creator_thread == get_current_thread());
  if (!ev->callback_called && ev->notify_fn != nullptr) {
    // TODO use atomic compare_exchange to set callback_called
    // to true, so that we don't call the callback multiple times.
    ev->callback_called = true;
    LTRACEF("Triggering event %p callback %p ctx %p on thread %s\n", ev,
            ev->notify_fn, ev->notify_ctx, get_current_thread()->name);
    backoff_wait_time = 0;
    ev->notify_fn(ev, ev->notify_ctx);
    event_unsignal(&ev->ev);
    ev->callback_called = false;
    return true;
  }
  return false;
}

void delete_if_in_list(EfiEventImpl *event, AutoLock *al) {
  if (event->node.prev != nullptr && event->node.next != nullptr) {
    list_delete(&event->node);
  }
}

void process_pending_events() {
  AutoLock al{&event_list_mutex};
  EfiEventImpl *ev = nullptr;
  EfiEventImpl *tmp_ev = nullptr;

  struct list_node completed_events = LIST_INITIAL_VALUE(completed_events);
  list_for_every_entry_safe(&pending_events, ev, tmp_ev, EfiEventImpl, node) {
    if (ev->ready() && (ev->type & EFI_EVENT_TYPE_NOTIFY_SIGNAL) &&
        ev->notify_fn != nullptr && !ev->callback_called) {
      delete_if_in_list(ev, &al);
      list_add_tail(&completed_events, &ev->node);
    }
  }
  al.release();
  list_for_every_entry_safe(&completed_events, ev, tmp_ev, EfiEventImpl, node) {
    list_delete(&ev->node);
    invoke_callback(ev);
    AutoLock al2{&event_list_mutex};
    list_add_tail(&pending_events, &ev->node);
  }
}

}  // namespace

EfiStatus wait_for_event(size_t num_events, EfiEvent *event, size_t *index) {
  LTRACEF("waiting for %zu events\n", num_events);
  if (num_events == 0 || event == nullptr || index == nullptr) {
    return EFI_STATUS_INVALID_PARAMETER;
  }
  for (size_t i = 0; i < num_events; i++) {
    EfiEventImpl *ev = reinterpret_cast<EfiEventImpl *>(event[i]);
    // NOTIFY_SIGNAL events may not be waited on.
    if (ev->type & EFI_EVENT_TYPE_NOTIFY_SIGNAL) {
      return EFI_STATUS_INVALID_PARAMETER;
    }
  }
  while (true) {
    for (size_t i = 0; i < num_events; i++) {
      EfiEventImpl *ev = reinterpret_cast<EfiEventImpl *>(event[i]);
      if (!ev->ready() && (ev->type & EFI_EVENT_TYPE_NOTIFY_WAIT)) {
        ev->notify_fn(ev, ev->notify_ctx);
      }
      if (ev->ready()) {
        event_unsignal(&ev->ev);
        *index = i;
        return EFI_STATUS_SUCCESS;
      }
    }
    EfiEventImpl *ev = reinterpret_cast<EfiEventImpl *>(event[0]);
    event_wait_timeout(&ev->ev, 10);
  }
  return EFI_STATUS_NOT_READY;
}

EfiStatus signal_event(EfiEvent e) {
  auto event = reinterpret_cast<EfiEventImpl *>(e);
  LTRACEF("%s(type=0x%x, ready=%d)\n", __FUNCTION__, event->type,
          event->ready());
  // This function can be called from interrupt context. In interrupt context,
  // we can't switch thread context, so any blocking APIs such as malloc/free
  // mutexes, etc. are not allowed.
  if (event->ready()) {
    LTRACEF("Event %p already signaled\n", event);
    return EFI_STATUS_SUCCESS;
  }
  event_signal(&event->ev, !arch_ints_disabled());
  if ((event->type & EFI_EVENT_TYPE_NOTIFY_SIGNAL) && event->notify_fn != nullptr) {
    // If this event is signaled on a different thread,  defer
    // calling callbacks until the next check_event call. As UEFI apps
    // are single threaded, we don't want to call event callbacks from another
    // thread
    if (event->creator_thread != get_current_thread() || arch_ints_disabled()) {
      LTRACEF(
          "Event %p of type 0x%x is signaled from thread %s, defer notify_fn "
          "because event is created on another thread %s or interrupt is "
          "disabled\n",
          event, event->type, get_current_thread()->name,
          event->creator_thread->name);
      return EFI_STATUS_SUCCESS;
    }
    invoke_callback(event);
  }
  return EFI_STATUS_SUCCESS;
}

EfiStatus create_event(EfiEventType type, EfiTpl notify_tpl,
                       EfiEventNotify notify_fn, void *notify_ctx,
                       EfiEvent *event) {
  process_pending_events();
  if (type == EFI_EVENT_TYPE_SIGNAL_EXIT_BOOT_SERVICES ||
      type == EFI_EVENT_TYPE_SIGNAL_VIRTUAL_ADDRESS_CHANGE) {
    printf(
        "Creating SIGNAL_EXIT_BOOT_SERVICES or SIGNAL_VIRTUAL_ADDRESS_CHANGE "
        "event is not supported yet 0x%x\n",
        type);
    return EFI_STATUS_UNSUPPORTED;
  }
  if ((type & EFI_EVENT_TYPE_NOTIFY_WAIT) &&
      ((type & EFI_EVENT_TYPE_NOTIFY_SIGNAL) || notify_fn == nullptr)) {
    return EFI_STATUS_INVALID_PARAMETER;
  }
  auto ev = reinterpret_cast<EfiEventImpl *>(malloc(sizeof(EfiEventImpl)));
  memset(ev, 0, sizeof(EfiEventImpl));
  ev->type = type;
  event_init(&ev->ev, false, 0);
  timer_initialize(&ev->timer);
  ev->notify_ctx = notify_ctx;
  ev->notify_fn = notify_fn;
  ev->creator_thread = get_current_thread();
  AutoLock al{&event_list_mutex};
  list_add_tail(&pending_events, &ev->node);

  LTRACEF("Created event 0x%x callback %p %p on thread %s\n", type, notify_fn,
          notify_ctx, get_current_thread()->name);
  *event = ev;
  return EFI_STATUS_SUCCESS;
}

EfiStatus check_event(EfiEvent e) {
  auto event = reinterpret_cast<EfiEventImpl *>(e);
  // Events can get signaled from interrupt context, in which we don't
  // call the callback. check_event is definitely going to be called
  // from UEFI app thread, this is a great time to handle any events
  // that are signaled, but not yet had their callbacks called.
  process_pending_events();
  if (event == nullptr) {
    // Some UEFI applications repeadtely call check_event(NULL) as a way to poll
    // for completed events. To avoid busy waiting, implement an exponential
    // backoff strategy if check_event is called repeatdely AND no events in the
    // system are completed.

    if (backoff_wait_time == 0) {
      thread_yield();
      backoff_wait_time++;
    } else {
      thread_sleep(backoff_wait_time);
      if (backoff_wait_time < 500) {
        backoff_wait_time <<= 1;
      }
    }
    return EFI_STATUS_INVALID_PARAMETER;
  }
  if (!event->ready() && (event->type & EFI_EVENT_TYPE_NOTIFY_WAIT)) {
    event->notify_fn(event, event->notify_ctx);
  }
  if (event->ready()) {
    if (event->type & EFI_EVENT_TYPE_NOTIFY_SIGNAL) {
      invoke_callback(event);
    }
    event_unsignal(&event->ev);
    return EFI_STATUS_SUCCESS;
  }
  return EFI_STATUS_NOT_READY;
}

EfiStatus close_event(EfiEvent e) {
  auto event = reinterpret_cast<EfiEventImpl *>(e);
  timer_cancel(&event->timer);
  AutoLock al{&event_list_mutex};
  delete_if_in_list(event, &al);
  al.release();
  event_destroy(&event->ev);
  free(event);
  return EFI_STATUS_SUCCESS;
}

void close_events_in_range(const void *start, size_t size) {
  const auto begin = reinterpret_cast<uintptr_t>(start);
  const auto end = begin + size;

  for (;;) {
    EfiEventImpl *victim = nullptr;
    {
      AutoLock al{&event_list_mutex};
      EfiEventImpl *ev = nullptr;
      list_for_every_entry(&pending_events, ev, EfiEventImpl, node) {
        const auto fn = reinterpret_cast<uintptr_t>(ev->notify_fn);
        if (fn >= begin && fn < end) {
          victim = ev;
          break;
        }
      }
    }
    if (victim == nullptr) {
      return;
    }
    LTRACEF("Closing image owned event %p callback %p\n", victim,
            victim->notify_fn);
    // close_event takes event_list_mutex itself, so it must not be held here.
    close_event(victim);
  }
}

namespace {
enum handler_return timer_expired(timer_t *t, lk_time_t now, void *arg) {
  // Runs in interrupt context; signal_event is IRQ-safe.
  signal_event(static_cast<EfiEventImpl *>(arg));
  return INT_RESCHEDULE;
}
}  // namespace

EfiStatus set_timer(EfiEvent e, EfiTimerDelay type, uint64_t trigger_time) {
  auto event = reinterpret_cast<EfiEventImpl *>(e);
  if (event == nullptr || !(event->type & EFI_EVENT_TYPE_TIMER)) {
    return EFI_STATUS_INVALID_PARAMETER;
  }
  timer_cancel(&event->timer);
  if (type == EFI_TIMER_DELAY_TIMER_CANCEL) {
    return EFI_STATUS_SUCCESS;
  }
  // trigger_time is in 100 ns units.
  lk_time_t ms = static_cast<lk_time_t>((trigger_time + 9999) / 10000);
  if (ms == 0) {
    ms = 1;
  }
  if (type == EFI_TIMER_DELAY_TIMER_PERIODIC) {
    timer_set_periodic(&event->timer, ms, timer_expired, event);
  } else if (type == EFI_TIMER_DELAY_TIMER_RELATIVE) {
    timer_set_oneshot(&event->timer, ms, timer_expired, event);
  } else {
    return EFI_STATUS_INVALID_PARAMETER;
  }
  return EFI_STATUS_SUCCESS;
}
