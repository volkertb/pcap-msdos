#ifndef __INTERFACE_H
#define __INTERFACE_H

/* Standard interface flags
 */
#define IFF_UP          0x1          /* interface is up              */
#define IFF_BROADCAST   0x2          /* broadcast address valid      */
#define IFF_DEBUG       0x4          /* turn on debugging            */
#define IFF_LOOPBACK    0x8          /* is a loopback net            */
#define IFF_POINTOPOINT 0x10         /* interface is has p-p link    */
#define IFF_NOTRAILERS  0x20         /* avoid use of trailers        */
#define IFF_RUNNING     0x40         /* resources allocated          */
#define IFF_NOARP       0x80         /* no ARP protocol              */
#define IFF_PROMISC     0x100        /* receive all packets          */
#define IFF_ALLMULTI    0x200        /* receive all multicast packets*/
#define IFF_MULTICAST   0x8000       /* Supports multicast           */

#endif
