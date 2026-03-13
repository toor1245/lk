#include <uefi/protocols/dt_fixup_protocol.h>

#include "uefi_platform.h"

const EfiDtFixupProtocol dtFixupProtocol = {
    .revision = EFI_DT_FIXUP_PROTOCOL_REVISION,
    .fixup = efi_dt_fixup,
};
