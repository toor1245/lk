#pragma once

#include <lk/list.h>
#include <uefi/types.h>

typedef struct {
  struct list_node link;
  struct list_node protocols;
} EfiHandleInternal;

typedef struct {
  struct list_node link;
  EfiGuid guid;
  void *interface;
  struct list_node open_intfs;
} EfiProtocolInterface;
