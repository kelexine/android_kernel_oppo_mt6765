/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __NET_NEXTHOP_H
#define __NET_NEXTHOP_H

#include <linux/rtnetlink.h>
#include <net/netlink.h>

static inline int rtnh_ok(const struct rtnexthop *rtnh, int remaining)
{
	return remaining >= (int)sizeof(*rtnh) &&
	       rtnh->rtnh_len >= sizeof(*rtnh) &&
	       rtnh->rtnh_len <= remaining;
}

static inline struct rtnexthop *rtnh_next(const struct rtnexthop *rtnh,
                                         int *remaining)
{
	int totlen = NLA_ALIGN(rtnh->rtnh_len);

	*remaining -= totlen;
	return (struct rtnexthop *) ((char *) rtnh + totlen);
}

static inline struct nlattr *rtnh_attrs(const struct rtnexthop *rtnh)
{
	return (struct nlattr *) ((char *) rtnh + NLA_ALIGN(sizeof(*rtnh)));
}

static inline int rtnh_attrlen(const struct rtnexthop *rtnh)
{
	return rtnh->rtnh_len - NLA_ALIGN(sizeof(*rtnh));
}


static inline unsigned int fib_info_num_path(const struct fib_info *fi)
{
	return fi->fib_nhs;
}

static inline struct fib_nh_common *fib_info_nhc(struct fib_info *fi, int nhsel)
{
	return &fi->fib_nh[nhsel].nh_common;
}

static inline struct fib_nh *fib_info_nh(struct fib_info *fi, int nhsel)
{
	return &fi->fib_nh[nhsel];
}
#endif
