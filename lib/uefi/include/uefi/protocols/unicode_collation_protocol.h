#ifndef __UNICODE_COLLATION_PROTOCOL_H__
#define __UNICODE_COLLATION_PROTOCOL_H__

#include <uefi/types.h>

typedef struct {
  uint64_t frequency;
  uint64_t end_value;
} EfiTimestampProperties;

struct EfiUnicodeCollationProtocol {
  uint64_t (*get_timestamp)(void);
  EfiStatus (*get_properties)(EfiTimestampProperties*);
};

#endif  //__UNICODE_COLLATION_PROTOCOL_H__