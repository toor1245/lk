#ifndef __UNICODE_COLLATION_H__
#define __UNICODE_COLLATION_H__

#include <uefi/protocols/unicode_collation_protocol.h>

const EfiUnicodeCollationProtocol *get_unicode_collation_protocol(void);

#endif