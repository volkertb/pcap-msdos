/*
 *	Definitions for the 'struct sk_buff' memory handlers.
 *
 *	Authors:
 *		Alan Cox, <gw4pts@gw4pts.ampr.org>
 *		Florian La Roche, <rzsfl@rz.uni-sb.de>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */
 
#ifndef _LINUX_SKBUFF_H
#define _LINUX_SKBUFF_H

#include <linux/config.h>
#include <linux/time.h>

#include <asm/atomic.h>
#include <asm/types.h>

#define CONFIG_SKB_CHECK 0

#define HAVE_ALLOC_SKB		/* For the drivers to know */
#define HAVE_ALIGNABLE_SKB	/* Ditto 8)		   */


#define FREE_READ	1
#define FREE_WRITE	0

#define CHECKSUM_NONE 0
#define CHECKSUM_HW 1
#define CHECKSUM_UNNECESSARY 2

struct sk_buff_head 
{
	struct sk_buff	* next;
	struct sk_buff	* prev;
	__u32		qlen;		/* Must be same length as a pointer
					   for using debugging */
#if CONFIG_SKB_CHECK
	int		magic_debug_cookie;
#endif
};

struct sk_buff 
{
	struct sk_buff	* next;			/* Next buffer in list 				*/
	struct sk_buff	* prev;			/* Previous buffer in list 			*/
	struct sk_buff_head * list;		/* List we are on				*/
#if CONFIG_SKB_CHECK
	int		magic_debug_cookie;
#endif
	struct sock	*sk;			/* Socket we are owned by 			*/
	unsigned long	when;			/* used to compute rtt's			*/
	struct timeval	stamp;			/* Time we arrived				*/
	struct device	*dev;			/* Device we arrived on/are leaving by		*/

	/* Transport layer header */
	union
	{
		struct tcphdr	*th;
		struct udphdr	*uh;
		struct icmphdr	*icmph;
		struct igmphdr	*igmph;
		struct iphdr	*ipiph;
		struct spxhdr	*spxh;
		unsigned char	*raw;
	} h;

	/* Network layer header */
	union
	{
		struct iphdr	*iph;
		struct ipv6hdr	*ipv6h;
		struct arphdr	*arph;
		struct ipxhdr	*ipxh;
		unsigned char	*raw;
	} nh;
  
	/* Link layer header */
	union 
	{	
	  	struct ethhdr	*ethernet;
	  	unsigned char 	*raw;
	} mac;

#if defined(CONFIG_IPV6) || defined (CONFIG_IPV6_MODULE)
	/*
	 *	Generic "neighbour" information
	 */
	struct neighbour *nexthop;
#endif		
	struct  dst_entry *dst;
	char    	cb[32];

	__u32		seq;			/* TCP sequence number				*/
	__u32		end_seq;		/* seq [+ fin] [+ syn] + datalen		*/
	__u32		ack_seq;		/* TCP ack sequence number			*/
  
	unsigned int 	len;			/* Length of actual data			*/
	unsigned int	csum;			/* Checksum 					*/
	volatile char 	acked,			/* Are we acked ?				*/
			used,			/* Are we in use ?				*/
			arp;			/* Has IP/ARP resolution finished		*/
	unsigned char	tries,			/* Times tried					*/
  			inclone,		/* Inline clone					*/
  			priority,
  			pkt_type,		/* Packet class					*/
  			pkt_bridged,		/* Tracker for bridging 			*/
  			ip_summed;		/* Driver fed us an IP checksum			*/
#define PACKET_HOST		0		/* To us					*/
#define PACKET_BROADCAST	1		/* To all					*/
#define PACKET_MULTICAST	2		/* To group					*/
#define PACKET_OTHERHOST	3		/* To someone else 				*/
#define PACKET_NDISC		17		/* Outgoing NDISC packet			*/
	atomic_t	users;			/* User count - see datagram.c,tcp.c 		*/
	unsigned short	protocol;		/* Packet protocol from driver. 		*/
	unsigned int	truesize;		/* Buffer size 					*/

	atomic_t	count;			/* reference count				*/
	struct sk_buff	*data_skb;		/* Link to the actual data skb			*/
	unsigned char	*head;			/* Head of buffer 				*/
	unsigned char	*data;			/* Data head pointer				*/
	unsigned char	*tail;			/* Tail pointer					*/
	unsigned char 	*end;			/* End pointer					*/
	void 		(*destructor)(struct sk_buff *);	/* Destruct function		*/
#define SKB_CLONE_ORIG		1
#define SKB_CLONE_INLINE	2
	
#if defined(CONFIG_SHAPER) || defined(CONFIG_SHAPER_MODULE)
	__u32		shapelatency;		/* Latency on frame */
	__u32		shapeclock;		/* Time it should go out */
	__u32		shapelen;		/* Frame length in clocks */
	__u32		shapestamp;		/* Stamp for shaper    */
	__u16		shapepend;		/* Pending */
#endif
};

#ifdef CONFIG_SKB_LARGE
#define SK_WMEM_MAX	65535
#define SK_RMEM_MAX	65535
#else
#define SK_WMEM_MAX	32767
#define SK_RMEM_MAX	32767
#endif

#if CONFIG_SKB_CHECK
#define SK_FREED_SKB	0x0DE2C0DE
#define SK_GOOD_SKB	0xDEC0DED1
#define SK_HEAD_SKB	0x12231298
#endif

#ifdef __KERNEL__
/*
 *	Handling routines are only of interest to the kernel
 */
#include <linux/malloc.h>

#include <asm/system.h>

#if 0
extern void			print_skb(struct sk_buff *);
#endif
extern void			__kfree_skb(struct sk_buff *skb);
extern void			skb_queue_head_init(struct sk_buff_head *list);
extern void			skb_queue_head(struct sk_buff_head *list,struct sk_buff *buf);
extern void			skb_queue_tail(struct sk_buff_head *list,struct sk_buff *buf);
extern struct sk_buff *		skb_dequeue(struct sk_buff_head *list);
extern void 			skb_insert(struct sk_buff *old,struct sk_buff *newsk);
extern void			skb_append(struct sk_buff *old,struct sk_buff *newsk);
extern void			skb_unlink(struct sk_buff *buf);
extern __u32			skb_queue_len(struct sk_buff_head *list);
extern struct sk_buff *		skb_peek_copy(struct sk_buff_head *list);
extern struct sk_buff *		alloc_skb(unsigned int size, int priority);
extern struct sk_buff *		dev_alloc_skb(unsigned int size);
extern void			kfree_skbmem(struct sk_buff *skb);
extern struct sk_buff *		skb_clone(struct sk_buff *skb, int priority);
extern struct sk_buff *		skb_copy(struct sk_buff *skb, int priority);
extern struct sk_buff *		skb_realloc_headroom(struct sk_buff *skb, int newheadroom);
#define dev_kfree_skb(a, b)	kfree_skb((a), (b))
extern unsigned char *		skb_put(struct sk_buff *skb, unsigned int len);
extern unsigned char *		skb_push(struct sk_buff *skb, unsigned int len);
extern unsigned char *		skb_pull(struct sk_buff *skb, unsigned int len);
extern int			skb_headroom(struct sk_buff *skb);
extern int			skb_tailroom(struct sk_buff *skb);
extern void			skb_reserve(struct sk_buff *skb, unsigned int len);
extern void 			skb_trim(struct sk_buff *skb, unsigned int len);

extern __inline__ int skb_queue_empty(struct sk_buff_head *list)
{
	return (list->next == (struct sk_buff *) list);
}

extern __inline__ void kfree_skb(struct sk_buff *skb, int rw)
{
	if (atomic_dec_and_test(&skb->users))
		__kfree_skb(skb);
}

extern __inline__ int skb_cloned(struct sk_buff *skb)
{
	return (skb->data_skb->count != 1);
}

extern __inline__ int skb_shared(struct sk_buff *skb)
{
	return (skb->users != 1);
}

/*
 *	Copy shared buffers into a new sk_buff. We effectively do COW on
 *	packets to handle cases where we have a local reader and forward
 *	and a couple of other messy ones. The normal one is tcpdumping
 *	a packet thats being forwarded.
 */
 
extern __inline__ struct sk_buff *skb_unshare(struct sk_buff *skb, int pri, int dir)
{
	struct sk_buff *nskb;
	if(!skb_cloned(skb))
		return skb;
	nskb=skb_copy(skb, pri);
	kfree_skb(skb, dir);	/* Free our shared copy */
	return nskb;
}

/*
 *	Peek an sk_buff. Unlike most other operations you _MUST_
 *	be careful with this one. A peek leaves the buffer on the
 *	list and someone else may run off with it. For an interrupt
 *	type system cli() peek the buffer copy the data and sti();
 */
 
extern __inline__ struct sk_buff *skb_peek(struct sk_buff_head *list_)
{
	struct sk_buff *list = ((struct sk_buff *)list_)->next;
	if (list == (struct sk_buff *)list_)
		list = NULL;
	return list;
}

/*
 *	Return the length of an sk_buff queue
 */
 
extern __inline__ __u32 skb_queue_len(struct sk_buff_head *list_)
{
	return(list_->qlen);
}

#if CONFIG_SKB_CHECK
extern int 			skb_check(struct sk_buff *skb,int,int, char *);
#define IS_SKB(skb)		skb_check((skb), 0, __LINE__,__FILE__)
#define IS_SKB_HEAD(skb)	skb_check((skb), 1, __LINE__,__FILE__)
#else
#define IS_SKB(skb)		
#define IS_SKB_HEAD(skb)	

extern __inline__ void skb_queue_head_init(struct sk_buff_head *list)
{
	list->prev = (struct sk_buff *)list;
	list->next = (struct sk_buff *)list;
	list->qlen = 0;
}

/*
 *	Insert an sk_buff at the start of a list.
 *
 *	The "__skb_xxxx()" functions are the non-atomic ones that
 *	can only be called with interrupts disabled.
 */

extern __inline__ void __skb_queue_head(struct sk_buff_head *list, struct sk_buff *newsk)
{
	struct sk_buff *prev, *next;

	newsk->list = list;
	list->qlen++;
	prev = (struct sk_buff *)list;
	next = prev->next;
	newsk->next = next;
	newsk->prev = prev;
	next->prev = newsk;
	prev->next = newsk;
}

extern __inline__ void skb_queue_head(struct sk_buff_head *list, struct sk_buff *newsk)
{
	unsigned long flags;

	save_flags(flags);
	cli();
	__skb_queue_head(list, newsk);
	restore_flags(flags);
}

/*
 *	Insert an sk_buff at the end of a list.
 */

extern __inline__ void __skb_queue_tail(struct sk_buff_head *list, struct sk_buff *newsk)
{
	struct sk_buff *prev, *next;

	newsk->list = list;
	list->qlen++;
	next = (struct sk_buff *)list;
	prev = next->prev;
	newsk->next = next;
	newsk->prev = prev;
	next->prev = newsk;
	prev->next = newsk;
}

extern __inline__ void skb_queue_tail(struct sk_buff_head *list, struct sk_buff *newsk)
{
	unsigned long flags;

	save_flags(flags);
	cli();
	__skb_queue_tail(list, newsk);
	restore_flags(flags);
}

/*
 *	Remove an sk_buff from a list.
 */

extern __inline__ struct sk_buff *__skb_dequeue(struct sk_buff_head *list)
{
	struct sk_buff *next, *prev, *result;

	prev = (struct sk_buff *) list;
	next = prev->next;
	result = NULL;
	if (next != prev) {
		result = next;
		next = next->next;
		list->qlen--;
		next->prev = prev;
		prev->next = next;
		result->next = NULL;
		result->prev = NULL;
		result->list = NULL;
	}
	return result;
}

extern __inline__ struct sk_buff *skb_dequeue(struct sk_buff_head *list)
{
	long flags;
	struct sk_buff *result;

	save_flags(flags);
	cli();
	result = __skb_dequeue(list);
	restore_flags(flags);
	return result;
}

/*
 *	Insert a packet on a list.
 */

extern __inline__ void __skb_insert(struct sk_buff *newsk,
	struct sk_buff * prev, struct sk_buff *next,
	struct sk_buff_head * list)
{
	newsk->next = next;
	newsk->prev = prev;
	next->prev = newsk;
	prev->next = newsk;
	newsk->list = list;
	list->qlen++;
}

/*
 *	Place a packet before a given packet in a list
 */
extern __inline__ void skb_insert(struct sk_buff *old, struct sk_buff *newsk)
{
	unsigned long flags;

	save_flags(flags);
	cli();
	__skb_insert(newsk, old->prev, old, old->list);
	restore_flags(flags);
}

/*
 *	Place a packet after a given packet in a list.
 */

extern __inline__ void skb_append(struct sk_buff *old, struct sk_buff *newsk)
{
	unsigned long flags;

	save_flags(flags);
	cli();
	__skb_insert(newsk, old, old->next, old->list);
	restore_flags(flags);
}

/*
 * remove sk_buff from list. _Must_ be called atomically, and with
 * the list known..
 */
extern __inline__ void __skb_unlink(struct sk_buff *skb, struct sk_buff_head *list)
{
	struct sk_buff * next, * prev;

	list->qlen--;
	next = skb->next;
	prev = skb->prev;
	skb->next = NULL;
	skb->prev = NULL;
	skb->list = NULL;
	next->prev = prev;
	prev->next = next;
}

/*
 *	Remove an sk_buff from its list. Works even without knowing the list it
 *	is sitting on, which can be handy at times. It also means that THE LIST
 *	MUST EXIST when you unlink. Thus a list must have its contents unlinked
 *	_FIRST_.
 */

extern __inline__ void skb_unlink(struct sk_buff *skb)
{
	unsigned long flags;

	save_flags(flags);
	cli();
	if(skb->list)
		__skb_unlink(skb, skb->list);
	restore_flags(flags);
}

/*
 *	Add data to an sk_buff
 */
 
extern __inline__ unsigned char *skb_put(struct sk_buff *skb, unsigned int len)
{
	extern char *skb_put_errstr;
	unsigned char *tmp=skb->tail;
	skb->tail+=len;
	skb->len+=len;
	if(skb->tail>skb->end)
	{
		__label__ here;
		panic(skb_put_errstr,&&here,len);
here:
	}
	return tmp;
}

extern __inline__ unsigned char *skb_push(struct sk_buff *skb, unsigned int len)
{
	extern char *skb_push_errstr;
	skb->data-=len;
	skb->len+=len;
	if(skb->data<skb->head)
	{
		__label__ here;
		panic(skb_push_errstr, &&here,len);
here:
	}
	return skb->data;
}

extern __inline__ unsigned char * skb_pull(struct sk_buff *skb, unsigned int len)
{
	if (len > skb->len)
		return NULL;
	skb->data+=len;
	skb->len-=len;
	return skb->data;
}

extern __inline__ int skb_headroom(struct sk_buff *skb)
{
	return skb->data-skb->head;
}

extern __inline__ int skb_tailroom(struct sk_buff *skb)
{
	return skb->end-skb->tail;
}

extern __inline__ void skb_reserve(struct sk_buff *skb, unsigned int len)
{
	skb->data+=len;
	skb->tail+=len;
}

extern __inline__ void skb_trim(struct sk_buff *skb, unsigned int len)
{
	if (skb->len > len) {
		skb->len = len;
		skb->tail = skb->data+len;
	}
}

/* dev_tint can lock buffer at any moment,
 * so that cli(), unlink it and sti(),
 * now it is safe.
 */

extern __inline__ int skb_steal(struct sk_buff *skb)
{
	unsigned long flags;

	save_flags(flags);
	cli();
	if (skb->next) {
		skb_unlink(skb);
		atomic_dec(&skb->users);
	}
	restore_flags(flags);
	return 1;
}

extern __inline__ void __skb_steal(struct sk_buff *skb)
{
	if (skb->next) {
		skb_unlink(skb);
		atomic_dec(&skb->users);
	}
}

extern __inline__ void skb_orphan(struct sk_buff *skb)
{
	if (skb->destructor)
		skb->destructor(skb);
	skb->destructor = NULL;
	skb->sk = NULL;
}


#endif

extern struct sk_buff *		skb_realloc_headroom(struct sk_buff *skb, int newheadroom);
extern struct sk_buff *		skb_recv_datagram(struct sock *sk,unsigned flags,int noblock, int *err);
extern unsigned int		datagram_poll(struct socket *sock, poll_table *wait);
extern int			skb_copy_datagram(struct sk_buff *from, int offset, char *to,int size);
extern int			skb_copy_datagram_iovec(struct sk_buff *from, int offset, struct iovec *to,int size);
extern void			skb_free_datagram(struct sock * sk, struct sk_buff *skb);

#endif	/* __KERNEL__ */
#endif	/* _LINUX_SKBUFF_H */
