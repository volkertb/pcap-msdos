#ifndef __NE_H
#define __NE_H

/* Some defines that people can play with if so inclined.
 */

/* Do we support clones that don't adhere to 14,15 of the SAprom ?
 */
/* #define SUPPORT_NE_BAD_CLONES */ /* in makefile */

/* Do we perform extra sanity checks on stuff ?
 */
/* #define NE_SANITY_CHECK */ /* in makefile */

/* Do we implement the read before write bugfix ?
 */
/* #define NE_RW_BUGFIX */ /* in makefile */

/* Do we have a non std. amount of memory? (in units of 256 byte pages)
 */
/* #define PACKETBUF_MEMSIZE  0x40 */


extern int ne_probe (struct device *dev);

#endif
