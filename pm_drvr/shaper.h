#ifndef __LINUX_SHAPER_H
#define __LINUX_SHAPER_H

#ifdef __KERNEL__

#define SHAPER_QLEN  10
/*
 *  This is a bit speed dependant (read it shouldnt be a constant!)
 *
 *  5 is about right for 28.8 upwards. Below that double for every
 *  halving of speed or so. - ie about 20 for 9600 baud.
 */
#define SHAPER_LATENCY  (5*HZ)
#define SHAPER_MAXSLIP  2
#define SHAPER_BURST  (HZ/50)    /* Good for >128K then */

struct shaper
{
  struct sk_buff_head sendq;
  __u32 bytespertick;
  __u32 shapelatency;
  __u32 shapeclock;
  __u32 recovery;    /* Time we can next clock a packet out on
           an empty queue */
  char locked;
  struct device *dev;
  int  (*hard_start_xmit) (struct sk_buff *skb,
    struct device *dev);
  int  (*hard_header) (struct sk_buff *skb,
    struct device *dev,
    WORD type,
    void *daddr,
    void *saddr,
    unsigned len);
  int  (*rebuild_header)(struct sk_buff *skb);
  int (*hard_header_cache)(struct dst_entry *dst, struct dst_entry *neigh,
    struct hh_cache *hh);
  void (*header_cache_update)(struct hh_cache *hh, struct device *dev, BYTE *  haddr);
  struct net_device_stats* (*get_stats)(struct device *dev);
  struct wait_queue *wait_queue;
  struct timer_list timer;
};

#endif

#define SHAPER_SET_DEV    0x0001
#define SHAPER_SET_SPEED  0x0002

struct shaperconf
{
  __u16  ss_cmd;
  union
  {
    char   ssu_name[14];
    __u32  ssu_speed;
  } ss_u;
#define ss_speed ss_u.ssu_speed
#define ss_name ss_u.ssu_name
};

#endif
