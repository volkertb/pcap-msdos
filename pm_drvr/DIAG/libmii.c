/* libmii.c: MII diagnostic and setup library.

	Copyright 1997-1998 by Donald Becker.
   This version released under the Gnu Public Lincese, incorporated herein
   by reference.  Contact the author for use under other terms.
   The author may be reached as becker@cesdis.edu.
   C/O USRA-CESDIS, Code 930.5 Bldg. 28, Nimbus Rd., Greenbelt MD 20771

   References
	http://cesdis.gsfc.nasa.gov/linux/misc/NWay.html
	http://www.national.com/pf/DP/DP83840.html
*/

static const char version[] =
"libmii.c:v1.04 8/10/98  Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";

extern void mdio_sync(int ioaddr);
extern int mdio_read(int ioaddr, int phy_id, int location);

#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

typedef unsigned short u16;

static const char *media_names[] = {
	"10baseT", "10baseT-FD", "100baseTx", "100baseTx-FD", "100baseT4",
	"Flow-control", 0,
};

static void qs6612(int ioaddr, int phy_id);
static void smsc83c180(int ioaddr, int phy_id);
static void tdk78q2120(int ioaddr, int phy_id);

struct mii_partnum {
	char *vendor;				/* Vendor name. */
	u16	 phy_id0;				/* Vendor ID (alternate ver. of ieee_oui[]) */
	u16	 phy_id1;				/* Vendor ID (alternate ver. of ieee_oui[]) */
	unsigned char ieee_oui[3];	/* IEEE-assigned organizationally unique ID */
	char flags;
	void (*(func))(int xcvr_if, int phy_id);/* Function to emit more info. */
} oui_map[] = {
	{"Unknown transceiver type", 0x0000, 0x0000, {0,}, 0, NULL,},
	{"National Semiconductor 83840A", 0x2000, 0x5c01, {0,}, 0, NULL,},
	{"Level One LXT970", 0x7810, 0x0000, {0,}, 0, NULL, },
	{"Level One LXT971", 0x7810, 0x0001, {0,}, 0, NULL, },
	{"Level One (unknown type)", 0, 0, {0x1e,0x04,0x00}, 0, NULL, },
	{"Davicom DM9101", 0x0181, 0xB800, {0,}, 0, NULL, },
	{"Davicom (unknown type)", 0, 0, {0x00, 0x60, 0x6e}, 0, NULL, },
	{"Quality Semiconductor QS6612", 0x0181, 0x4410, {0,}, 0, qs6612},
	{"Quality Semiconductor (unknown type)", 0,0, {0x00, 0x60, 0x51}, 0, NULL},
	{"SMSC 83c180", 0x0282, 0x1C51, {0}, 0, smsc83c180, },
	{"TDK Semiconductor 78Q2120", 0x0300, 0xE542, {0,}, 0, tdk78q2120, },
	{"TDK Semiconductor 78Q2120", 0x0300, 0xE543, {0,}, 0, tdk78q2120, },
	{"TDK transceiver (unknown type)", 0,0, {0x00, 0xc0, 0x39}, 0, tdk78q2120},
	{0, },
};

void show_mii_details(int ioaddr, int phy_id)
{
	int mii_reg, i, vendor = 0;
	u16 mii_val[32], bmcr, bmsr, new_bmsr;

	printf(" MII PHY #%d transceiver registers:", phy_id);
	for (mii_reg = 0; mii_reg < 32; mii_reg++) {
		mii_val[mii_reg] = mdio_read(ioaddr, phy_id, mii_reg);
		printf("%s %4.4x", (mii_reg % 8) == 0 ? "\n  " : "",
			   mii_val[mii_reg]);
	}
	printf(".\n");
	if (mii_val[0] == 0xffff) {
		printf("  No MII transceiver present!.\n");
		return;
	}
	bmcr = mii_val[0];
	bmsr = mii_val[1];
	printf(" Basic mode control register 0x%4.4x:", bmcr);
	if (bmcr & 0x1000)
		printf(" Auto-negotiation enabled.\n");
	else
		printf(" Auto-negotiation disabled!\n"
			   "   Speed fixed at 10%s mbps, %s-duplex.\n",
			   bmcr & 0x2000 ? "0" : "",
			   bmcr & 0x0100 ? "full":"half");
	if (bmcr & 0x8000)
		printf("  Transceiver currently being reset!\n");
	if (bmcr & 0x4000)
		printf("  Transceiver in loopback mode!\n");
	if (bmcr & 0x0800)
		printf("  Transceiver powered down!\n");
	if (bmcr & 0x0400)
		printf("  Transceiver isolated from the MII!\n");
	if (bmcr & 0x0200)
		printf("  Restarted auto-negotiation in progress!\n");
	if (bmcr & 0x0080)
		printf("  Internal Collision-Test enabled!\n");
	
	new_bmsr = mdio_read(ioaddr, phy_id, 1);
	printf(" Basic mode status register 0x%4.4x ... %4.4x.\n"
		   "   Link status: %sestablished.\n"
		   "   Capable of ",
		   bmsr, new_bmsr,
		   bmsr & 0x0004 ? "" :
		    (new_bmsr & 0x0004) ? "previously broken, but now re" : "not ");
	if (bmsr & 0xF800) {
		for (i = 15; i >= 11; i--)
			if (bmsr & (1<<i))
				printf(" %s", media_names[i-11]);
	} else
		printf("<Warning! No media capabilities>");

	printf(".\n"
		   "   %s to perform Auto-negotiation, negotiation %scomplete.\n",
		   bmsr & 0x0008 ? "Able" : "Unable",
		   bmsr & 0x0020 ? "" : "not ");

	if (bmsr & 0x0010)
		printf(" Remote fault detected!\n");
	if (bmsr & 0x0002)
		printf("   *** Link Jabber! ***\n");

	if (mii_val[2] ^ mii_val[3]) { 		/* Eliminate 0x0000 and 0xffff IDs. */
		unsigned char oui_0 = mii_val[2] >> 10;
		unsigned char oui_1 = mii_val[2] >> 2;
		unsigned char oui_2 = (mii_val[2] << 6) | (mii_val[3] >> 10);

		printf(" Vendor ID is %2.2x:%2.2x:%2.2x:--:--:--, model %d rev. %d.\n",
			   oui_0, oui_1, oui_2,
			   ((mii_val[3] >> 4) & 0x3f), mii_val[3] & 0x0f);
		for ( i = 0; oui_map[i].vendor; i++)
			/* We match either the Phy ID or the IEEE OUI. */
			if ((oui_map[i].phy_id0 == mii_val[2] &&
				 oui_map[i].phy_id1 == mii_val[3]) ||
				(oui_map[i].ieee_oui[0] == oui_0 &&
				 oui_map[i].ieee_oui[1] == oui_1 &&
				 oui_map[i].ieee_oui[2] == oui_2)) {
				printf("   Vendor/Part: %s.\n", oui_map[i].vendor);
				vendor = i;
				break;
			}
		if (oui_map[i].vendor == NULL)
			printf("   No specific information is known about this transceiver"
				   " type.\n");
	} else
		printf(" This transceiver has no vendor identification.\n");

	{
		int nway_advert = mdio_read(ioaddr, phy_id, 4);
		int lkpar = mdio_read(ioaddr, phy_id, 5);
		printf(" I'm advertising %4.4x:", nway_advert);
		for (i = 10; i >= 5; i--)
			if (nway_advert & (1<<i))
				printf(" %s", media_names[i-5]);
		printf("\n   Advertising %sadditional info pages.\n",
			   nway_advert & 0x8000 ? "" : "no ");
		if ((nway_advert & 31) == 1)
			printf("   IEEE 802.3 CSMA/CD protocol.\n");
		else
			printf("   Using an unknown (non 802.3) encapsulation.\n");
		printf(" Link partner capability is %4.4x:",
			   lkpar);
		for (i = 10; i >= 5; i--)
			if (lkpar & (1<<i))
				printf(" %s", media_names[i-5]);
		printf(".\n   Negotiation %s.\n",
			   lkpar & 0x4000 ? " completed" : "did not complete");
	}
	if (oui_map[vendor].func)
		oui_map[vendor].func(ioaddr, phy_id);

}

int monitor_mii(int ioaddr, int phy_id)
{
	int i;
	unsigned short new_1, baseline_1 = mdio_read(ioaddr, phy_id, 1);
	struct timeval tv, sleepval;
	time_t cur_time;
	struct timezone tz;
	char timebuf[12];

	if (baseline_1 == 0xffff) {
		fprintf(stderr, "No MII transceiver present to monitor.\n");
		return -1;
	}

	cur_time = time(NULL);
	strftime(timebuf, sizeof(timebuf), "%H:%M:%S", localtime(&cur_time));

	printf("%s.%03d  Baseline value of MII BMSR (basic mode status register)"
		   " is %4.4x.\n", timebuf, (int)tv.tv_usec/1000, baseline_1);
	while (1) {
		new_1 = mdio_read(ioaddr, phy_id, 1);
		if (new_1 != baseline_1) {
			cur_time = time(NULL);
			strftime(timebuf, sizeof(timebuf), "%H:%M:%S",
					 localtime(&cur_time));
			printf("%s.%03d  MII BMSR now %4.4x: %4s link, NWay %s, "
				   "%3sJabber%s (%4.4x).\n",
				   timebuf, (int)tv.tv_usec/1000, new_1,
				   new_1 & 0x04 ? "Good" : "no",
				   new_1 & 0x20 ? "done" : "busy",
				   new_1 & 0x02 ? "" : "No ",
				   new_1 & 0x10 ? ", remote fault" : "",
				   mdio_read(ioaddr, phy_id, 5)
				   );
			if (!(baseline_1 & 0x20)  && (new_1 & 0x20)) {
				int lkpar = mdio_read(ioaddr, phy_id, 5);
				printf("   New link partner capability is %4.4x %4.4x:",
					   lkpar, mdio_read(ioaddr, phy_id, 6));
				for (i = 9; i >= 5; i--)
					if (lkpar & (1<<i))
						printf(" %s", media_names[i-5]);
				printf(".\n");
			}
			baseline_1 = new_1;
		}
		sleepval.tv_sec = 0;
		sleepval.tv_usec = 200000;
		select(0, 0, 0, 0, &sleepval);			/* Or just sleep(1); */
	}
	printf("  Value of MII BMSR (basic mode status register) is %4.4x.\n",
		   mdio_read(ioaddr, phy_id, 1));
	return 0;
}

/* Emit transceiver-specific info. */
static void qs6612(int ioaddr, int phy_id)
{
	printf("  QS6612 extra registers: Mode %4.4x.\n"
		   "    Interrupt source %4.4x, mask %4.4x.\n"
		   "    PHY control %4.4x.\n",
		   mdio_read(ioaddr, phy_id, 17),
		   mdio_read(ioaddr, phy_id, 29),
		   mdio_read(ioaddr, phy_id, 30),
		   mdio_read(ioaddr, phy_id, 31));
	return;
}
static void smsc83c180(int ioaddr, int phy_id)
{
	int mii_reg25 = mdio_read(ioaddr, phy_id, 25);
	printf("  SMSC 83c180 extra registers:\n"
		   "    Auto-negotiation status 0x%4.4x.\n"
		   "      10baseT polarity is %s.\n"
		   "      PHY address is %d.\n"
		   "      Auto-negotiation %scomplete, 1%s0Mbps %s duplex.\n"
		   "    Rx symbol errors since last read %d.\n",
		   mii_reg25,
		   mii_reg25 & 0x2000 ? "normal" : "reversed",
		   (mii_reg25>>8) & 0x1F,
		   mii_reg25 & 0x0080 ? "did not " : "",
		   mii_reg25 & 0x0020 ? "0" : "",
		   mii_reg25 & 0x0040 ? "full" : "half",
		   mdio_read(ioaddr, phy_id, 26));
	return;
}
static const char *tdk_events[8] = {
	"Jabber", "Rx error", "Negotiation page received", "Link detection fault",
	"Link partner acknowledge", "Link status change", "Remote partner fault",
	"Auto-Negotiation complete"};

static void tdk78q2120(int ioaddr, int phy_id)
{
	int mii_reg16 = mdio_read(ioaddr, phy_id, 16);
	int mii_reg17 = mdio_read(ioaddr, phy_id, 17);
	int mii_reg18 = mdio_read(ioaddr, phy_id, 18);
	int i;
	printf("  TDK 78q2120 extra registers:\n"
		   "    Vendor specific register 16 is 0x%4.4x.\n"
		   "      Link polarity is %s %s.\n"
		   "%s%s"
		   "    Vendor specific register 18 is 0x%4.4x.\n"
		   "      Auto-negotiation %s, 1%s0Mbps %s duplex.\n"
		   "      Rx link in %s state, PLL %s.\n"
		   "    Vendor specific register 17 is 0x%4.4x.\n"
		   "      Events since last read:",
		   mii_reg16,
		   mii_reg16 & 0x0020 ? "OVERRIDDEN to" : "detected as",
		   mii_reg16 & 0x0010 ? "reversed" : "normal",
		   mii_reg16 & 0x0002 ?
		   "     100baseTx Coding and scrambling is disabled!\n":"",
		   mii_reg16 & 0x0001 ? "     Rx_CLK power-save mode is enabled!\n":"",
		   mii_reg18,
		   mii_reg18 & 0x1000 ? "had no common media" : "complete",
		   mii_reg18 & 0x0400 ? "0" : "",
		   mii_reg18 & 0x0800 ? "full" : "half",
		   mii_reg18 & 0x0200 ? "pass" : "fail",
		   mii_reg18 & 0x0100 ? "locked" : "slipped since last read",
		   mii_reg17);
	for (i = 0; i < 8; i++)
		if (mii_reg17 & (1 << i))
			printf("  %s", tdk_events[i]);
	if (mii_reg17 & 0xff00) {
		printf("\n      Events that will raise an interrupt:");
		for (i = 0; i < 8; i++)
			if (mii_reg17 & (0x100 << i))
				printf("  %s", tdk_events[i]);
	}
	printf("\n");
	return;
}

/*
 * Local variables:
 *  compile-command: "cc -O -Wall -c libmii.c"
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
