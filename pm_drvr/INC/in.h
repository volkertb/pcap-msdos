/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Definitions of the Internet Protocol.
 *
 * Version:	@(#)in.h	1.0.1	04/21/93
 *
 * Authors:	Original taken from the GNU Project <netinet/in.h> file.
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
#ifndef _LINUX_IN_H
#define _LINUX_IN_H

#include <linux/types.h>

/* Standard well-defined IP protocols.  */
enum {
  IPPROTO_IP = 0,		/* Dummy protocol for TCP		*/
  IPPROTO_ICMP = 1,		/* Internet Control Message Protocol	*/
  IPPROTO_IGMP = 2,		/* Internet Group Management Protocol	*/
  IPPROTO_IPIP = 4,		/* IPIP tunnels (older KA9Q tunnels use 94) */
  IPPROTO_TCP = 6,		/* Transmission Control Protocol	*/
  IPPROTO_EGP = 8,		/* Exterior Gateway Protocol		*/
  IPPROTO_PUP = 12,		/* PUP protocol				*/
  IPPROTO_UDP = 17,		/* User Datagram Protocol		*/
  IPPROTO_IDP = 22,		/* XNS IDP protocol			*/

  IPPROTO_IPV6	 = 41,		/* IPv6-in-IPv4 tunnelling		*/
  IPPROTO_ICMPV6 = 58,		/* ICMPv6				*/

  IPPROTO_RAW	 = 255,		/* Raw IP packets			*/
  IPPROTO_MAX
};


/* Internet address. */
struct in_addr {
	__u32	s_addr;
};

#define IP_TOS		1
#define IP_TTL		2
#define IP_HDRINCL	3
#define IP_OPTIONS	4
#define IP_LOCALADDR	5	/* Cannot remove; a lot of apps still use it. ANK */
#define IP_RECVOPTS	6
#define IP_RETOPTS	7
#define IP_RXINFO	8
#define IP_TXINFO	IP_RXINFO
/* Gated uses it. Remove later or preserve for 4.4BSD compatibility??? */
#define IP_RECVDSTADDR  9
#define IP_PMTUDISC	10
#define IP_RECVERR	11

/* BSD compatibility */
#define IP_RECVRETOPTS	IP_RETOPTS

/* IP_PMTUDISC values */
#define IP_PMTUDISC_WANT		0	/* Use per route hints	*/
#define IP_PMTUDISC_DONT		1	/* Never send DF frames */
#define IP_PMTUDISC_DO			2	/* Always DF		*/

#define IP_MULTICAST_IF			32
#define IP_MULTICAST_TTL 		33
#define IP_MULTICAST_LOOP 		34
#define IP_ADD_MEMBERSHIP		35
#define IP_DROP_MEMBERSHIP		36
#define IP_MULTICAST_IFN		37
#define IP_ADD_MEMBERSHIPN		38
#define IP_DROP_MEMBERSHIPN		39

/* These need to appear somewhere around here */
#define IP_DEFAULT_MULTICAST_TTL        1
#define IP_DEFAULT_MULTICAST_LOOP       1
#define IP_MAX_MEMBERSHIPS              20

/* Request struct for multicast socket ops */

struct ip_mreq 
{
	struct in_addr imr_multiaddr;	/* IP multicast address of group */
	struct in_addr imr_interface;	/* local IP address of interface */
};

struct ip_mreqn
{
	struct in_addr	imr_multiaddr;		/* IP multicast address of group */
	struct in_addr	imr_address;		/* local IP address of interface */
	int		imr_ifindex;		/* Interface index */
};

struct in_pktinfo
{
	int		ipi_ifindex;
	struct in_addr	ipi_spec_dst;
	struct in_addr	ipi_addr;
};

/* Structure describing an Internet (IP) socket address. */
#define __SOCK_SIZE__	16		/* sizeof(struct sockaddr)	*/
struct sockaddr_in {
  sa_family_t		sin_family;	/* Address family		*/
  unsigned short int	sin_port;	/* Port number			*/
  struct in_addr	sin_addr;	/* Internet address		*/

  /* Pad to size of `struct sockaddr'. */
  unsigned char		__pad[__SOCK_SIZE__ - sizeof(short int) -
			sizeof(unsigned short int) - sizeof(struct in_addr)];
};
#define sin_zero	__pad		/* for BSD UNIX comp. -FvK	*/


/*
 * Definitions of the bits in an Internet address integer.
 * On subnets, host and network parts are found according
 * to the subnet mask, not these masks.
 */
#define	IN_CLASSA(a)		((((long int) (a)) & 0x80000000) == 0)
#define	IN_CLASSA_NET		0xff000000
#define	IN_CLASSA_NSHIFT	24
#define	IN_CLASSA_HOST		(0xffffffff & ~IN_CLASSA_NET)
#define	IN_CLASSA_MAX		128

#define	IN_CLASSB(a)		((((long int) (a)) & 0xc0000000) == 0x80000000)
#define	IN_CLASSB_NET		0xffff0000
#define	IN_CLASSB_NSHIFT	16
#define	IN_CLASSB_HOST		(0xffffffff & ~IN_CLASSB_NET)
#define	IN_CLASSB_MAX		65536

#define	IN_CLASSC(a)		((((long int) (a)) & 0xe0000000) == 0xc0000000)
#define	IN_CLASSC_NET		0xffffff00
#define	IN_CLASSC_NSHIFT	8
#define	IN_CLASSC_HOST		(0xffffffff & ~IN_CLASSC_NET)

#define	IN_CLASSD(a)		((((long int) (a)) & 0xf0000000) == 0xe0000000)
#define	IN_MULTICAST(a)		IN_CLASSD(a)
#define IN_MULTICAST_NET	0xF0000000

#define	IN_EXPERIMENTAL(a)	((((long int) (a)) & 0xf0000000) == 0xf0000000)
#define	IN_BADCLASS(a)		IN_EXPERIMENTAL((a))

/* Address to accept any incoming messages. */
#define	INADDR_ANY		((unsigned long int) 0x00000000)

/* Address to send to all hosts. */
#define	INADDR_BROADCAST	((unsigned long int) 0xffffffff)

/* Address indicating an error return. */
#define	INADDR_NONE		((unsigned long int) 0xffffffff)

/* Network number for local host loopback. */
#define	IN_LOOPBACKNET		127

/* Address to loopback in software to local host.  */
#define	INADDR_LOOPBACK		0x7f000001	/* 127.0.0.1   */
#define	IN_LOOPBACK(a)		((((long int) (a)) & 0xff000000) == 0x7f000000)

/* Defines for Multicast INADDR */
#define INADDR_UNSPEC_GROUP   	0xe0000000U	/* 224.0.0.0   */
#define INADDR_ALLHOSTS_GROUP 	0xe0000001U	/* 224.0.0.1   */
#define INADDR_ALLRTRS_GROUP    0xe0000002U	/* 224.0.0.2 */
#define INADDR_MAX_LOCAL_GROUP  0xe00000ffU	/* 224.0.0.255 */


/* <asm/byteorder.h> contains the htonl type stuff.. */
#include <asm/byteorder.h> 

#ifdef __KERNEL__
/* Some random defines to make it easier in the kernel.. */
#define LOOPBACK(x)	(((x) & htonl(0xff000000)) == htonl(0x7f000000))
#define MULTICAST(x)	(((x) & htonl(0xf0000000)) == htonl(0xe0000000))
#define BADCLASS(x)	(((x) & htonl(0xf0000000)) == htonl(0xf0000000))
#define ZERONET(x)	(((x) & htonl(0xff000000)) == htonl(0x00000000))
#define LOCAL_MCAST(x)	(((x) & htonl(0xFFFFFF00)) == htonl(0xE0000000))

#endif

#endif	/* _LINUX_IN_H */
