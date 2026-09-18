#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void init_cosmos_vpn();
void activate_cosmos_vpn(const char* server_ip);
void deactivate_cosmos_vpn();
int is_cosmos_vpn_active();

#ifdef __cplusplus
}
#endif
