/*
 * $Id: pci.c,v 1.91 1999/01/21 13:34:01 davem Exp $
 *
 * PCI Bus Services, see include/linux/pci.h for further explanation.
 *
 * Copyright 1993 -- 1997 Drew Eckhardt, Frederic Potter,
 * David Mosberger-Tang
 *
 * Copyright 1997 -- 1999 Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 */

#include "pmdrvr.h"
#include "module.h"
#include "bios32.h"
#include "pci.h"

int pci_debug = 0;

#define PRINTK(x) do {             \
                    if (pci_debug) \
                       printk x ;  \
                  } while (0)

struct pci_bus  pci_root;
struct pci_dev *pci_devices = NULL;

static struct pci_dev **pci_last_dev_p = &pci_devices;
static int              pci_reverse = 0;


/*
 * The bridge_id field is an offset of an item into the array
 * BRIDGE_MAPPING_TYPE. 0xff indicates that the device is not a PCI
 * bridge, or that we don't know for the moment how to configure it.
 * I'm trying to do my best so that the kernel stays small.  Different
 * chipset can have same optimization structure. i486 and pentium
 * chipsets from the same manufacturer usually have the same
 * structure.
 */
#define DEVICE(vid,did,name) \
        { PCI_VENDOR_ID_##vid, PCI_DEVICE_ID_##did, (name), 0xFF }

#define BRIDGE(vid,did,name,bridge) \
        { PCI_VENDOR_ID_##vid, PCI_DEVICE_ID_##did, (name), (bridge) }

/*
 * Sorted in ascending order by vendor and device.
 * Use binary search for lookup. If you add a device make sure
 * it is sequential by both vendor and device id.
 */
struct pci_dev_info dev_info[] = {
  DEVICE (COMPAQ, COMPAQ_TOKENRING, "TokenRing"),
  DEVICE (COMPAQ, COMPAQ_1280, "QVision 1280/p"),
  DEVICE (COMPAQ, COMPAQ_TRIFLEX, "TriFlex"),
  DEVICE (COMPAQ, COMPAQ_SMART2P, "Smart-2/P RAID Controller"),
  DEVICE (COMPAQ, COMPAQ_NETEL100, "Netelligent 10/100"),
  DEVICE (COMPAQ, COMPAQ_NETEL10, "Netelligent 10"),
  DEVICE (COMPAQ, COMPAQ_NETFLEX3I, "NetFlex 3"),
  DEVICE (COMPAQ, COMPAQ_NETEL100D, "Netelligent 10/100 Dual"),
  DEVICE (COMPAQ, COMPAQ_NETEL100PI, "Netelligent 10/100 ProLiant"),
  DEVICE (COMPAQ, COMPAQ_NETEL100I, "Netelligent 10/100 Integrated"),
  DEVICE (COMPAQ, COMPAQ_THUNDER, "ThunderLAN"),
  DEVICE (COMPAQ, COMPAQ_NETFLEX3B, "NetFlex 3 BNC"),
  DEVICE (NCR, NCR_53C810, "53c810"),
  DEVICE (NCR, NCR_53C820, "53c820"),
  DEVICE (NCR, NCR_53C825, "53c825"),
  DEVICE (NCR, NCR_53C815, "53c815"),
  DEVICE (NCR, NCR_53C860, "53c860"),
  DEVICE (NCR, NCR_53C1510D, "53c1510d"),
  DEVICE (NCR, NCR_53C896, "53c896"),
  DEVICE (NCR, NCR_53C895, "53c895"),
  DEVICE (NCR, NCR_53C885, "53c885"),
  DEVICE (NCR, NCR_53C875, "53c875"),
  DEVICE (NCR, NCR_53C1510, "53c1510"),
  DEVICE (NCR, NCR_53C875J, "53c875J"),
  DEVICE (ATI, ATI_68800, "68800AX"),
  DEVICE (ATI, ATI_215CT222, "215CT222"),
  DEVICE (ATI, ATI_210888CX, "210888CX"),
  DEVICE (ATI, ATI_215GB, "Mach64 GB"),
  DEVICE (ATI, ATI_215GD, "Mach64 GD (Rage Pro)"),
  DEVICE (ATI, ATI_215GI, "Mach64 GI (Rage Pro)"),
  DEVICE (ATI, ATI_215GP, "Mach64 GP (Rage Pro)"),
  DEVICE (ATI, ATI_215GQ, "Mach64 GQ (Rage Pro)"),
  DEVICE (ATI, ATI_215GT, "Mach64 GT (Rage II)"),
  DEVICE (ATI, ATI_215GTB, "Mach64 GT (Rage II)"),
  DEVICE (ATI, ATI_210888GX, "210888GX"),
  DEVICE (ATI, ATI_215LG, "Mach64 LG (3D Rage LT)"),
  DEVICE (ATI, ATI_264LT, "Mach64 LT"),
  DEVICE (ATI, ATI_264VT, "Mach64 VT"),
  DEVICE (VLSI, VLSI_82C592, "82C592-FC1"),
  DEVICE (VLSI, VLSI_82C593, "82C593-FC1"),
  DEVICE (VLSI, VLSI_82C594, "82C594-AFC2"),
  DEVICE (VLSI, VLSI_82C597, "82C597-AFC2"),
  DEVICE (VLSI, VLSI_82C541, "82C541 Lynx"),
  DEVICE (VLSI, VLSI_82C543, "82C543 Lynx ISA"),
  DEVICE (VLSI, VLSI_82C532, "82C532"),
  DEVICE (VLSI, VLSI_82C534, "82C534"),
  DEVICE (VLSI, VLSI_82C535, "82C535"),
  DEVICE (VLSI, VLSI_82C147, "82C147"),
  DEVICE (VLSI, VLSI_VAS96011, "VAS96011 (Golden Gate II)"),
  DEVICE (ADL, ADL_2301, "2301"),
  DEVICE (NS, NS_87415, "87415"),
  DEVICE (NS, NS_87410, "87410"),
  DEVICE (TSENG, TSENG_W32P_2, "ET4000W32P"),
  DEVICE (TSENG, TSENG_W32P_b, "ET4000W32P rev B"),
  DEVICE (TSENG, TSENG_W32P_c, "ET4000W32P rev C"),
  DEVICE (TSENG, TSENG_W32P_d, "ET4000W32P rev D"),
  DEVICE (TSENG, TSENG_ET6000, "ET6000"),
  DEVICE (WEITEK, WEITEK_P9000, "P9000"),
  DEVICE (WEITEK, WEITEK_P9100, "P9100"),
  BRIDGE (DEC, DEC_BRD, "DC21050", 0x00),
  DEVICE (DEC, DEC_TULIP, "DC21040"),
  DEVICE (DEC, DEC_TGA, "DC21030 (TGA)"),
  DEVICE (DEC, DEC_TULIP_FAST, "DC21140"),
  DEVICE (DEC, DEC_TGA2, "TGA2"),
  DEVICE (DEC, DEC_FDDI, "DEFPA"),
  DEVICE (DEC, DEC_TULIP_PLUS, "DC21041"),
  DEVICE (DEC, DEC_21142, "DC21142"),
  DEVICE (DEC, DEC_21052, "DC21052"),
  DEVICE (DEC, DEC_21150, "DC21150"),
  DEVICE (DEC, DEC_21152, "DC21152"),
  DEVICE (DEC, DEC_21153, "DC21153"),
  DEVICE (DEC, DEC_21154, "DC21154"),
  DEVICE (DEC, DEC_21285, "DC21285"),
  DEVICE (CIRRUS, CIRRUS_7548, "GD 7548"),
  DEVICE (CIRRUS, CIRRUS_5430, "GD 5430"),
  DEVICE (CIRRUS, CIRRUS_5434_4, "GD 5434"),
  DEVICE (CIRRUS, CIRRUS_5434_8, "GD 5434"),
  DEVICE (CIRRUS, CIRRUS_5436, "GD 5436"),
  DEVICE (CIRRUS, CIRRUS_5446, "GD 5446"),
  DEVICE (CIRRUS, CIRRUS_5480, "GD 5480"),
  DEVICE (CIRRUS, CIRRUS_5464, "GD 5464"),
  DEVICE (CIRRUS, CIRRUS_5465, "GD 5465"),
  DEVICE (CIRRUS, CIRRUS_6729, "CL 6729"),
  DEVICE (CIRRUS, CIRRUS_6832, "PD 6832"),
  DEVICE (CIRRUS, CIRRUS_7542, "CL 7542"),
  DEVICE (CIRRUS, CIRRUS_7543, "CL 7543"),
  DEVICE (CIRRUS, CIRRUS_7541, "CL 7541"),
  DEVICE (IBM, IBM_FIRE_CORAL, "Fire Coral"),
  DEVICE (IBM, IBM_TR, "Token Ring"),
  DEVICE (IBM, IBM_82G2675, "82G2675"),
  DEVICE (IBM, IBM_MCA, "MicroChannel"),
  DEVICE (IBM, IBM_82351, "82351"),
  DEVICE (IBM, IBM_PYTHON, "Python"),
  DEVICE (IBM, IBM_SERVERAID, "ServeRAID"),
  DEVICE (IBM, IBM_TR_WAKE, "Wake On LAN Token Ring"),
  DEVICE (IBM, IBM_MPIC, "mPIC"),
  DEVICE (IBM, IBM_3780IDSP, "MWave DSP"),
  DEVICE (IBM, IBM_MPIC, "mPIC-2"),
  DEVICE (WD, WD_7197, "WD 7197"),
  DEVICE (AMD, AMD_LANCE, "79C970"),
  DEVICE (AMD, AMD_LANCE_HOME, "79C970H?"),
  DEVICE (AMD, AMD_SCSI, "53C974"),
  DEVICE (TRIDENT, TRIDENT_9397, "Cyber9397"),
  DEVICE (TRIDENT, TRIDENT_9420, "TG 9420"),
  DEVICE (TRIDENT, TRIDENT_9440, "TG 9440"),
  DEVICE (TRIDENT, TRIDENT_9660, "TG 9660 / Cyber9385"),
  DEVICE (TRIDENT, TRIDENT_9750, "Image 975"),
  DEVICE (AI, AI_M1435, "M1435"),
  DEVICE (MATROX, MATROX_MGA_2, "Atlas PX2085"),
  DEVICE (MATROX, MATROX_MIL, "Millennium"),
  DEVICE (MATROX, MATROX_MYS, "Mystique"),
  DEVICE (MATROX, MATROX_MIL_2, "Millennium II"),
  DEVICE (MATROX, MATROX_MIL_2_AGP, "Millennium II AGP"),
  DEVICE (MATROX, MATROX_G200_PCI, "G200"),
  DEVICE (MATROX, MATROX_G200_AGP, "G200 AGP"),
  DEVICE (MATROX, MATROX_MGA_IMP, "MGA Impression"),
  DEVICE (MATROX, MATROX_G100_MM, "G100 MMedia?"),
  DEVICE (MATROX, MATROX_G100_AGP, "G100 AGP"),
  DEVICE (CT, CT_65545, "65545"),
  DEVICE (CT, CT_65548, "65548"),
  DEVICE (CT, CT_65550, "65550"),
  DEVICE (CT, CT_65554, "65554"),
  DEVICE (CT, CT_65555, "65555"),
  DEVICE (MIRO, MIRO_36050, "ZR36050"),
  DEVICE (NEC, NEC_PCX2, "PowerVR PCX2"),
  DEVICE (FD, FD_36C70, "TMC-18C30"),
  DEVICE (SI, SI_5591_AGP, "5591/5592 AGP"),
  DEVICE (SI, SI_6201, "6201"),
  DEVICE (SI, SI_6202, "6202"),
  DEVICE (SI, SI_503, "85C503"),
  DEVICE (SI, SI_ACPI, "ACPI"),
  DEVICE (SI, SI_5597_VGA, "5597/5598 VGA"),
  DEVICE (SI, SI_6205, "6205"),
  DEVICE (SI, SI_501, "85C501"),
  DEVICE (SI, SI_496, "85C496"),
  DEVICE (SI, SI_601, "85C601"),
  DEVICE (SI, SI_5107, "5107"),
  DEVICE (SI, SI_5511, "85C5511"),
  DEVICE (SI, SI_5513, "85C5513"),
  DEVICE (SI, SI_5571, "5571"),
  DEVICE (SI, SI_5591, "5591/5592 Host"),
  DEVICE (SI, SI_5597, "5597/5598 Host"),
  DEVICE (SI, SI_7001, "7001 USB"),
  DEVICE (HP, HP_J2585A, "J2585A"),
  DEVICE (HP, HP_J2585B, "J2585B (Lassen)"),
  DEVICE (PCTECH, PCTECH_RZ1000, "RZ1000 (buggy)"),
  DEVICE (PCTECH, PCTECH_RZ1001, "RZ1001 (buggy?)"),
  DEVICE (PCTECH, PCTECH_SAMURAI_0, "Samurai 0"),
  DEVICE (PCTECH, PCTECH_SAMURAI_1, "Samurai 1"),
  DEVICE (PCTECH, PCTECH_SAMURAI_IDE, "Samurai IDE"),
  DEVICE (DPT, DPT, "SmartCache/Raid"),
  DEVICE (OPTI, OPTI_92C178, "92C178"),
  DEVICE (OPTI, OPTI_82C557, "82C557 Viper-M"),
  DEVICE (OPTI, OPTI_82C558, "82C558 Viper-M ISA+IDE"),
  DEVICE (OPTI, OPTI_82C621, "82C621"),
  DEVICE (OPTI, OPTI_82C700, "82C700"),
  DEVICE (OPTI, OPTI_82C701, "82C701 FireStar Plus"),
  DEVICE (OPTI, OPTI_82C814, "82C814 Firebridge 1"),
  DEVICE (OPTI, OPTI_82C822, "82C822"),
  DEVICE (OPTI, OPTI_82C861, "82C861"),
  DEVICE (OPTI, OPTI_82C825, "82C825 Firebridge 2"),
  DEVICE (SGS, SGS_2000, "STG 2000X"),
  DEVICE (SGS, SGS_1764, "STG 1764X"),
  DEVICE (BUSLOGIC, BUSLOGIC_MULTIMASTER_NC, "MultiMaster NC"),
  DEVICE (BUSLOGIC, BUSLOGIC_MULTIMASTER, "MultiMaster"),
  DEVICE (BUSLOGIC, BUSLOGIC_FLASHPOINT, "FlashPoint"),
  DEVICE (TI, TI_TVP4010, "TVP4010 Permedia"),
  DEVICE (TI, TI_TVP4020, "TVP4020 Permedia 2"),
  DEVICE (TI, TI_PCI1130, "PCI1130"),
  DEVICE (TI, TI_PCI1131, "PCI1131"),
  DEVICE (TI, TI_PCI1250, "PCI1250"),
  DEVICE (OAK, OAK_OTI107, "OTI107"),
  DEVICE (WINBOND2, WINBOND2_89C940, "NE2000-PCI"),
  DEVICE (MOTOROLA, MOTOROLA_MPC105, "MPC105 Eagle"),
  DEVICE (MOTOROLA, MOTOROLA_MPC106, "MPC106 Grackle"),
  DEVICE (MOTOROLA, MOTOROLA_RAVEN, "Raven"),
  DEVICE (PROMISE, PROMISE_20246, "IDE UltraDMA/33"),
  DEVICE (PROMISE, PROMISE_5300, "DC5030"),
  DEVICE (N9, N9_I128, "Imagine 128"),
  DEVICE (N9, N9_I128_2, "Imagine 128v2"),
  DEVICE (UMC, UMC_UM8673F, "UM8673F"),
  BRIDGE (UMC, UMC_UM8891A, "UM8891A", 0x01),
  DEVICE (UMC, UMC_UM8886BF, "UM8886BF"),
  DEVICE (UMC, UMC_UM8886A, "UM8886A"),
  BRIDGE (UMC, UMC_UM8881F, "UM8881F", 0x02),
  DEVICE (UMC, UMC_UM8886F, "UM8886F"),
  DEVICE (UMC, UMC_UM9017F, "UM9017F"),
  DEVICE (UMC, UMC_UM8886N, "UM8886N"),
  DEVICE (UMC, UMC_UM8891N, "UM8891N"),
  DEVICE (X, X_AGX016, "ITT AGX016"),
  DEVICE (PICOP, PICOP_PT86C52X, "PT86C52x Vesuvius"),
  DEVICE (PICOP, PICOP_PT80C524, "PT80C524 Nile"),
  DEVICE (APPLE, APPLE_BANDIT, "Bandit"),
  DEVICE (APPLE, APPLE_GC, "Grand Central"),
  DEVICE (APPLE, APPLE_HYDRA, "Hydra"),
  DEVICE (NEXGEN, NEXGEN_82C501, "82C501"),
  DEVICE (QLOGIC, QLOGIC_ISP1020, "ISP1020"),
  DEVICE (QLOGIC, QLOGIC_ISP1022, "ISP1022"),
  DEVICE (CYRIX, CYRIX_5510, "5510"),
  DEVICE (CYRIX, CYRIX_PCI_MASTER, "PCI Master"),
  DEVICE (CYRIX, CYRIX_5520, "5520"),
  DEVICE (CYRIX, CYRIX_5530_LEGACY, "5530 Kahlua Legacy"),
  DEVICE (CYRIX, CYRIX_5530_SMI, "5530 Kahlua SMI"),
  DEVICE (CYRIX, CYRIX_5530_IDE, "5530 Kahlua IDE"),
  DEVICE (CYRIX, CYRIX_5530_AUDIO, "5530 Kahlua Audio"),
  DEVICE (CYRIX, CYRIX_5530_VIDEO, "5530 Kahlua Video"),
  DEVICE (LEADTEK, LEADTEK_805, "S3 805"),
  DEVICE (CONTAQ, CONTAQ_82C599, "82C599"),
  DEVICE (CONTAQ, CONTAQ_82C693, "82C693"),
  DEVICE (OLICOM, OLICOM_OC3136, "OC-3136/3137"),
  DEVICE (OLICOM, OLICOM_OC2315, "OC-2315"),
  DEVICE (OLICOM, OLICOM_OC2325, "OC-2325"),
  DEVICE (OLICOM, OLICOM_OC2183, "OC-2183/2185"),
  DEVICE (OLICOM, OLICOM_OC2326, "OC-2326"),
  DEVICE (OLICOM, OLICOM_OC6151, "OC-6151/6152"),
  DEVICE (SUN, SUN_EBUS, "EBUS"),
  DEVICE (SUN, SUN_HAPPYMEAL, "Happy Meal Ethernet"),
  DEVICE (SUN, SUN_SIMBA, "Advanced PCI Bridge"),
  DEVICE (SUN, SUN_PBM, "PCI Bus Module"),
  DEVICE (SUN, SUN_SABRE, "Ultra IIi PCI"),
  DEVICE (CMD, CMD_640, "640 (buggy)"),
  DEVICE (CMD, CMD_643, "643"),
  DEVICE (CMD, CMD_646, "646"),
  DEVICE (CMD, CMD_670, "670"),
  DEVICE (VISION, VISION_QD8500, "QD-8500"),
  DEVICE (VISION, VISION_QD8580, "QD-8580"),
  DEVICE (BROOKTREE, BROOKTREE_848, "Bt848"),
  DEVICE (BROOKTREE, BROOKTREE_849A, "Bt849"),
  DEVICE (BROOKTREE, BROOKTREE_8474, "Bt8474"),
  DEVICE (SIERRA, SIERRA_STB, "STB Horizon 64"),
  DEVICE (ACC, ACC_2056, "2056"),
  DEVICE (WINBOND, WINBOND_83769, "W83769F"),
  DEVICE (WINBOND, WINBOND_82C105, "SL82C105"),
  DEVICE (WINBOND, WINBOND_83C553, "W83C553"),
  DEVICE (DATABOOK, DATABOOK_87144, "DB87144"),
//DEVICE (PLX, PLX_SPCOM200, "SPCom 200 PCI serial I/O"),
  DEVICE (PLX, PLX_9050, "PLX9050 PCI <-> IOBus Bridge"),
  DEVICE (PLX, PLX_9080, "PCI9080 I2O"),
  DEVICE (MADGE, MADGE_MK2, "Smart 16/4 BM Mk2 Ringnode"),
  DEVICE (3COM, 3COM_3C339, "3C339 TokenRing"),
  DEVICE (3COM, 3COM_3C590, "3C590 10bT"),
  DEVICE (3COM, 3COM_3C595TX, "3C595 100bTX"),
  DEVICE (3COM, 3COM_3C595T4, "3C595 100bT4"),
  DEVICE (3COM, 3COM_3C595MII, "3C595 100b-MII"),
  DEVICE (3COM, 3COM_3C900TPO, "3C900 10bTPO"),
  DEVICE (3COM, 3COM_3C900COMBO, "3C900 10b Combo"),
  DEVICE (3COM, 3COM_3C905TX, "3C905 100bTX"),
  DEVICE (3COM, 3COM_3C905T4, "3C905 100bT4"),
  DEVICE (3COM, 3COM_3C905B_TX, "3C905B 100bTX"),
  DEVICE (SMC, SMC_EPIC100, "9432 TX"),
  DEVICE (AL, AL_M1445, "M1445"),
  DEVICE (AL, AL_M1449, "M1449"),
  DEVICE (AL, AL_M1451, "M1451"),
  DEVICE (AL, AL_M1461, "M1461"),
  DEVICE (AL, AL_M1489, "M1489"),
  DEVICE (AL, AL_M1511, "M1511"),
  DEVICE (AL, AL_M1513, "M1513"),
  DEVICE (AL, AL_M1521, "M1521"),
  DEVICE (AL, AL_M1523, "M1523"),
  DEVICE (AL, AL_M1531, "M1531 Aladdin IV"),
  DEVICE (AL, AL_M1533, "M1533 Aladdin IV"),
  DEVICE (AL, AL_M3307, "M3307 MPEG-1 decoder"),
  DEVICE (AL, AL_M4803, "M4803"),
  DEVICE (AL, AL_M5219, "M5219"),
  DEVICE (AL, AL_M5229, "M5229 TXpro"),
  DEVICE (AL, AL_M5237, "M5237 USB"),
  DEVICE (SURECOM, SURECOM_NE34, "NE-34PCI LAN"),
  DEVICE (NEOMAGIC, NEOMAGIC_MAGICGRAPH_NM2070, "Magicgraph NM2070"),
  DEVICE (NEOMAGIC, NEOMAGIC_MAGICGRAPH_128V, "MagicGraph 128V"),
  DEVICE (NEOMAGIC, NEOMAGIC_MAGICGRAPH_128ZV, "MagicGraph 128ZV"),
  DEVICE (NEOMAGIC, NEOMAGIC_MAGICGRAPH_128ZVPLUS, "MagicGraph 128ZV+"),
  DEVICE (NEOMAGIC, NEOMAGIC_MAGICGRAPH_NM2160, "MagicGraph NM2160"),
  DEVICE (NEOMAGIC, NEOMAGIC_MAGICMEDIA_256AV, "MagicMedia 256AV"),
  DEVICE (ASP, ASP_ABP940, "ABP940"),
  DEVICE (ASP, ASP_ABP940U, "ABP940U"),
  DEVICE (ASP, ASP_ABP940UW, "ABP940UW"),
  DEVICE (MACRONIX, MACRONIX_MX98713, "MX98713"),
  DEVICE (MACRONIX, MACRONIX_MX987x5, "MX98715 / MX98725"),
  DEVICE (CERN, CERN_SPSB_PMC, "STAR/RD24 SCI-PCI (PMC)"),
  DEVICE (CERN, CERN_SPSB_PCI, "STAR/RD24 SCI-PCI (PMC)"),
  DEVICE (CERN, CERN_HIPPI_DST, "HIPPI destination"),
  DEVICE (CERN, CERN_HIPPI_SRC, "HIPPI source"),
  DEVICE (IMS, IMS_8849, "8849"),
  DEVICE (TEKRAM2, TEKRAM2_690c, "DC690c"),
  DEVICE (TUNDRA, TUNDRA_CA91C042, "CA91C042 Universe"),
  DEVICE (AMCC, AMCC_MYRINET, "Myrinet PCI (M2-PCI-32)"),
  DEVICE (AMCC, AMCC_S5933, "S5933"),
  DEVICE (AMCC, AMCC_S5933_HEPC3, "S5933 Traquair HEPC3"),
  DEVICE (INTERG, INTERG_1680, "IGA-1680"),
  DEVICE (INTERG, INTERG_1682, "IGA-1682"),
  DEVICE (REALTEK, REALTEK_8029, "8029"),
  DEVICE (REALTEK, REALTEK_8129, "8129"),
  DEVICE (REALTEK, REALTEK_8139, "8139"),
  DEVICE (TRUEVISION, TRUEVISION_T1000, "TARGA 1000"),
  DEVICE (INIT, INIT_320P, "320 P"),
  DEVICE (INIT, INIT_360P, "360 P"),
  DEVICE (VIA, VIA_82C505, "VT 82C505"),
  DEVICE (VIA, VIA_82C561, "VT 82C561"),
  DEVICE (VIA, VIA_82C586_1, "VT 82C586 Apollo IDE"),
  DEVICE (VIA, VIA_82C576, "VT 82C576 3V"),
  DEVICE (VIA, VIA_82C585, "VT 82C585 Apollo VP1/VPX"),
  DEVICE (VIA, VIA_82C586_0, "VT 82C586 Apollo ISA"),
  DEVICE (VIA, VIA_82C595, "VT 82C595 Apollo VP2"),
  DEVICE (VIA, VIA_82C597_0, "VT 82C597 Apollo VP3"),
  DEVICE (VIA, VIA_82C926, "VT 82C926 Amazon"),
  DEVICE (VIA, VIA_82C416, "VT 82C416MV"),
  DEVICE (VIA, VIA_82C595_97, "VT 82C595 Apollo VP2/97"),
  DEVICE (VIA, VIA_82C586_2, "VT 82C586 Apollo USB"),
  DEVICE (VIA, VIA_82C586_3, "VT 82C586B Apollo ACPI"),
  DEVICE (VIA, VIA_86C100A, "VT 86C100A"),
  DEVICE (VIA, VIA_82C597_1, "VT 82C597 Apollo VP3 AGP"),
  DEVICE (VIA, VIA_82C598_1, "VT 82C598 Apollo VP3 AGP"),
  DEVICE (VORTEX, VORTEX_GDT60x0, "GDT 60x0"),
  DEVICE (VORTEX, VORTEX_GDT6000B, "GDT 6000b"),
  DEVICE (VORTEX, VORTEX_GDT6x10, "GDT 6110/6510"),
  DEVICE (VORTEX, VORTEX_GDT6x20, "GDT 6120/6520"),
  DEVICE (VORTEX, VORTEX_GDT6530, "GDT 6530"),
  DEVICE (VORTEX, VORTEX_GDT6550, "GDT 6550"),
  DEVICE (VORTEX, VORTEX_GDT6x17, "GDT 6117/6517"),
  DEVICE (VORTEX, VORTEX_GDT6x27, "GDT 6127/6527"),
  DEVICE (VORTEX, VORTEX_GDT6537, "GDT 6537"),
  DEVICE (VORTEX, VORTEX_GDT6557, "GDT 6557"),
  DEVICE (VORTEX, VORTEX_GDT6x15, "GDT 6115/6515"),
  DEVICE (VORTEX, VORTEX_GDT6x25, "GDT 6125/6525"),
  DEVICE (VORTEX, VORTEX_GDT6535, "GDT 6535"),
  DEVICE (VORTEX, VORTEX_GDT6555, "GDT 6555"),
  DEVICE (VORTEX, VORTEX_GDT6x17RP, "GDT 6117RP/6517RP"),
  DEVICE (VORTEX, VORTEX_GDT6x27RP, "GDT 6127RP/6527RP"),
  DEVICE (VORTEX, VORTEX_GDT6537RP, "GDT 6537RP"),
  DEVICE (VORTEX, VORTEX_GDT6557RP, "GDT 6557RP"),
  DEVICE (VORTEX, VORTEX_GDT6x11RP, "GDT 6111RP/6511RP"),
  DEVICE (VORTEX, VORTEX_GDT6x21RP, "GDT 6121RP/6521RP"),
  DEVICE (VORTEX, VORTEX_GDT6x17RP1, "GDT 6117RP1/6517RP1"),
  DEVICE (VORTEX, VORTEX_GDT6x27RP1, "GDT 6127RP1/6527RP1"),
  DEVICE (VORTEX, VORTEX_GDT6537RP1, "GDT 6537RP1"),
  DEVICE (VORTEX, VORTEX_GDT6557RP1, "GDT 6557RP1"),
  DEVICE (VORTEX, VORTEX_GDT6x11RP1, "GDT 6111RP1/6511RP1"),
  DEVICE (VORTEX, VORTEX_GDT6x21RP1, "GDT 6121RP1/6521RP1"),
  DEVICE (VORTEX, VORTEX_GDT6x17RP2, "GDT 6117RP2/6517RP2"),
  DEVICE (VORTEX, VORTEX_GDT6x27RP2, "GDT 6127RP2/6527RP2"),
  DEVICE (VORTEX, VORTEX_GDT6537RP2, "GDT 6537RP2"),
  DEVICE (VORTEX, VORTEX_GDT6557RP2, "GDT 6557RP2"),
  DEVICE (VORTEX, VORTEX_GDT6x11RP2, "GDT 6111RP2/6511RP2"),
  DEVICE (VORTEX, VORTEX_GDT6x21RP2, "GDT 6121RP2/6521RP2"),
  DEVICE (EF, EF_ATM_FPGA, "155P-MF1 (FPGA)"),
  DEVICE (EF, EF_ATM_ASIC, "155P-MF1 (ASIC)"),
  DEVICE (FORE, FORE_PCA200PC, "PCA-200PC"),
  DEVICE (FORE, FORE_PCA200E, "PCA-200E"),
  DEVICE (IMAGINGTECH, IMAGINGTECH_ICPCI, "MVC IC-PCI"),
  DEVICE (PHILIPS, PHILIPS_SAA7146, "SAA7146"),
  DEVICE (CYCLONE, CYCLONE_SDK, "SDK"),
  DEVICE (ALLIANCE, ALLIANCE_PROMOTIO, "Promotion-6410"),
  DEVICE (ALLIANCE, ALLIANCE_PROVIDEO, "Provideo"),
  DEVICE (ALLIANCE, ALLIANCE_AT24, "AT24"),
  DEVICE (ALLIANCE, ALLIANCE_AT3D, "AT3D"),
  DEVICE (VMIC, VMIC_VME, "VMIVME-7587"),
  DEVICE (DIGI, DIGI_EPC, "AccelPort EPC"),
  DEVICE (DIGI, DIGI_RIGHTSWITCH, "RightSwitch SE-6"),
  DEVICE (DIGI, DIGI_XEM, "AccelPort Xem"),
  DEVICE (DIGI, DIGI_XR, "AccelPort Xr"),
  DEVICE (DIGI, DIGI_CX, "AccelPort C/X"),
  DEVICE (DIGI, DIGI_XRJ, "AccelPort Xr/J"),
  DEVICE (DIGI, DIGI_EPCJ, "AccelPort EPC/J"),
  DEVICE (DIGI, DIGI_XR_920, "AccelPort Xr 920"),
  DEVICE (MUTECH, MUTECH_MV1000, "MV-1000"),
  DEVICE (RENDITION, RENDITION_VERITE, "Verite 1000"),
  DEVICE (RENDITION, RENDITION_VERITE2100, "Verite 2100"),
  DEVICE (TOSHIBA, TOSHIBA_601, "Laptop"),
  DEVICE (TOSHIBA, TOSHIBA_TOPIC95, "ToPIC95"),
  DEVICE (TOSHIBA, TOSHIBA_TOPIC97, "ToPIC97"),
  DEVICE (RICOH, RICOH_RL5C466, "RL5C466"),
  DEVICE (ARTOP, ARTOP_ATP850UF, "ATP850UF"),
  DEVICE (ZEITNET, ZEITNET_1221, "1221"),
  DEVICE (ZEITNET, ZEITNET_1225, "1225"),
  DEVICE (OMEGA, OMEGA_82C092G, "82C092G"),
  DEVICE (LITEON, LITEON_LNE100TX, "LNE100TX"),
  DEVICE (NP, NP_PCI_FDDI, "NP-PCI"),
  DEVICE (ATT, ATT_L56XMF, "L56xMF"),
  DEVICE (SPECIALIX, SPECIALIX_IO8, "IO8+/PCI"),
  DEVICE (SPECIALIX, SPECIALIX_XIO, "XIO/SIO host"),
  DEVICE (SPECIALIX, SPECIALIX_RIO, "RIO host"),
  DEVICE (AURAVISION, AURAVISION_VXP524, "VXP524"),
  DEVICE (IKON, IKON_10115, "10115 Greensheet"),
  DEVICE (IKON, IKON_10117, "10117 Greensheet"),
  DEVICE (ZORAN, ZORAN_36057, "ZR36057"),
  DEVICE (ZORAN, ZORAN_36120, "ZR36120"),
  DEVICE (KINETIC, KINETIC_2915, "2915 CAMAC"),
  DEVICE (COMPEX, COMPEX_ENET100VG4, "Readylink ENET100-VG4"),
  DEVICE (COMPEX, COMPEX_RL2000, "ReadyLink 2000"),
  DEVICE (RP, RP8OCTA, "RocketPort 8 Oct"),
  DEVICE (RP, RP8INTF, "RocketPort 8 Intf"),
  DEVICE (RP, RP16INTF, "RocketPort 16 Intf"),
  DEVICE (RP, RP32INTF, "RocketPort 32 Intf"),
  DEVICE (CYCLADES, CYCLOM_Y_Lo, "Cyclom-Y below 1Mbyte"),
  DEVICE (CYCLADES, CYCLOM_Y_Hi, "Cyclom-Y above 1Mbyte"),
  DEVICE (CYCLADES, CYCLOM_Z_Lo, "Cyclom-Z below 1Mbyte"),
  DEVICE (CYCLADES, CYCLOM_Z_Hi, "Cyclom-Z above 1Mbyte"),
  DEVICE (ESSENTIAL, ESSENTIAL_ROADRUNNER, "Roadrunner serial HIPPI"),
  DEVICE (O2, O2_6832, "6832"),
  DEVICE (3DFX, 3DFX_VOODOO, "Voodoo"),
  DEVICE (3DFX, 3DFX_VOODOO2, "Voodoo2"),
  DEVICE (3DFX, 3DFX_VOODOO3, "Voodoo3"),
  DEVICE (SIGMADES, SIGMADES_6425, "REALmagic64/GX"),
  DEVICE (STALLION, STALLION_ECHPCI832, "EasyConnection 8/32"),
  DEVICE (STALLION, STALLION_ECHPCI864, "EasyConnection 8/64"),
  DEVICE (STALLION, STALLION_EIOPCI, "EasyIO"),
  DEVICE (OPTIBASE, OPTIBASE_FORGE, "MPEG Forge"),
  DEVICE (OPTIBASE, OPTIBASE_FUSION, "MPEG Fusion"),
  DEVICE (OPTIBASE, OPTIBASE_VPLEX, "VideoPlex"),
  DEVICE (OPTIBASE, OPTIBASE_VPLEXCC, "VideoPlex CC"),
  DEVICE (OPTIBASE, OPTIBASE_VQUEST, "VideoQuest"),
//DEVICE (ASIX, ASIX_88140, "88140"),
  DEVICE (SATSAGEM, SATSAGEM_PCR2101, "PCR2101 DVB receiver"),
  DEVICE (SATSAGEM, SATSAGEM_TELSATTURBO, "Telsat Turbo DVB"),
  DEVICE (ENSONIQ, ENSONIQ_AUDIOPCI, "AudioPCI"),
  DEVICE (PICTUREL, PICTUREL_PCIVST, "PCIVST"),
  DEVICE (NVIDIA_SGS, NVIDIA_SGS_RIVA128, "Riva 128"),
  DEVICE (CBOARDS, CBOARDS_DAS1602_16, "DAS1602/16"),
  DEVICE (SYMPHONY, SYMPHONY_101, "82C101"),
  DEVICE (TEKRAM, TEKRAM_DC290, "DC-290"),
  DEVICE (3DLABS, 3DLABS_300SX, "GLINT 300SX"),
  DEVICE (3DLABS, 3DLABS_500TX, "GLINT 500TX"),
  DEVICE (3DLABS, 3DLABS_DELTA, "GLINT Delta"),
  DEVICE (3DLABS, 3DLABS_PERMEDIA, "PERMEDIA"),
  DEVICE (3DLABS, 3DLABS_MX, "GLINT MX"),
  DEVICE (AVANCE, AVANCE_ALG2064, "ALG2064i"),
  DEVICE (AVANCE, AVANCE_2302, "ALG-2302"),
  DEVICE (NETVIN, NETVIN_NV5000SC, "NV5000"),
  DEVICE (S3, S3_PLATO_PXS, "PLATO/PX (system)"),
  DEVICE (S3, S3_ViRGE, "ViRGE"),
  DEVICE (S3, S3_TRIO, "Trio32/Trio64"),
  DEVICE (S3, S3_AURORA64VP, "Aurora64V+"),
  DEVICE (S3, S3_TRIO64UVP, "Trio64UV+"),
  DEVICE (S3, S3_ViRGE_VX, "ViRGE/VX"),
  DEVICE (S3, S3_868, "Vision 868"),
  DEVICE (S3, S3_928, "Vision 928-P"),
  DEVICE (S3, S3_864_1, "Vision 864-P"),
  DEVICE (S3, S3_864_2, "Vision 864-P"),
  DEVICE (S3, S3_964_1, "Vision 964-P"),
  DEVICE (S3, S3_964_2, "Vision 964-P"),
  DEVICE (S3, S3_968, "Vision 968"),
  DEVICE (S3, S3_TRIO64V2, "Trio64V2/DX or /GX"),
  DEVICE (S3, S3_PLATO_PXG, "PLATO/PX (graphics)"),
  DEVICE (S3, S3_ViRGE_DXGX, "ViRGE/DX or /GX"),
  DEVICE (S3, S3_ViRGE_GX2, "ViRGE/GX2"),
  DEVICE (S3, S3_ViRGE_MX, "ViRGE/MX"),
  DEVICE (S3, S3_ViRGE_MXP, "ViRGE/MX+"),
  DEVICE (S3, S3_ViRGE_MXPMV, "ViRGE/MX+MV"),
  DEVICE (S3, S3_SONICVIBES, "SonicVibes"),
  DEVICE (INTEL, INTEL_82375, "82375EB"),
  BRIDGE (INTEL, INTEL_82424, "82424ZX Saturn", 0x00),
  DEVICE (INTEL, INTEL_82378, "82378IB"),
  DEVICE (INTEL, INTEL_82430, "82430ZX Aries"),
  BRIDGE (INTEL, INTEL_82434, "82434LX Mercury/Neptune", 0x00),
  DEVICE (INTEL, INTEL_82092AA_0, "82092AA PCMCIA bridge"),
  DEVICE (INTEL, INTEL_82092AA_1, "82092AA EIDE"),
  DEVICE (INTEL, INTEL_7116, "SAA7116"),
  DEVICE (INTEL, INTEL_82596, "82596"),
  DEVICE (INTEL, INTEL_82865, "82865"),
  DEVICE (INTEL, INTEL_82557, "82557"),
  DEVICE (INTEL, INTEL_82437, "82437"),
//DEVICE (INTEL, INTEL_82371_0, "82371 Triton PIIX"),
//DEVICE (INTEL, INTEL_82371_1, "82371 Triton PIIX"),
  DEVICE (INTEL, INTEL_82371MX, "430MX - 82371MX MPIIX"),
  DEVICE (INTEL, INTEL_82437MX, "430MX - 82437MX MTSC"),
  DEVICE (INTEL, INTEL_82441, "82441FX Natoma"),
  DEVICE (INTEL, INTEL_82380FB, "82380FB Mobile"),
  DEVICE (INTEL, INTEL_82439, "82439HX Triton II"),
  DEVICE (INTEL, INTEL_82371SB_0, "82371SB PIIX3 ISA"),
  DEVICE (INTEL, INTEL_82371SB_1, "82371SB PIIX3 IDE"),
  DEVICE (INTEL, INTEL_82371SB_2, "82371SB PIIX3 USB"),
  DEVICE (INTEL, INTEL_82437VX, "82437VX Triton II"),
  DEVICE (INTEL, INTEL_82439TX, "82439TX"),
  DEVICE (INTEL, INTEL_82371AB_0, "82371AB PIIX4 ISA"),
  DEVICE (INTEL, INTEL_82371AB, "82371AB PIIX4 IDE"),
  DEVICE (INTEL, INTEL_82371AB_2, "82371AB PIIX4 USB"),
  DEVICE (INTEL, INTEL_82371AB_3, "82371AB PIIX4 ACPI"),
  DEVICE (INTEL, INTEL_82443LX_0, "440LX - 82443LX PAC Host"),
  DEVICE (INTEL, INTEL_82443LX_1, "440LX - 82443LX PAC AGP"),
  DEVICE (INTEL, INTEL_82443BX_0, "440BX - 82443BX Host"),
  DEVICE (INTEL, INTEL_82443BX_1, "440BX - 82443BX AGP"),
  DEVICE (INTEL, INTEL_82443BX_2, "440BX - 82443BX Host (no AGP)"),
//DEVICE (INTEL, INTEL_82443GX_0, "440GX - 82443GX Host"),
//DEVICE (INTEL, INTEL_82443GX_1, "440GX - 82443GX AGP"),
//DEVICE (INTEL, INTEL_82443GX_2, "440GX - 82443GX Host (no AGP)"),
  DEVICE (INTEL, INTEL_P6, "Orion P6"),
  DEVICE (INTEL, INTEL_82450GX, "82450GX Orion P6"),
  DEVICE (KTI, KTI_ET32P2, "ET32P2"),
  DEVICE (ADAPTEC, ADAPTEC_7810, "AIC-7810 RAID"),
  DEVICE (ADAPTEC, ADAPTEC_7850, "AIC-7850"),
  DEVICE (ADAPTEC, ADAPTEC_7855, "AIC-7855"),
  DEVICE (ADAPTEC, ADAPTEC_5800, "AIC-5800"),
  DEVICE (ADAPTEC, ADAPTEC_7860, "AIC-7860"),
  DEVICE (ADAPTEC, ADAPTEC_7861, "AIC-7861"),
  DEVICE (ADAPTEC, ADAPTEC_7870, "AIC-7870"),
  DEVICE (ADAPTEC, ADAPTEC_7871, "AIC-7871"),
  DEVICE (ADAPTEC, ADAPTEC_7872, "AIC-7872"),
  DEVICE (ADAPTEC, ADAPTEC_7873, "AIC-7873"),
  DEVICE (ADAPTEC, ADAPTEC_7874, "AIC-7874"),
  DEVICE (ADAPTEC, ADAPTEC_7895, "AIC-7895U"),
  DEVICE (ADAPTEC, ADAPTEC_7880, "AIC-7880U"),
  DEVICE (ADAPTEC, ADAPTEC_7881, "AIC-7881U"),
  DEVICE (ADAPTEC, ADAPTEC_7882, "AIC-7882U"),
  DEVICE (ADAPTEC, ADAPTEC_7883, "AIC-7883U"),
  DEVICE (ADAPTEC, ADAPTEC_7884, "AIC-7884U"),
  DEVICE (ADAPTEC, ADAPTEC_1030, "ABA-1030 DVB receiver"),
  DEVICE (ADAPTEC2, ADAPTEC2_2940U2, "AHA-2940U2"),
  DEVICE (ADAPTEC2, ADAPTEC2_7890, "AIC-7890/1"),
  DEVICE (ADAPTEC2, ADAPTEC2_3940U2, "AHA-3940U2"),
  DEVICE (ADAPTEC2, ADAPTEC2_7896, "AIC-7896/7"),
  DEVICE (ATRONICS, ATRONICS_2015, "IDE-2015PL"),
  DEVICE (TIGERJET, TIGERJET_300, "Tiger300 ISDN"),
  DEVICE (ARK, ARK_STING, "Stingray"),
  DEVICE (ARK, ARK_STINGARK, "Stingray ARK 2000PV"),
  DEVICE (ARK, ARK_2000MT, "2000MT")
};


/*
 * device_info[] is sorted so we can use binary search
 */
struct pci_dev_info *pci_lookup_dev (unsigned vendor, unsigned dev)
{
  int min = 0, max = DIM(dev_info) - 1;

  for (;;)
  {
    int  i     = (min + max) >> 1;
    long order = dev_info[i].vendor - (long)vendor;

    if (!order)
       order = dev_info[i].device - (long)dev;

    if (order < 0)
    {
      min = i + 1;
      if (min > max)
         return (0);
      continue;
    }

    if (order > 0)
    {
      max = i - 1;
      if (min > max)
         return (0);
      continue;
    }
    return (&dev_info[i]);
  }
}

const char *pci_strdev (unsigned vendor, unsigned device)
{
  struct pci_dev_info *info = pci_lookup_dev (vendor, device);
  return (info ? info->name : "Unknown device");
}

const char *pci_strvendor (unsigned vendor)
{
  switch (vendor)
  {
    case PCI_VENDOR_ID_COMPAQ:
         return "Compaq";
    case PCI_VENDOR_ID_NCR:
         return "NCR";
    case PCI_VENDOR_ID_ATI:
         return "ATI";
    case PCI_VENDOR_ID_VLSI:
         return "VLSI";
    case PCI_VENDOR_ID_ADL:
         return "Advance Logic";
    case PCI_VENDOR_ID_NS:
         return "NS";
    case PCI_VENDOR_ID_TSENG:
         return "Tseng'Lab";
    case PCI_VENDOR_ID_WEITEK:
         return "Weitek";
    case PCI_VENDOR_ID_DEC:
         return "DEC";
    case PCI_VENDOR_ID_CIRRUS:
         return "Cirrus Logic";
    case PCI_VENDOR_ID_IBM:
         return "IBM";
    case PCI_VENDOR_ID_WD:
         return "Western Digital";
    case PCI_VENDOR_ID_AMD:
         return "AMD";
    case PCI_VENDOR_ID_TRIDENT:
         return "Trident";
    case PCI_VENDOR_ID_AI:
         return "Acer Incorporated";
    case PCI_VENDOR_ID_MATROX:
         return "Matrox";
    case PCI_VENDOR_ID_CT:
         return "Chips & Technologies";
    case PCI_VENDOR_ID_MIRO:
         return "Miro";
    case PCI_VENDOR_ID_NEC:
         return "NEC";
    case PCI_VENDOR_ID_FD:
         return "Future Domain";
    case PCI_VENDOR_ID_SI:
         return "Silicon Integrated Systems";
    case PCI_VENDOR_ID_HP:
         return "Hewlett Packard";
    case PCI_VENDOR_ID_PCTECH:
         return "PCTECH";
    case PCI_VENDOR_ID_DPT:
         return "DPT";
    case PCI_VENDOR_ID_OPTI:
         return "OPTi";
    case PCI_VENDOR_ID_SGS:
         return "SGS Thomson";
    case PCI_VENDOR_ID_BUSLOGIC:
         return "BusLogic";
    case PCI_VENDOR_ID_TI:
         return "Texas Instruments";
    case PCI_VENDOR_ID_OAK:
         return "OAK";
    case PCI_VENDOR_ID_WINBOND2:
         return "Winbond";
    case PCI_VENDOR_ID_MOTOROLA:
         return "Motorola";
    case PCI_VENDOR_ID_PROMISE:
         return "Promise Technology";
    case PCI_VENDOR_ID_APPLE:
         return "Apple";
    case PCI_VENDOR_ID_N9:
         return "Number Nine";
    case PCI_VENDOR_ID_UMC:
         return "UMC";
    case PCI_VENDOR_ID_X:
         return "X TECHNOLOGY";
    case PCI_VENDOR_ID_NEXGEN:
         return "Nexgen";
    case PCI_VENDOR_ID_QLOGIC:
         return "Q Logic";
    case PCI_VENDOR_ID_LEADTEK:
         return "Leadtek Research";
    case PCI_VENDOR_ID_CONTAQ:
         return "Contaq";
    case PCI_VENDOR_ID_FOREX:
         return "Forex";
    case PCI_VENDOR_ID_OLICOM:
         return "Olicom";
    case PCI_VENDOR_ID_CMD:
         return "CMD";
    case PCI_VENDOR_ID_VISION:
         return "Vision";
    case PCI_VENDOR_ID_BROOKTREE:
         return "Brooktree";
    case PCI_VENDOR_ID_SIERRA:
         return "Sierra";
    case PCI_VENDOR_ID_ACC:
         return "ACC MICROELECTRONICS";
    case PCI_VENDOR_ID_WINBOND:
         return "Winbond";
    case PCI_VENDOR_ID_DATABOOK:
         return "Databook";
    case PCI_VENDOR_ID_3COM:
         return "3Com";
    case PCI_VENDOR_ID_SMC:
         return "SMC";
    case PCI_VENDOR_ID_AL:
         return "Acer Labs";
    case PCI_VENDOR_ID_MITSUBISHI:
         return "Mitsubishi";
    case PCI_VENDOR_ID_NEOMAGIC:
         return "Neomagic";
    case PCI_VENDOR_ID_ASP:
         return "Advanced System Products";
    case PCI_VENDOR_ID_CERN:
         return "CERN";
    case PCI_VENDOR_ID_IMS:
         return "IMS";
    case PCI_VENDOR_ID_TEKRAM2:
         return "Tekram";
    case PCI_VENDOR_ID_TUNDRA:
         return "Tundra";
    case PCI_VENDOR_ID_AMCC:
         return "AMCC";
    case PCI_VENDOR_ID_INTERG:
         return "Intergraphics";
    case PCI_VENDOR_ID_REALTEK:
         return "Realtek";
    case PCI_VENDOR_ID_TRUEVISION:
         return "Truevision";
    case PCI_VENDOR_ID_INIT:
         return "Initio Corp";
    case PCI_VENDOR_ID_VIA:
         return "VIA Technologies";
    case PCI_VENDOR_ID_VORTEX:
         return "VORTEX";
    case PCI_VENDOR_ID_EF:
         return "Efficient Networks";
    case PCI_VENDOR_ID_FORE:
         return "Fore Systems";
    case PCI_VENDOR_ID_IMAGINGTECH:
         return "Imaging Technology";
    case PCI_VENDOR_ID_PHILIPS:
         return "Philips";
    case PCI_VENDOR_ID_PLX:
         return "PLX";
    case PCI_VENDOR_ID_ALLIANCE:
         return "Alliance";
    case PCI_VENDOR_ID_VMIC:
         return "VMIC";
    case PCI_VENDOR_ID_DIGI:
         return "Digi Intl.";
    case PCI_VENDOR_ID_MUTECH:
         return "Mutech";
    case PCI_VENDOR_ID_RENDITION:
         return "Rendition";
    case PCI_VENDOR_ID_TOSHIBA:
         return "Toshiba";
    case PCI_VENDOR_ID_RICOH:
         return "Ricoh";
    case PCI_VENDOR_ID_ZEITNET:
         return "ZeitNet";
    case PCI_VENDOR_ID_OMEGA:
         return "Omega Micro";
    case PCI_VENDOR_ID_NP:
         return "Network Peripherals";
    case PCI_VENDOR_ID_SPECIALIX:
         return "Specialix";
    case PCI_VENDOR_ID_IKON:
         return "Ikon";
    case PCI_VENDOR_ID_ZORAN:
         return "Zoran";
    case PCI_VENDOR_ID_COMPEX:
         return "Compex";
    case PCI_VENDOR_ID_RP:
         return "Comtrol";
    case PCI_VENDOR_ID_CYCLADES:
         return "Cyclades";
    case PCI_VENDOR_ID_3DFX:
         return "3Dfx";
    case PCI_VENDOR_ID_SIGMADES:
         return "Sigma Designs";
    case PCI_VENDOR_ID_OPTIBASE:
         return "Optibase";
    case PCI_VENDOR_ID_NVIDIA_SGS:
         return "NVidia/SGS Thomson";
    case PCI_VENDOR_ID_ENSONIQ:
         return "Ensoniq";
    case PCI_VENDOR_ID_SYMPHONY:
         return "Symphony";
    case PCI_VENDOR_ID_TEKRAM:
         return "Tekram";
    case PCI_VENDOR_ID_3DLABS:
         return "3Dlabs";
    case PCI_VENDOR_ID_AVANCE:
         return "Avance";
    case PCI_VENDOR_ID_NETVIN:
         return "NetVin";
    case PCI_VENDOR_ID_S3:
         return "S3 Inc.";
    case PCI_VENDOR_ID_INTEL:
         return "Intel";
    case PCI_VENDOR_ID_KTI:
         return "KTI";
    case PCI_VENDOR_ID_ADAPTEC:
         return "Adaptec";
    case PCI_VENDOR_ID_ADAPTEC2:
         return "Adaptec";
    case PCI_VENDOR_ID_ATRONICS:
         return "Atronics";
    case PCI_VENDOR_ID_ARK:
         return "ARK Logic";
#if 0
    case PCI_VENDOR_ID_ASIX:
         return "ASIX";
#endif
    case PCI_VENDOR_ID_LITEON:
         return "Lite-on";
    default:
         return "Unknown vendor";
  }
}

const char *pci_strclass (unsigned class)
{
  switch (class >> 8)
  {
    case PCI_CLASS_NOT_DEFINED:
         return "Non-VGA device";
    case PCI_CLASS_NOT_DEFINED_VGA:
         return "VGA compatible device";

    case PCI_CLASS_STORAGE_SCSI:
         return "SCSI storage controller";
    case PCI_CLASS_STORAGE_IDE:
         return "IDE interface";
    case PCI_CLASS_STORAGE_FLOPPY:
         return "Floppy disk controller";
    case PCI_CLASS_STORAGE_IPI:
         return "IPI bus controller";
    case PCI_CLASS_STORAGE_RAID:
         return "RAID bus controller";
    case PCI_CLASS_STORAGE_OTHER:
         return "Unknown mass storage controller";

    case PCI_CLASS_NETWORK_ETHERNET:
         return "Ethernet controller";
    case PCI_CLASS_NETWORK_TOKEN_RING:
         return "Token ring network controller";
    case PCI_CLASS_NETWORK_FDDI:
         return "FDDI network controller";
    case PCI_CLASS_NETWORK_ATM:
         return "ATM network controller";
    case PCI_CLASS_NETWORK_OTHER:
         return "Network controller";

    case PCI_CLASS_DISPLAY_VGA:
         return "VGA compatible controller";
    case PCI_CLASS_DISPLAY_XGA:
         return "XGA compatible controller";
    case PCI_CLASS_DISPLAY_OTHER:
         return "Display controller";

    case PCI_CLASS_MULTIMEDIA_VIDEO:
         return "Multimedia video controller";
    case PCI_CLASS_MULTIMEDIA_AUDIO:
         return "Multimedia audio controller";
    case PCI_CLASS_MULTIMEDIA_OTHER:
         return "Multimedia controller";

    case PCI_CLASS_MEMORY_RAM:
         return "RAM memory";
    case PCI_CLASS_MEMORY_FLASH:
         return "FLASH memory";
    case PCI_CLASS_MEMORY_OTHER:
         return "Memory";

    case PCI_CLASS_BRIDGE_HOST:
         return "Host bridge";
    case PCI_CLASS_BRIDGE_ISA:
         return "ISA bridge";
    case PCI_CLASS_BRIDGE_EISA:
         return "EISA bridge";
    case PCI_CLASS_BRIDGE_MC:
         return "MicroChannel bridge";
    case PCI_CLASS_BRIDGE_PCI:
         return "PCI bridge";
    case PCI_CLASS_BRIDGE_PCMCIA:
         return "PCMCIA bridge";
    case PCI_CLASS_BRIDGE_NUBUS:
         return "NuBus bridge";
    case PCI_CLASS_BRIDGE_CARDBUS:
         return "CardBus bridge";
    case PCI_CLASS_BRIDGE_OTHER:
         return "Bridge";

    case PCI_CLASS_COMMUNICATION_SERIAL:
         return "Serial controller";
    case PCI_CLASS_COMMUNICATION_PARALLEL:
         return "Parallel controller";
    case PCI_CLASS_COMMUNICATION_OTHER:
         return "Communication controller";

    case PCI_CLASS_SYSTEM_PIC:
         return "PIC";
    case PCI_CLASS_SYSTEM_DMA:
         return "DMA controller";
    case PCI_CLASS_SYSTEM_TIMER:
         return "Timer";
    case PCI_CLASS_SYSTEM_RTC:
         return "RTC";
    case PCI_CLASS_SYSTEM_OTHER:
         return "System peripheral";

    case PCI_CLASS_INPUT_KEYBOARD:
         return "Keyboard controller";
    case PCI_CLASS_INPUT_PEN:
         return "Digitizer Pen";
    case PCI_CLASS_INPUT_MOUSE:
         return "Mouse controller";
    case PCI_CLASS_INPUT_OTHER:
         return "Input device controller";

    case PCI_CLASS_DOCKING_GENERIC:
         return "Generic Docking Station";
    case PCI_CLASS_DOCKING_OTHER:
         return "Docking Station";

    case PCI_CLASS_PROCESSOR_386:
         return "386";
    case PCI_CLASS_PROCESSOR_486:
         return "486";
    case PCI_CLASS_PROCESSOR_PENTIUM:
         return "Pentium";
    case PCI_CLASS_PROCESSOR_ALPHA:
         return "Alpha";
    case PCI_CLASS_PROCESSOR_POWERPC:
         return "Power PC";
    case PCI_CLASS_PROCESSOR_CO:
         return "Co-processor";

    case PCI_CLASS_SERIAL_FIREWIRE:
         return "FireWire (IEEE 1394)";
    case PCI_CLASS_SERIAL_ACCESS:
         return "ACCESS Bus";
    case PCI_CLASS_SERIAL_SSA:
         return "SSA";
    case PCI_CLASS_SERIAL_USB:
         return "USB Controller";
    case PCI_CLASS_SERIAL_FIBER:
         return "Fiber Channel";

    default:
         return "Unknown class";
  }
}

struct pci_dev *pci_find_slot (unsigned bus, unsigned devfn)
{
  struct pci_dev *dev;

  for (dev = pci_devices; dev; dev = dev->next)
      if (dev->bus->number == bus && dev->devfn == devfn)
         break;
  return (dev);
}

struct pci_dev *pci_find_device (unsigned vendor, unsigned device, struct pci_dev *from)
{
  if (!from)
       from = pci_devices;
  else from = from->next;
  while (from && (from->vendor != vendor || from->device != device))
    from = from->next;
  return from;
}

struct pci_dev *pci_find_class (unsigned class, struct pci_dev *from)
{
  if (!from)
       from = pci_devices;
  else from = from->next;
  while (from && from->class != class)
    from = from->next;
  return (from);
}

int pci_read_config_byte (struct pci_dev *dev, BYTE where, BYTE *val)
{
  return pcibios_read_config_byte (dev->bus->number, dev->devfn, where, val);
}

int pci_read_config_word (struct pci_dev *dev, BYTE where, WORD *val)
{
  return pcibios_read_config_word (dev->bus->number, dev->devfn, where, val);
}

int pci_read_config_dword (struct pci_dev *dev, BYTE where, DWORD * val)
{
  return pcibios_read_config_dword (dev->bus->number, dev->devfn, where, val);
}

int pci_write_config_byte (struct pci_dev *dev, BYTE where, BYTE val)
{
  return pcibios_write_config_byte (dev->bus->number, dev->devfn, where, val);
}

int pci_write_config_word (struct pci_dev *dev, BYTE where, WORD val)
{
  return pcibios_write_config_word (dev->bus->number, dev->devfn, where, val);
}

int pci_write_config_dword (struct pci_dev *dev, BYTE where, DWORD val)
{
  return pcibios_write_config_dword (dev->bus->number, dev->devfn, where, val);
}

void pci_set_master (struct pci_dev *dev)
{
  WORD cmd;
  BYTE lat;

  pci_read_config_word (dev, PCI_COMMAND, &cmd);
  if (!(cmd & PCI_COMMAND_MASTER))
  {
    PRINTK (("pci: Enabling bus mastering for device %02X:%02X\n",
             dev->bus->number, dev->devfn));
    cmd |= PCI_COMMAND_MASTER;
    pci_write_config_word (dev, PCI_COMMAND, cmd);
  }
  pci_read_config_byte (dev, PCI_LATENCY_TIMER, &lat);
  if (lat < 16)
  {
    PRINTK (("pci: Increasing latency timer of device %02X:%02X to 64\n",
             dev->bus->number, dev->devfn));
    pci_write_config_byte (dev, PCI_LATENCY_TIMER, 64);
  }
}

/*
 * __initfunc
 */
void pci_read_bases (struct pci_dev *dev, unsigned howmany)
{
  unsigned reg;
  DWORD    l;

  for (reg = 0; reg < howmany; reg++)
  {
    pci_read_config_dword (dev, PCI_BASE_ADDRESS_0 + (reg << 2), &l);
    if (l == 0xffffffff)
       continue;

    dev->base_address[reg] = l;
    if ((l & (PCI_BASE_ADDRESS_SPACE | PCI_BASE_ADDRESS_MEM_TYPE_MASK)) ==
        (PCI_BASE_ADDRESS_SPACE_MEMORY | PCI_BASE_ADDRESS_MEM_TYPE_64))
    {
      reg++;
      pci_read_config_dword (dev, PCI_BASE_ADDRESS_0 + (reg << 2), &l);
      if (l)
         dev->base_address [reg-1] |= (uint64)l << 32;
    }
  }
}  

/*
 * __initfunc
 */
int pci_scan_bus (struct pci_bus *bus)
{
  struct pci_dev *dev, **bus_last;
  struct pci_bus *child;

  unsigned devfn, max;
  BYTE     cmd, irq, tmp, hdr_type, is_multi = 0;
  DWORD    l, class;

  PRINTK (("pci_scan_bus (%d)\n", bus->number));
  bus_last = &bus->devices;
  max = bus->secondary;

  for (devfn = 0; devfn < 0xff; devfn++)
  {
    if (PCI_FUNC(devfn) && !is_multi)
    {
      /* not a multi-function device */
      continue;
    }
    if (pcibios_read_config_byte (bus->number, devfn, PCI_HEADER_TYPE, &hdr_type))
       continue;

    if (!PCI_FUNC (devfn))
       is_multi = hdr_type & 0x80;

    if (pcibios_read_config_dword (bus->number, devfn, PCI_VENDOR_ID, &l) ||
        /* some broken boards return 0 if a slot is empty: */
        l == 0xFFFFFFFF || l == 0x00000000 ||
        l == 0x0000FFFF || l == 0xFFFF0000)
    {
      is_multi = 0;
      continue;
    }

    dev = k_calloc (sizeof(*dev), 1);
    if (!dev)
    {
      printk ("pci: out of memory.\n");
      continue;
    }
    dev->bus    = bus;
    dev->devfn  = devfn;
    dev->vendor = l & 0xffff;
    dev->device = (l >> 16) & 0xFFFF;

    /* non-destructively determine if device can be a master:
     */
    pcibios_read_config_byte  (bus->number, devfn, PCI_COMMAND, &cmd);
    pcibios_write_config_byte (bus->number, devfn, PCI_COMMAND, cmd | PCI_COMMAND_MASTER);
    pcibios_read_config_byte  (bus->number, devfn, PCI_COMMAND, &tmp);
    dev->master = ((tmp & PCI_COMMAND_MASTER) != 0);
    pcibios_write_config_byte (bus->number, devfn, PCI_COMMAND, cmd);

    pcibios_read_config_dword (bus->number, devfn, PCI_CLASS_REVISION, &class);
    class >>= 8;                /* upper 3 bytes */
    dev->class = class;
    class >>= 8;
    dev->hdr_type = hdr_type;

    switch (hdr_type & 0x7F)         /* header type */
    {
      case PCI_HEADER_TYPE_NORMAL:   /* standard header */
           if (class == PCI_CLASS_BRIDGE_PCI)
              goto bad;

           /* If the card generates interrupts, read IRQ number
            * (some architectures change it during pcibios_fixup())
            */
           pcibios_read_config_byte (bus->number, dev->devfn,
                                     PCI_INTERRUPT_PIN, &irq);
           if (irq)
              pcibios_read_config_byte (bus->number, dev->devfn,
                                        PCI_INTERRUPT_LINE, &irq);
           dev->irq = irq;

           /* read base address registers, again pcibios_fixup() can
            * tweak these
            */
           pci_read_bases (dev, 6);
           pcibios_read_config_dword (bus->number, devfn, PCI_ROM_ADDRESS, &l);
           dev->rom_address = (l == 0xffffffff) ? 0 : l;
           break;

      case PCI_HEADER_TYPE_BRIDGE: /* bridge header */
           if (class != PCI_CLASS_BRIDGE_PCI)
              goto bad;
           pci_read_bases (dev, 2);
           pcibios_read_config_dword (bus->number, devfn, PCI_ROM_ADDRESS1, &l);
           dev->rom_address = (l == 0xffffffff) ? 0 : l;
           break;

      case PCI_HEADER_TYPE_CARDBUS: /* CardBus bridge header */
           if (class != PCI_CLASS_BRIDGE_CARDBUS)
              goto bad;
           pci_read_bases (dev, 1);
           break;

      default:              /* unknown header */
      bad:
           PRINTK (("pci: %02X:%02X [%04X/%04X/%06X] has unknown header "
                    "type %02X, ignoring.\n",
                    bus->number, dev->devfn, dev->vendor, dev->device,
                    (int)class, hdr_type));
           continue;
    }

    PRINTK (("pci: %02X:%02X [%04X/%04X]\n",
             bus->number, dev->devfn, dev->vendor, dev->device));

    /* Put it into the global PCI device chain. It's used to
     * find devices once everything is set up.
     */
    if (!pci_reverse)
    {
      *pci_last_dev_p = dev;
      pci_last_dev_p  = &dev->next;
    }
    else
    {
      dev->next   = pci_devices;
      pci_devices = dev;
    }

    /* Now insert it into the list of devices held by the parent bus.
     */
    *bus_last = dev;
    bus_last  = &dev->sibling;

#if 0
    /* Setting of latency timer in case it was less than 32 was
     * a great idea, but it confused several broken devices. Grrr.
     */
    pcibios_read_config_byte (bus->number, dev->devfn, PCI_LATENCY_TIMER, &tmp);
    if (tmp < 32)
       pcibios_write_config_byte (bus->number, dev->devfn,
                                  PCI_LATENCY_TIMER, 32);
#endif
  }

#if 0
  /* After performing arch-dependent fixup of the bus, look behind
   * all PCI-to-PCI bridges on this bus.
   */
  pcibios_fixup_bus (bus);
#endif

  /* The fixup code may have just found some peer pci bridges on this
   * machine.  Update the max variable if that happened so we don't
   * get duplicate bus numbers.
   */
  for (child = &pci_root; child; child = child->next)
      max = ((max > child->subordinate) ? max : child->subordinate);

  for (dev = bus->devices; dev; dev = dev->sibling)
  {
    /* If it's a bridge, scan the bus behind it.
     */
    if ((dev->class >> 8) == PCI_CLASS_BRIDGE_PCI)
    {
      unsigned devfn = dev->devfn;
      DWORD    buses;
      WORD     cr;

      /* Check for a duplicate bus.  If we already scanned this bus
       * number as a peer bus, don't also scan it as a child bus
       */
      if (((dev->vendor == PCI_VENDOR_ID_SERVERWORKS) &&
           ((dev->device == PCI_DEVICE_ID_SERVERWORKS_HE) ||
            (dev->device == PCI_DEVICE_ID_SERVERWORKS_LE) ||
            (dev->device == PCI_DEVICE_ID_SERVERWORKS_CMIC_HE))) ||
          ((dev->vendor == PCI_VENDOR_ID_COMPAQ) &&
           (dev->device == PCI_DEVICE_ID_COMPAQ_6010)) ||
          ((dev->vendor == PCI_VENDOR_ID_INTEL) &&
           ((dev->device == PCI_DEVICE_ID_INTEL_82454NX) ||
            (dev->device == PCI_DEVICE_ID_INTEL_82451NX))))
         goto skip_it;

      /* Read the existing primary/secondary/subordinate bus number
       * configuration to determine if the PCI bridge has already been
       * configured by the system.  If so, check to see if we've already
       * scanned this bus as a result of peer bus scanning, if so, skip this.
       */
      pcibios_read_config_dword (bus->number, devfn, PCI_PRIMARY_BUS, &buses);
      if ((buses & 0xFFFFFF) != 0)
      {
        for (child = pci_root.next; child; child = child->next)
            if (child->number == ((buses >> 8) & 0xff))
               goto skip_it;
      }

      /* Insert it into the tree of buses.
       */
      child = k_calloc (sizeof(*child), 1);
      if (!child)
      {
        printk ("pci: out of memory for bridge.\n");
        continue;
      }
      child->next   = bus->children;
      bus->children = child;
      child->self   = dev;
      child->parent = bus;

      /* Set up the primary, secondary and subordinate
       * bus numbers.
       */
      child->number      = child->secondary = ++max;
      child->primary     = bus->secondary;
      child->subordinate = 0xff;

      /* Clear all status bits and turn off memory,
       * I/O and master enables.
       */
      pcibios_read_config_word  (bus->number, devfn, PCI_COMMAND, &cr);
      pcibios_write_config_word (bus->number, devfn, PCI_COMMAND, 0x0000);
      pcibios_write_config_word (bus->number, devfn, PCI_STATUS, 0xffff);

      /* Read the existing primary/secondary/subordinate bus
       * number configuration to determine if the PCI bridge
       * has already been configured by the system.  If so,
       * do not modify the configuration, merely note it.
       */
      pcibios_read_config_dword (bus->number, devfn, PCI_PRIMARY_BUS, &buses);
      if (buses & 0xFFFFFF)
      {
        unsigned cmax;

        child->primary     = buses & 0xFF;
        child->secondary   = (buses >> 8) & 0xFF;
        child->subordinate = (buses >> 16) & 0xFF;
        child->number      = child->secondary;
        cmax = pci_scan_bus (child);
        if (cmax > max)
           max = cmax;
      }
      else
      {
        /* Configure the bus numbers for this bridge:
         */
        buses &= 0xff000000;
        buses |= (((unsigned) (child->primary)     << 0) |
                  ((unsigned) (child->secondary)   << 8) |
                  ((unsigned) (child->subordinate) << 16));
        pcibios_write_config_dword (bus->number, devfn, PCI_PRIMARY_BUS, buses);

        /* Now we can scan all subordinate buses:
         */
        max = pci_scan_bus (child);

        /* Set the subordinate bus number to its real value:
         */
        child->subordinate = max;
        buses = (buses & 0xff00ffff) | ((unsigned)(child->subordinate) << 16);
        pcibios_write_config_dword (bus->number, devfn, PCI_PRIMARY_BUS, buses);
      }
      pcibios_write_config_word (bus->number, devfn, PCI_COMMAND, cr);
    skip_it:;
    }
  }

  /* We've scanned the bus and so we know all about what's on
   * the other side of any bridges that may be on this bus plus
   * any devices.
   *
   * Return how far we've got finding sub-buses.
   */
  PRINTK (("pci: pci_scan_bus returning with max=%02X\n", max));
  return (max);
}


struct pci_bus *pci_scan_peer_bridge (int bus)
{
  struct pci_bus *b = &pci_root;

  while (b && bus)
  {
    if (b->number == bus)
       return (b);
    b = b->next;
  }
  b = k_calloc (sizeof (*b), 1);
  b->next        = pci_root.next;
  pci_root.next  = b;
  b->number      = b->secondary = bus;
  b->subordinate = pci_scan_bus (b);
  return (b);
}

/*
 * __initfunc
 */
int pci_init (void)
{
  if (!init_timer(NULL))
  {
    PRINTK (("pci: Failed to hook timer, IRQ %d\n", timer_irq));
    return (0);
  }

  __dpmi_get_segment_base_address (_my_ds(), &_virtual_base);

  pcibios_init();

  if (!pcibios_present())
  {
    PRINTK (("pci: No PCI bus detected\n"));
    return (0);
  }

  PRINTK (("pci: Probing PCI hardware\n"));

  memset (&pci_root, 0, sizeof(pci_root));
  pci_root.subordinate = pci_scan_bus (&pci_root);

#if 0
  /* give BIOS a chance to apply platform specific fixes:
   */
  pcibios_fixup();
#endif

#ifdef CONFIG_PCI_QUIRKS
  pci_quirks_init();
#endif

  return (1);
}

/*
 * __initfunc
 */
void pci_setup (char *str, int *ints)
{
  while (str)
  {
    char *k = strchr (str, ',');

    if (k)
       *k++ = '\0';
    if (*str && (str = pcibios_setup(str)) != NULL && *str)
    {
      if (!strcmp (str, "reverse"))
           pci_reverse = 1;
      else printk ("pci: Unknown option `%s'\n", str);
    }
    str = k;
  }
}

