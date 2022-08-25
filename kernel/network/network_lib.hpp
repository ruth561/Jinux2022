#pragma once 

#include <cstdint>

// host to network short
uint16_t htons(uint16_t host_short);

// network to host short
uint16_t ntohs(uint16_t network_short);

// host to network long
uint32_t htonl(uint32_t host_long);

// network to host long
uint32_t ntohl(uint32_t network_long);

