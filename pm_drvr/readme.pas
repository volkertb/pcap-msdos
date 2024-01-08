******************************************************************************
*                                                                            *
*                      U S B - Support for Turbo-Pascal                      *
*                                                                            *
*                                                                            *
*            (c) 1998 by Dieter R. Pawelczak <dieterpbigfoot.de>             *
*                                                                            *
******************************************************************************


For many embedded systems USB seems to be a good solution for data acquisition
modules - as USB supports a frame rate of 1 ms which is fast enough for many
feedback control applications. The problem now is that USB is currently only
supported by Windows 98 and updates of Windows 95. The design of the USB host
controller again emphasizes the possibilities using USB in an embedded System:
Once the USB communication is established, the host controller can work in the
background using DMA-memory transfers - accompanied with a synchronous
interrupt every millisecond, a real time feedback control system is easily
established.

For a simple motor velocity control I developped a microcontroller board with
the USBN9602 controller by National Semiconductors. For a simple test
environment I created some basic routines to access the USB host controller
and did some experiments on the USB transfers. The following Turbo-Pascal units
allow the initialization and the control of the USB host controller. As an
example how to access and configure a USB device, I added the example program
HUBDISCO, which enables and configures a 4-port HUB (using TUSB2040 from TI)
and switches through its downstream ports.

The example must be run in real mode environment as it needs the fact:
physical address == linear address.

These units have been created for testing purpose only and don't present a
complete USB environment. It has been tested on several different main boards
and processor types (Intel PIIX4,PIIX3).
The units are created for Turbo-Pascal 7.0, but should run with some minor
changes with Turbo-Pascal 6.0. The strange looking inline asm instructions are
mainly 32 bit port access commands, which are not supported by Turbo Pascal.

USB.PAS:      basic USB routines
PCI.PAS:      access of PCI devices
DUTILS.PAS:   utility unit
HUBDISCO.PAS: example program

The units are based on the USB specification Version 1.1 and the Intel UHCI
documentation.


Literature

USB-Spec., Version 1.1., see http://www.usb.org
Intel UHCI, Intel PIIX4 documentation,  see http://www.intel.com



Dieter R. Pawelczak, January 1999

As I don't work on the USB-sector any longer, this is a final release of the
units.
