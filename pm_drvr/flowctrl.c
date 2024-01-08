/*
 * flow-ctrl.c: Send 802.3x PAUSE flow control packets to another machine
 */

static char usage_msg[] =
  "usage: flow-ctrl [-p pause-time] [-vrVD] [-i interface] [-n npackets] [00:11:22:33:44:55]\n"
  "      -p pause-time:  Specify the `pause time' value in the packet (0-65535)\n"
  "      -v:             Be verbose\n"
  "      -r:             Repeat packet indefinitely\n"
  "      -V:             Show version\n"
  "      -D:             Debug\n"
  "      -i interface:   Specify interface (eg eth0)\n"
  "      -n npackets:    Number of times to send the PAUSE packet\n"
  "\n"

  "      The target host's MAC address may be obtained using the `arp' command\n"
  "      If it is not specified, the reserved unicast address 01:80:c2:00:00:01 is used\n";


static char version_msg[] = "flow-ctrl.c: v0.01 1 Jul 00 Andrew Morton, Donald Becker, http://www.scyld.com/";

/*
 * This program generates and transmits 802.3x MAC-layer PAUSE flow control packets
 * and sends them to the dsignated host.
 * 
 * This software may be used and distributed according to the terms
 * of the GNU Public License, incorporated herein by reference.
 * Contact the author for use under other terms.
 * 
 * Derived from Donald Becker's ether-wake.c
 * 
 * Andrew Morton <andrewm@uow.edu.au>
 * 
 * Note: On some systems dropping root capability allows the process to be
 * dumped, traced or debugged.
 * If someone traces this program, they get control of a raw socket.
 * Linux handles this safely, but beware when porting this program.
 * 
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netinet/in.h>

u_char outpack[1000];
int    outpack_sz = 0;
int    debug = 0;
int    pause_time = 100;
int    npackets = 1;

static int opt_no_src_addr = 0;

static int get_fill (unsigned char *pkt, char *arg);

int main (int argc, char *argv[])
{
  struct sockaddr whereto;         /* who to wake up */
  char  *ifname = "eth0";
  int    one = 1;                  /* True, for socket options. */
  int    s;                        /* Raw socket */
  int    errflag = 0, verbose = 0, do_version = 0;
  int    i, c, pktsize;

  while ((c = getopt (argc, argv, "rDn:i:p:v")) != -1)
    switch (c)
    {
      case 'D':
           debug++;
           break;
      case 'i':
           ifname = optarg;
           break;
      case 'p':
           pause_time = strtol (optarg, NULL, 10);
           break;
      case 'v':
           verbose++;
           break;
      case 'r':
           npackets = -1;
           break;
      case 'n':
           npackets = strtol (optarg, NULL, 10);
           break;
      case 'V':
           do_version++;
           break;
      case '?':
           errflag++;
    }

  if (verbose || do_version)
     printf ("%s\n", version_msg);
  if (errflag)
  {
    fprintf (stderr, usage_msg);
    return 3;
  }

  /* Note: PF_INET, SOCK_DGRAM, IPPROTO_UDP would allow SIOCGIFHWADDR to
   * work as non-root, but we need SOCK_PACKET to specify the Ethernet
   * destination address.
   */
  if ((s = socket (AF_INET, SOCK_PACKET, SOCK_PACKET)) < 0)
  {
    if (errno == EPERM)
         fprintf (stderr, "flow-ctrl must run as root\n");
    else perror ("flow-ctrl: socket");
    if (!debug)
      return (2);
  }

  /* Don't revert if debugging allows a normal user to get the raw socket.
   */
  setuid (getuid());

  pktsize = get_fill (outpack, optind == argc ? "01:80:c2:00:00:01" : argv[optind]);

  /* Fill in the source address, if possible.
   * The code to retrieve the local station address is Linux specific.
   */
  if (!opt_no_src_addr)
  {
    struct ifreq if_hwaddr;
    u_char *hwaddr = if_hwaddr.ifr_hwaddr.sa_data;

    strcpy (if_hwaddr.ifr_name, ifname);
    if (ioctl (s, SIOCGIFHWADDR, &if_hwaddr) < 0)
    {
      fprintf (stderr, "SIOCGIFHWADDR on %s failed: %s\n",
               ifname, strerror (errno));
      return (1);
    }
    memcpy (outpack + 6, if_hwaddr.ifr_hwaddr.sa_data, 6);

    if (verbose)
    {
      printf ("The hardware address (SIOCGIFHWADDR) of %s is type %d  "
              "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x.\n",
              ifname, if_hwaddr.ifr_hwaddr.sa_family,
              hwaddr[0], hwaddr[1], hwaddr[2], hwaddr[3], hwaddr[4], hwaddr[5]);
    }
  }

  if (verbose > 1)
  {
    printf ("The final packet is: ");
    for (i = 0; i < pktsize; i++)
      printf (" %2.2x", outpack[i]);
    printf (".\n");
  }

  /* This is necessary for broadcasts to work */
  if (setsockopt (s, SOL_SOCKET, SO_BROADCAST, (char*)&one, sizeof(one)) < 0)
     perror ("setsockopt: SO_BROADCAST");

  whereto.sa_family = 0;
  strcpy (whereto.sa_data, ifname);

  for (;;)
  {
    if ((i = sendto (s, outpack, pktsize, 0, &whereto, sizeof (whereto))) < 0)
    {
      perror ("sendto");
      exit (1);
    }
    if (verbose)
    {
      printf (".");
      fflush (stdout);
    }
    if (debug)
       printf ("Sendto worked ! %d.\n", i);
    if (npackets != -1)
       npackets--;
    if (npackets == 0)
       break;
  }
  if (verbose)
    printf ("\n");

#ifdef USE_SEND
  if (bind (s, &whereto, sizeof (whereto)) < 0)
     perror ("bind");
  else if (send (s, outpack, 100, 0) < 0)
     perror ("send");
#endif

  return 0;
}

static int get_fill (unsigned char *pkt, char *arg)
{
  char station_addr[6];
  int  sa[6];
  int  byte_cnt;
  int  offset, i;
  char *cp;

  for (cp = arg; *cp; cp++)
    if (*cp != ':' && !isxdigit (*cp))
    {
      fprintf (stderr, "flow-ctrl: patterns must be specified as hex digits.\n");
      exit (2);
    }

  byte_cnt = sscanf (arg, "%2x:%2x:%2x:%2x:%2x:%2x",
                     &sa[0], &sa[1], &sa[2], &sa[3], &sa[4], &sa[5]);
  for (i = 0; i < 6; i++)
     station_addr[i] = sa[i];

  if (debug)
     fprintf (stderr, "Command line stations address is "
              "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x.\n",
              sa[0], sa[1], sa[2], sa[3], sa[4], sa[5]);

  if (byte_cnt != 6)
  {
    fprintf (stderr, "flow-ctrl: The destination address must be specified as "
             "00:11:22:33:44:55.\n");
    exit (2);
  }

  memcpy (pkt, station_addr, 6);
  memcpy (pkt + 6, station_addr, 6);
  pkt[12] = 0x88;
  pkt[13] = 0x08;
  /* MAC control opcode 00:01 */
  pkt[14] = 0;
  pkt[15] = 1;
  pkt[16] = pause_time >> 8;
  pkt[17] = pause_time;

  offset = 18;

  memset (pkt + offset, 0xff, 42);
  offset += 42;

  if (debug)
  {
    fprintf (stderr, "Packet is\n");
    for (i = 0; i < offset; i++)
    {
      if ((i & 15) == 0)
         fprintf (stderr, "0x%04x: ", i);
      fprintf (stderr, "%02x ", pkt[i]);
      if (((i + 1) & 15) == 0)
         fprintf (stderr, "\n");
    }
    fprintf (stderr, "\n");
  }
  return (offset);
}


