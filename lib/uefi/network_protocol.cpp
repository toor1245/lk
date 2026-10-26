#include "network_protocol.h"
#include "efi_internal.h"
#include "events.h"

#include <endian.h>
#include <kernel/event.h>
#include <kernel/spinlock.h>
#include <lib/minip.h>
#include <lib/pktbuf.h>
#include <lk/err.h>
#include <lk/trace.h>
#include <stdlib.h>
#include <string.h>

#define LOCAL_TRACE 0

#define SNP_MAX_OBJS 4
#define SNP_ETH_HDR_SIZE 14
#define SNP_MTU 1500
#define SNP_MAX_FRAME (SNP_ETH_HDR_SIZE + SNP_MTU)
#define SNP_RX_RING_SIZE 16

struct SnpRxSlot {
  uint8_t data[SNP_MAX_FRAME];
  uint32_t len;
};

typedef struct EfiNetObject {
  EfiHandleInternal handle;
  EfiSimpleNetworkProtocol snp;
  EfiSimpleNetworkMode net_mode;

  spin_lock_t lock;
  SnpRxSlot *rx_ring;
  uint32_t rx_head;
  uint32_t rx_tail;
  uint32_t rx_count;

  uint32_t int_status;
  void *tx_done_buf;
} EfiNetObject;

static const uint8_t snp_broadcast_mac[6] = {0xff, 0xff, 0xff,
                                             0xff, 0xff, 0xff};

static EfiNetObject *net_objs[SNP_MAX_OBJS];

static EfiNetObject *snp_find_object(EfiSimpleNetworkProtocol *snp) {
  for (size_t i = 0; i < SNP_MAX_OBJS; i++) {
    if (net_objs[i] && &net_objs[i]->snp == snp) {
      return net_objs[i];
    }
  }

  return nullptr;
}

static bool snp_mac_in_mcast_filter(EfiNetObject *obj, const uint8_t *dst) {
  for (uint32_t i = 0; i < obj->net_mode.m_cast_filter_count; i++) {
    if (memcmp(dst, obj->net_mode.m_cast_filter[i].addr, 6) == 0) {
      return true;
    }
  }

  return false;
}

static bool snp_accept_frame(EfiNetObject *obj, const uint8_t *dst) {
  const uint32_t filters = obj->net_mode.receive_filter_setting;
  const bool multicast = (dst[0] & 0x01) != 0;

  if (filters & EFI_SIMPLE_NETWORK_RECEIVE_PROMISCUOUS) {
    return true;
  }
  if ((filters & EFI_SIMPLE_NETWORK_RECEIVE_BROADCAST) &&
      memcmp(dst, snp_broadcast_mac, 6) == 0) {
    return true;
  }
  if ((filters & EFI_SIMPLE_NETWORK_RECEIVE_PROMISCUOUS_MULTICAST) &&
      multicast) {
    return true;
  }
  if ((filters & EFI_SIMPLE_NETWORK_RECEIVE_MULTICAST) && multicast &&
      snp_mac_in_mcast_filter(obj, dst)) {
    return true;
  }
  if ((filters & EFI_SIMPLE_NETWORK_RECEIVE_UNICAST) &&
      memcmp(dst, obj->net_mode.current_address.addr, 6) == 0) {
    return true;
  }

  return false;
}

static bool snp_raw_rx_hook(void *arg, pktbuf_t *p) {
  auto *obj = static_cast<EfiNetObject *>(arg);

  LTRACEF("hook: dlen %u, data %p, phys 0x%lx\n", p->dlen, p->data,
          (unsigned long)pktbuf_data_phys(p));

  if (p->dlen < SNP_ETH_HDR_SIZE || p->dlen > SNP_MAX_FRAME) {
    return false;
  }

  LTRACEF("frame %u bytes, dst %02x:%02x:%02x:%02x:%02x:%02x, filter 0x%x, "
          "accept %d, queued %u\n",
          p->dlen, p->data[0], p->data[1], p->data[2], p->data[3], p->data[4],
          p->data[5], obj->net_mode.receive_filter_setting,
          snp_accept_frame(obj, p->data), obj->rx_count);

  if (!snp_accept_frame(obj, p->data)) {
    return false;
  }

  arch_interrupt_saved_state_t state = spin_lock_irqsave(&obj->lock);

  bool queued = false;
  if (obj->rx_count < SNP_RX_RING_SIZE) {
    SnpRxSlot *slot = &obj->rx_ring[obj->rx_tail];
    memcpy(slot->data, p->data, p->dlen);  // must copy: driver requeues on return
    slot->len = p->dlen;
    obj->rx_tail = (obj->rx_tail + 1) % SNP_RX_RING_SIZE;
    obj->rx_count++;
    obj->int_status |= EFI_SIMPLE_NETWORK_RECEIVE_INTERRUPT;
    queued = true;
  }

  spin_unlock_irqrestore(&obj->lock, state);

  if (queued) {
    signal_event(obj->snp.wait_for_packet);
  }

  return true;  // swallow the frame; minip won't see it
}

static void snp_wait_for_packet_notify(EfiEvent, void *) {}

static void snp_reset_queues(EfiNetObject *obj) {
  arch_interrupt_saved_state_t state = spin_lock_irqsave(&obj->lock);
  obj->rx_head = 0;
  obj->rx_tail = 0;
  obj->rx_count = 0;
  obj->int_status = 0;
  obj->tx_done_buf = nullptr;
  spin_unlock_irqrestore(&obj->lock, state);

  event_unsignal(&static_cast<EfiEventImpl *>(obj->snp.wait_for_packet)->ev);
}

EfiStatus snp_start(EfiSimpleNetworkProtocol *self) {
  if (self == nullptr) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  EfiNetObject *net_obj = snp_find_object(self);
  if (!net_obj) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  if (self->mode->state != EFI_SIMPLE_NETWORK_STATE_STOPPED) {
    return EFI_STATUS_ALREADY_STARTED;
  }

  self->mode->receive_filter_setting =
      EFI_SIMPLE_NETWORK_RECEIVE_UNICAST | EFI_SIMPLE_NETWORK_RECEIVE_BROADCAST;
  self->mode->state = EFI_SIMPLE_NETWORK_STATE_STARTED;

  return EFI_STATUS_SUCCESS;
}

EfiStatus snp_stop(EfiSimpleNetworkProtocol *self) {
  if (self == nullptr) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  EfiNetObject *net_obj = snp_find_object(self);
  if (!net_obj) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  if (self->mode->state == EFI_SIMPLE_NETWORK_STATE_STOPPED) {
    return EFI_STATUS_NOT_STARTED;
  }
  if (self->mode->state == EFI_SIMPLE_NETWORK_STATE_INITIALIZED) {
    self->shutdown(self);
  }

  self->mode->state = EFI_SIMPLE_NETWORK_STATE_STOPPED;

  return EFI_STATUS_SUCCESS;
}

EfiStatus snp_initialize(EfiSimpleNetworkProtocol *self,
                         size_t extra_rx_buffer_size,
                         size_t extra_tx_buffer_size) {
  if (self == nullptr) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  EfiNetObject *net_obj = snp_find_object(self);
  if (!net_obj) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  if (extra_rx_buffer_size != 0 || extra_tx_buffer_size != 0) {
    return EFI_STATUS_UNSUPPORTED;
  }

  switch (self->mode->state) {
    case EFI_SIMPLE_NETWORK_STATE_STARTED:
    case EFI_SIMPLE_NETWORK_STATE_INITIALIZED:
      break;
    default:
      return EFI_STATUS_NOT_STARTED;
  }

  if (net_obj->rx_ring == nullptr) {
    net_obj->rx_ring =
        static_cast<SnpRxSlot *>(calloc(SNP_RX_RING_SIZE, sizeof(SnpRxSlot)));
    if (net_obj->rx_ring == nullptr) {
      return EFI_STATUS_OUT_OF_RESOURCES;
    }
  }

  snp_reset_queues(net_obj);

  minip_set_raw_rx(snp_raw_rx_hook, net_obj);

  self->mode->state = EFI_SIMPLE_NETWORK_STATE_INITIALIZED;

  LTRACEF("snp %p initialized, mac %02x:%02x:%02x:%02x:%02x:%02x\n", self,
          self->mode->current_address.addr[0],
          self->mode->current_address.addr[1],
          self->mode->current_address.addr[2],
          self->mode->current_address.addr[3],
          self->mode->current_address.addr[4],
          self->mode->current_address.addr[5]);

  return EFI_STATUS_SUCCESS;
}

EfiStatus snp_shutdown(EfiSimpleNetworkProtocol *self) {
  if (self == nullptr) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  EfiNetObject *net_obj = snp_find_object(self);
  if (!net_obj) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  if (self->mode->state != EFI_SIMPLE_NETWORK_STATE_INITIALIZED) {
    return EFI_STATUS_NOT_STARTED;
  }

  minip_set_raw_rx(nullptr, nullptr);
  snp_reset_queues(net_obj);

  self->mode->state = EFI_SIMPLE_NETWORK_STATE_STARTED;

  return EFI_STATUS_SUCCESS;
}

EfiStatus snp_reset(EfiSimpleNetworkProtocol *self,
                    bool extended_verification) {
  if (self == nullptr) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  EfiNetObject *net_obj = snp_find_object(self);
  if (!net_obj) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  if (self->mode->state != EFI_SIMPLE_NETWORK_STATE_INITIALIZED) {
    return EFI_STATUS_NOT_STARTED;
  }

  snp_reset_queues(net_obj);

  return EFI_STATUS_SUCCESS;
}

EfiStatus snp_receive_filters(EfiSimpleNetworkProtocol *self, uint32_t enable,
                              uint32_t disable, bool reset_mcast_filter,
                              size_t mcast_filter_count,
                              EfiMacAddress *mcast_filter) {
  if (self == nullptr) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  EfiNetObject *net_obj = snp_find_object(self);
  if (!net_obj) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  if (self->mode->state != EFI_SIMPLE_NETWORK_STATE_INITIALIZED) {
    return EFI_STATUS_NOT_STARTED;
  }

  const uint32_t mask = self->mode->receive_filter_mask;
  if ((enable & ~mask) != 0 || (disable & ~mask) != 0) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  if (mcast_filter_count > 0) {
    if (mcast_filter == nullptr ||
        mcast_filter_count > self->mode->max_m_cast_filter_count ||
        (enable & EFI_SIMPLE_NETWORK_RECEIVE_MULTICAST) == 0) {
      return EFI_STATUS_INVALID_PARAMETER;
    }
    for (size_t i = 0; i < mcast_filter_count; i++) {
      if ((mcast_filter[i].addr[0] & 0x01) == 0) {
        return EFI_STATUS_INVALID_PARAMETER;
      }
    }
  }

  arch_interrupt_saved_state_t state = spin_lock_irqsave(&net_obj->lock);

  if (reset_mcast_filter) {
    self->mode->m_cast_filter_count = 0;
    memset(self->mode->m_cast_filter, 0, sizeof(self->mode->m_cast_filter));
  }
  if (mcast_filter_count > 0) {
    for (size_t i = 0; i < mcast_filter_count; i++) {
      self->mode->m_cast_filter[i] = mcast_filter[i];
    }
    self->mode->m_cast_filter_count = mcast_filter_count;
  }

  self->mode->receive_filter_setting =
      (self->mode->receive_filter_setting | enable) & ~disable;

  spin_unlock_irqrestore(&net_obj->lock, state);

  LTRACEF("snp %p receive filter now 0x%x, %u mcast filter(s)\n", self,
          self->mode->receive_filter_setting, self->mode->m_cast_filter_count);

  return EFI_STATUS_SUCCESS;
}

EfiStatus snp_station_address(EfiSimpleNetworkProtocol *self, bool reset,
                              EfiMacAddress *new_addr) {
  return EFI_STATUS_UNSUPPORTED;
}

EfiStatus snp_statistics(EfiSimpleNetworkProtocol *self, bool reset,
                         size_t *stats_size,
                         EfiNetworkStatistics *stats_table) {
  return EFI_STATUS_UNSUPPORTED;
}

EfiStatus snp_m_cast_ip_to_mac(EfiSimpleNetworkProtocol *self, bool ipv6,
                               EfiIpAddr *ip, EfiMacAddress *mac) {
  if (self == nullptr || ip == nullptr || mac == nullptr) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  if (snp_find_object(self) == nullptr) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  if (self->mode->state == EFI_SIMPLE_NETWORK_STATE_STOPPED) {
    return EFI_STATUS_NOT_STARTED;
  }

  memset(mac, 0, sizeof(*mac));

  if (ipv6) {
    // RFC 2464: 33:33 followed by the low 32 bits of the address.
    if (ip->v6.addr[0] != 0xff) {
      return EFI_STATUS_INVALID_PARAMETER;
    }
    mac->addr[0] = 0x33;
    mac->addr[1] = 0x33;
    memcpy(&mac->addr[2], &ip->v6.addr[12], 4);
  } else {
    // RFC 1112: 01:00:5e plus the low 23 bits of the address.
    if (ip->v4.addr[0] < 224 || ip->v4.addr[0] > 239) {
      return EFI_STATUS_INVALID_PARAMETER;
    }
    mac->addr[0] = 0x01;
    mac->addr[1] = 0x00;
    mac->addr[2] = 0x5e;
    mac->addr[3] = ip->v4.addr[1] & 0x7f;
    mac->addr[4] = ip->v4.addr[2];
    mac->addr[5] = ip->v4.addr[3];
  }

  return EFI_STATUS_SUCCESS;
}

EfiStatus snp_nv_data(EfiSimpleNetworkProtocol *self, bool read_write,
                      size_t offset, size_t buf_size, void *buf) {
  return EFI_STATUS_UNSUPPORTED;
}

EfiStatus snp_get_status(EfiSimpleNetworkProtocol *self,
                         uint32_t *interrupt_status, void **tx_buf) {
  if (self == nullptr) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  EfiNetObject *net_obj = snp_find_object(self);
  if (!net_obj) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  if (self->mode->state != EFI_SIMPLE_NETWORK_STATE_INITIALIZED) {
    return EFI_STATUS_NOT_STARTED;
  }

  arch_interrupt_saved_state_t state = spin_lock_irqsave(&net_obj->lock);

  if (interrupt_status != nullptr) {
    *interrupt_status = net_obj->int_status;
    net_obj->int_status = 0;
  }
  if (tx_buf != nullptr) {
    *tx_buf = net_obj->tx_done_buf;
    net_obj->tx_done_buf = nullptr;
  }

  spin_unlock_irqrestore(&net_obj->lock, state);

  return EFI_STATUS_SUCCESS;
}

EfiStatus snp_transmit(EfiSimpleNetworkProtocol *self, size_t header_size,
                       size_t buf_size, void *buf, EfiMacAddress *src,
                       EfiMacAddress *dest, uint16_t *protocol) {
  if (self == nullptr || buf == nullptr) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  EfiNetObject *net_obj = snp_find_object(self);
  if (!net_obj) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  if (self->mode->state != EFI_SIMPLE_NETWORK_STATE_INITIALIZED) {
    return EFI_STATUS_NOT_STARTED;
  }

  if (header_size != 0) {
    if (header_size != self->mode->media_header_size || dest == nullptr) {
      return EFI_STATUS_INVALID_PARAMETER;
    }
  }

  if (buf_size < self->mode->media_header_size) {
    return EFI_STATUS_BUFFER_TOO_SMALL;
  }
  if (buf_size > SNP_MAX_FRAME) {
    return EFI_STATUS_BAD_BUFFER_SIZE;
  }

  auto *frame = static_cast<uint8_t *>(buf);
  if (header_size != 0) {
    memcpy(&frame[0], dest->addr, 6);
    memcpy(&frame[6], src ? src->addr : self->mode->current_address.addr, 6);
    const uint16_t type = htons(protocol ? *protocol : 0x0800);
    memcpy(&frame[12], &type, sizeof(type));
  }

  pktbuf_t *p = pktbuf_alloc();
  if (p == nullptr) {
    return EFI_STATUS_OUT_OF_RESOURCES;
  }

  // A full sized frame does not fit behind the default PKTBUF_MAX_HDR reserve.
  pktbuf_reset(p, 0);
  pktbuf_append_data(p, frame, buf_size);

  status_t err = minip_tx_raw(p);
  if (err < 0) {
    // The tx hook takes ownership of the pktbuf unless it never ran.
    if (err == ERR_NOT_CONFIGURED) {
      pktbuf_free(p, true);
    }
    return EFI_STATUS_DEVICE_ERROR;
  }

  arch_interrupt_saved_state_t state = spin_lock_irqsave(&net_obj->lock);
  net_obj->tx_done_buf = buf;
  net_obj->int_status |= EFI_SIMPLE_NETWORK_TRANSMIT_INTERRUPT;
  spin_unlock_irqrestore(&net_obj->lock, state);

  LTRACEF("snp %p transmitted %zu bytes\n", self, buf_size);

  return EFI_STATUS_SUCCESS;
}

EfiStatus snp_receive(EfiSimpleNetworkProtocol *self, size_t *header_size,
                      size_t *buf_size, void *buf, EfiMacAddress *src,
                      EfiMacAddress *dest, uint16_t *protocol) {
  if (self == nullptr || buf == nullptr || buf_size == nullptr) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  EfiNetObject *net_obj = snp_find_object(self);
  if (!net_obj) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  if (self->mode->state != EFI_SIMPLE_NETWORK_STATE_INITIALIZED) {
    return EFI_STATUS_NOT_STARTED;
  }

  arch_interrupt_saved_state_t state = spin_lock_irqsave(&net_obj->lock);

  if (net_obj->rx_count == 0) {
    spin_unlock_irqrestore(&net_obj->lock, state);
    return EFI_STATUS_NOT_READY;
  }

  SnpRxSlot *slot = &net_obj->rx_ring[net_obj->rx_head];
  if (*buf_size < slot->len) {
    *buf_size = slot->len;
    spin_unlock_irqrestore(&net_obj->lock, state);
    return EFI_STATUS_BUFFER_TOO_SMALL;
  }

  const uint32_t len = slot->len;
  memcpy(buf, slot->data, len);

  net_obj->rx_head = (net_obj->rx_head + 1) % SNP_RX_RING_SIZE;
  net_obj->rx_count--;
  const uint32_t remaining = net_obj->rx_count;
  if (remaining == 0) {
    net_obj->int_status &= ~EFI_SIMPLE_NETWORK_RECEIVE_INTERRUPT;
  }

  spin_unlock_irqrestore(&net_obj->lock, state);

  if (remaining == 0) {
    event_unsignal(&static_cast<EfiEventImpl *>(self->wait_for_packet)->ev);
  } else {
    signal_event(self->wait_for_packet);
  }

  auto *frame = static_cast<const uint8_t *>(buf);
  *buf_size = len;
  if (header_size != nullptr) {
    *header_size = self->mode->media_header_size;
  }
  if (dest != nullptr) {
    memset(dest, 0, sizeof(*dest));
    memcpy(dest->addr, &frame[0], 6);
  }
  if (src != nullptr) {
    memset(src, 0, sizeof(*src));
    memcpy(src->addr, &frame[6], 6);
  }
  if (protocol != nullptr) {
    uint16_t type;
    memcpy(&type, &frame[12], sizeof(type));
    *protocol = ntohs(type);
  }

  return EFI_STATUS_SUCCESS;
}

static EfiNetObject *snp_create_object() {
  auto *obj = static_cast<EfiNetObject *>(calloc(1, sizeof(EfiNetObject)));
  if (obj == nullptr) {
    return nullptr;
  }

  obj->lock = SPIN_LOCK_INITIAL_VALUE;

  obj->net_mode.state = EFI_SIMPLE_NETWORK_STATE_STOPPED;
  obj->net_mode.hw_address_size = 6;
  obj->net_mode.media_header_size = SNP_ETH_HDR_SIZE;
  obj->net_mode.max_packet_size = SNP_MTU;
  obj->net_mode.receive_filter_mask =
      EFI_SIMPLE_NETWORK_RECEIVE_UNICAST |
      EFI_SIMPLE_NETWORK_RECEIVE_BROADCAST |
      EFI_SIMPLE_NETWORK_RECEIVE_MULTICAST |
      EFI_SIMPLE_NETWORK_RECEIVE_PROMISCUOUS |
      EFI_SIMPLE_NETWORK_RECEIVE_PROMISCUOUS_MULTICAST;
  obj->net_mode.max_m_cast_filter_count = MAX_MCAST_FILTER_CNT;
  minip_get_macaddr(obj->net_mode.current_address.addr);
  memcpy(obj->net_mode.permanent_address.addr,
         obj->net_mode.current_address.addr, 6);
  memcpy(obj->net_mode.broadcast_address.addr, snp_broadcast_mac, 6);
  obj->net_mode.if_type = 1;
  obj->net_mode.media_present = true;

  obj->snp.revision = EFI_SIMPLE_NETWORK_PROTOCOL_REVISION;
  obj->snp.start = snp_start;
  obj->snp.stop = snp_stop;
  obj->snp.initialize = snp_initialize;
  obj->snp.reset = snp_reset;
  obj->snp.shutdown = snp_shutdown;
  obj->snp.receive_filters = snp_receive_filters;
  obj->snp.station_address = snp_station_address;
  obj->snp.statistics = snp_statistics;
  obj->snp.m_cast_ip_to_mac = snp_m_cast_ip_to_mac;
  obj->snp.nv_data = snp_nv_data;
  obj->snp.get_status = snp_get_status;
  obj->snp.transmit = snp_transmit;
  obj->snp.receive = snp_receive;
  obj->snp.mode = &obj->net_mode;

  if (create_event(EFI_EVENT_TYPE_NOTIFY_WAIT, 0, snp_wait_for_packet_notify,
                   nullptr, &obj->snp.wait_for_packet) != EFI_STATUS_SUCCESS) {
    free(obj);
    return nullptr;
  }

  return obj;
}

EfiSimpleNetworkProtocol *open_simple_network_protocol() {
  if (net_objs[0] != nullptr) {
    return &net_objs[0]->snp;
  }

  EfiNetObject *obj = snp_create_object();
  if (obj == nullptr) {
    return nullptr;
  }

  net_objs[0] = obj;
  return &obj->snp;
}
