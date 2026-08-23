#ifndef _OPLUS_KERNEL2USER_H
#define _OPLUS_KERNEL2USER_H

#include <net/sock.h>

/*NLMSG_MIN_TYPE is 0x10,so user to kernel we start at 0x20,kernel to user we start at 0x30*/
enum{
    OPLUS_FOREGROUND_ANDROID_UID		= 0x20,
    OPLUS_MPTCP_UID					= 0x21,
    OPLUS_SEND_TCP_RETRANSMIT		= 0x30,
    OPLUS_SEND_NETWORK_SCORE			= 0x31,
};

#define MAX_PARA_LEN 100
#define MAX_MPTCP_APP_LEN 100
#define MAX_LINK_LEN 32

struct general_oplus_info {
    u32 para_type;
    u32 para_one;
    u32 para_two;
    u32 para_three;
    char para_array[MAX_PARA_LEN];
};

static inline int oplus_kernel_send_to_user(int msg_type, char *payload, int payload_len) { return 0; }
static inline void oplus_handle_retransmit(const struct sock *sk, int type) {}
static inline uid_t get_uid_from_sock(const struct sock *sk) { return 0; }
static inline int get_link_index_from_sock(const struct sock *sk) { return 0; }

#endif
