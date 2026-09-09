#include <lwip/def.h>
#include <lwip/netif.h>
#include <lwip/prot/dhcp.h>

extern "C" void __real_dhcp_append_extra_opts(struct netif*, uint8_t, struct dhcp_msg*, uint16_t*);

extern "C" void __wrap_dhcp_append_extra_opts(struct netif* interface, uint8_t state,
                                              struct dhcp_msg* message, uint16_t* optionsLength) {
  __real_dhcp_append_extra_opts(interface, state, message, optionsLength);
  // RFC 2131 section 4.1: request broadcast replies until a client IPv4 address is assigned.
  if (message->ciaddr.addr == 0) message->flags |= PP_HTONS(0x8000);
}
