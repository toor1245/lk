#pragma once

#include <uefi/protocols/simple_network_protocol.h>
#include <uefi/types.h>

EfiSimpleNetworkProtocol* open_simple_network_protocol();