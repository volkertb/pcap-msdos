#!/usr/bin/perl -w

use strict;

my (@regdata, $i);
my (@regdata_ns, @dword);
my $high_rb = 0;

for ($i = 0; $i < 2000; $i++) {
	$regdata[$i] = 0;
	$regdata_ns[$i] = 0;
	$dword[$i] = 0;
}

while (<>) {
	while (/\W$/) { chop; }
	if (/^ 0x0(\d\d): (.*)$/) {
		&parse_reg_line(hex($1), $2);
	}
}
&show_reg_data;
exit(0);


sub parse_reg_line {
	my ($regbase, $line) = @_;

	my (@data) = ($line =~ /(\w+) (\w+) (\w+) (\w+) (\w+) (\w+) (\w+) (\w+)/);
	my ($dword, @bytes, $byte, $swap, $rbs);

	foreach $dword (@data) {
		(@bytes) = ($dword =~ /(\w\w)(\w\w)(\w\w)(\w\w)/);

		$dword[$regbase] = hex($dword);

		$rbs = $regbase;
		foreach $byte (@bytes) {
			$regdata[$rbs] = hex ($byte);
			$high_rb = $rbs if ($rbs > $high_rb);
			$rbs++;
		}

		$swap = $bytes[0];
		$bytes[0] = $bytes[3];
		$bytes[3] = $swap;

		$swap = $bytes[1];
		$bytes[1] = $bytes[2];
		$bytes[2] = $swap;

		foreach $byte (@bytes) {
			$regdata[$regbase] = hex ($byte);
			$high_rb = $regbase if ($regbase > $high_rb);
			$regbase++;
		}
	}
}


sub printbits {
	my ($val, $regname, @bitstrings) = @_;
	my ($i);

	print "$regname: ";
	if ($val == 0) {
		print "0\n";
		return;
	}

	for ($i = 31; $i >= 0; $i--) {
		if ($val & (1 << $i)) {
			if ($bitstrings[$i]) {
				print $bitstrings[$i], " ";
			} else {
				printf "BIT%d ", $i;
			}
		}
	}

	print "\n";
}


sub printCR {
	my ($val) = @_;

	&printbits ($val, "CR",
		"BUFE", "RES1", "TE", "RE", "RST");
}


sub printERSR {
	my ($val) = @_;

	&printbits ($val, "ERSR",
		"EROK", "EROVW", "ERBad", "ERGood");
}


sub printIxR {
	my ($let, $val) = @_;

	my $str = printf "I%sR", $let;

	&printbits ($val, $str,
		"ROK",
		"RER",
		"TOK",
		"TER",
		"RXOVW",
		"PUN/LinkChg",
		"FOVW",
		"RES7",
		"RES8",
		"RES9",
		"RES10",
		"RES11",
		"RES12",
		"LenChg",
		"TimeOut",
		"SERR");
}


sub printTSD {
	my ($regnum, $val) = @_;

	my $er = ($val >> 16) & 0b111111;
	if ($er == 0) {
		$er = 8;
	} else {
		$er *= 32;
	}

	printf "TSD$regnum: SIZE=%d ERTXTH=%d NCC=%d %s%s%s%s%s%s%s%s\n",
		$val & 0b1111111111111,
		$er,
		($val >> 24) & 0b1111,
		$val & (1 << 13) ? "OWN " : "",
		$val & (1 << 14) ? "TUN " : "",
		$val & (1 << 15) ? "TOK " : "",
		$val & (1 << 22) ? "RES22 " : "",
		$val & (1 << 23) ? "RES23 " : "",
		$val & (1 << 28) ? "CDH " : "",
		$val & (1 << 29) ? "OWC " : "",
		$val & (1 << 30) ? "TABT " : "",
		$val & (1 << 31) ? "CRS " : "";
}


sub printTCR {
	my ($val) = @_;

	my %vermap = (
		0x60000000 => "RTL-8139",
		0x70000000 => "RTL-8139A",
		0x70800000 => "RTL-8139A_G",
		0x78000000 => "RTL-8139B",
		0x7C000000 => "RTL-8130",
		0x74000000 => "RTL-8139C",
	);
	my $verstr = $vermap{$val & 0x7C800000};
	$verstr = 'unknown' unless $verstr;

	my %loopmap = (
		0 => "normal",
		1 => "MAC-loop",
		2 => "PHY-loop",
		3 => "TW-loop",
	);
	my $loopstr = $loopmap{($val >> 17) & 3};
	$loopstr = 'unknown' unless $loopstr;

	my $dmaburst = 16 << (($val >> 8) & 7);

	printf "TCR: CHIP=%s TXRR=%d MAXDMA=%d LOOP=%s %s%s%s\n",
		$verstr,
		($val >> 4) & 15,
		$dmaburst,
		$loopstr,
		$val & (1 << 16) ? "CRC " : "",
		$val & (1 << 24) ? "IFG0 " : "",
		$val & (1 << 25) ? "IFG1 " : "";
}


sub printRCR {
	my ($val) = @_;

	my $dmaburst = 16 << (($val >> 8) & 7);
	$dmaburst = 'unlimited' if ($dmaburst == 2048);

	my $rxfifo = 16 << (($val >> 13) & 7);
	$rxfifo = 'none' if ($rxfifo == 2048);

	my %sizemap = (
		0 => "8K+16",
		1 => "16K+16",
		2 => "32K+16",
		3 => "64K+16",
	);
	my $sizemapstr = $sizemap{($val >> 11) & 3};
	$sizemapstr = 'unknown' unless $sizemapstr;
	
	my $erth = ($val >> 24) & 15;
	if ($erth == 0) {
		$erth = "none";
	} else {
		$erth .= "/16";
	}
	
	printf "RCR: MAXDMA=%s RBLEN=%s RXFTH=%s ERTH=%s\n",
		$dmaburst,
		$sizemapstr,
		$rxfifo,
		$erth;
	
	&printbits ($val & 0b11110000111111110000000011111111,
		"RCR (cont)",
		"APP",
		"APM",
		"AM",
		"AB",
		"AR",
		"AER",
		"9356SEL",
		"WRAP",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"RER8",
		"MulERINT");
}


sub print9346CR {
	my ($val) = @_;

	my %map = (
		0 => "normal mode",
		1 => "Auto-load",
		2 => "93C[45]6 programming",
		3 => "Config register write enable",
	);
	my $mapstr = $map{($val >> 6) & 3};
	$mapstr = 'unknown' unless $mapstr;

	print "9346CR: $mapstr\n";
}


sub printConfig0 {
	my ($val) = @_;

	&printbits ($val, "Config0",
		"BS0",
		"BS1",
		"BS2",
		"PL0",
		"PL1",
		"T10",
		"PCS",
		"SCR");
}


sub printConfig1 {
	my ($val) = @_;

	&printbits ($val, "Config1",
		"PMEn",
		"VPD",
		"IOMAP",
		"MEMMAP",
		"LWACT",
		"DVRLOAD",
		"LEDS0",
		"LEDS1");
}


sub printConfig3 {
	my ($val) = @_;

	&printbits ($val, "Config3",
		"FBtBEn",
		"FuncRegEn",
		"CLKRUN_En",
		"CardB_En",
		"LinkUp",
		"Magic",
		"PARM_En",
		"GNTSel");
}


sub printConfig4 {
	my ($val) = @_;

	&printbits ($val, "Config4",
		"Rd_Aux",
		"PARM_En2",
		"LWPTN",
		"MSWFB",
		"LWPME",
		"LongWF",
		"AnaOff",
		"RxFIFOAutoClr");
}


sub printMSR {
	my ($val) = @_;

	&printbits ($val, "MSR",
		"RXPF",
		"TXPF",
		"LINKB",
		"SPEED_10",
		"Aux_Status",
		"RES5",
		"RXFCE",
		"TXFCE");
}


sub printTSAD {
	my ($val) = @_;

	&printbits ($val, "TSAD",
		"OWN0",
		"OWN1",
		"OWN2",
		"OWN3",
		"TABT0",
		"TABT1",
		"TABT2",
		"TABT3",
		"TUN0",
		"TUN1",
		"TUN2",
		"TUN3",
		"TOK0",
		"TOK1",
		"TOK2",
		"TOK3");
}


sub printBMCR {
	my ($val) = @_;

	&printbits ($val, "BMCR",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"FDX",
		"NwayRestart",
		"",
		"",
		"ANE",
		"Spd_Set",
		"",
		"Reset");
}


sub printMISR {
	my ($val) = @_;

	&printbits ($val, "MISR");
}


sub printANAR {
	my ($val) = @_;

	&printbits ($val, "ANAR",
		"Sel0",
		"Sel1",
		"Sel2",
		"Sel3",
		"Sel4",
		"10",
		"10FD",
		"TX",
		"TXFD",
		"T4",
		"Pause",
		"",
		"",
		"RF",
		"ACK",
		"NP");
}


sub printBMSR {
	my ($val) = @_;

	&printbits ($val, "BMSR",
		"ExtCap",
		"JabberDetect",
		"LinkStatus",
		"AutoNeg",
		"RF",
		"ANC",
		"",
		"",
		"",
		"",
		"",
		"10baseT-HD",
		"10baseT-FD",
		"100baseT-HD",
		"100baseT-HD",
		"100baseT-T4");
}


sub printANLPAR {
	my ($val) = @_;

	&printbits ($val, "ANLPAR",
		"Sel0",
		"Sel1",
		"Sel2",
		"Sel3",
		"Sel4",
		"10",
		"10FD",
		"TX",
		"TXFD",
		"T4",
		"Pause",
		"",
		"",
		"RF",
		"ACK",
		"NP");
}


sub printANER {
	my ($val) = @_;

	&printbits ($val, "ANER",
		"LP_NW_ABLE",
		"PAGE_RX",
		"NP_ABLE",
		"LP_NP_ABLE",
		"MLF");
}


sub printNWAYTR {
	my ($val) = @_;

	&printbits ($val, "NWAYTR",
		"FLAGLSC",
		"FLAGPDF",
		"FLAGABD",
		"ENNWLE",
		"",
		"",
		"",
		"NWLPBK",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"");
}


sub printCSCR {
	my ($val) = @_;

	&printbits ($val, "CSCR",
		"PASS_SCR",
		"",
		"Con_Status_En",
		"Con_Status",
		"",
		"F_Connect",
		"F_LINK_100",
		"JBEN",
		"HEART_BEAT",
		"LD",
		"",
		"",
		"",
		"",
		"",
		"Testfun");
}


sub show_reg_data {
	printf "ID registers: %02X:%02X:%02X:%02X:%02X:%02X\n",
		$regdata[0x00],
		$regdata[0x01],
		$regdata[0x02],
		$regdata[0x03],
		$regdata[0x04],
		$regdata[0x05];

	printf "Reserved (06) = 0x%02X\n", $regdata[0x06];
	printf "Reserved (07) = 0x%02X\n", $regdata[0x07];

	printf "MAR0-7: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
		$regdata[0x08],
		$regdata[0x09],
		$regdata[0x0A],
		$regdata[0x0B],
		$regdata[0x0C],
		$regdata[0x0D],
		$regdata[0x0E],
		$regdata[0x0F];

	&printTSD(0, $dword[0x10]);
	&printTSD(1, $dword[0x14]);
	&printTSD(2, $dword[0x18]);
	&printTSD(3, $dword[0x1C]);

	printf "TSAD0 = 0x%08X\n", $dword[0x20];
	printf "TSAD1 = 0x%08X\n", $dword[0x24];
	printf "TSAD2 = 0x%08X\n", $dword[0x28];
	printf "TSAD3 = 0x%08X\n", $dword[0x2C];

	printf "RBSTART = 0x%08X\n", $dword[0x30];

	printf "ERBCR = 0x%04X\n", $dword[0x34] >> 16;
	&printERSR($regdata[0x36]);
	&printCR($regdata[0x37]);

	printf "CAPR = %d\n", $dword[0x38] & 0xFFFF;
	printf "CBR = %d\n", $dword[0x38] >> 16;
	&printIxR("M", $dword[0x3C] & 0xFFFF);
	&printIxR("S", $dword[0x3C] >> 16);

	&printTCR($dword[0x40]);
	&printRCR($dword[0x44]);
	printf "TCTR = %u\n", $dword[0x48];
	printf "MPC = 0x%08X\n", $dword[0x4C];

	&print9346CR($regdata[0x50]);
	&printConfig0($regdata[0x51]);
	&printConfig1($regdata[0x52]);
	printf "Reserved (53) = 0x%02X\n", $regdata[0x53];

	printf "TimerInt = 0x%08X\n", $dword[0x54];

	&printMSR($regdata[0x58]);
	&printConfig3($regdata[0x59]);
	&printConfig4($regdata[0x5A]);
	printf "Reserved (5B) = 0x%02X\n", $regdata[0x5B];

	&printMISR($dword[0x5C] & 0xFFFF);
	printf "RERID = 0x%02X\n", $regdata[0x5E];
	printf "Reserved (5F) = 0x%02X\n", $regdata[0x5F];

	&printTSAD($dword[0x60] & 0xFFFF);
	&printBMCR($dword[0x60] >> 16);

	&printBMSR($dword[0x64] & 0xFFFF);
	&printANAR($dword[0x64] >> 16);

	&printANLPAR($dword[0x68] & 0xFFFF);
	&printANER($dword[0x68] >> 16);

	printf "DIS = %d\n", $dword[0x6C] & 0xFFFF;
	printf "FCSC = %d\n", $dword[0x6C] >> 16;

	&printNWAYTR($dword[0x70] & 0xFFFF);
	printf "Rx Err Count = %d\n", $dword[0x70] >> 16;

	&printCSCR($dword[0x74] & 0xFFFF);
	printf "Reserved(76) = 0x%04X\n", $dword[0x74] >> 16;

	printf "PHY1_PARM = 0x%08X\n", $dword[0x78];
	printf "TW_PARM = 0x%08X\n", $dword[0x7C];

	if ($high_rb < 0x80) {
		return;
	}

	printf "PHY2_PARM = 0x%02X\n", $regdata[0x80];
	printf "Reserved (81) = 0x%02X\n", $regdata[0x81];
	printf "Reserved (82) = 0x%02X\n", $regdata[0x82];
	printf "Reserved (83) = 0x%02X\n", $regdata[0x83];

	printf "CRC0-7: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
		$regdata[0x84],
		$regdata[0x85],
		$regdata[0x86],
		$regdata[0x87],
		$regdata[0x88],
		$regdata[0x89],
		$regdata[0x8A],
		$regdata[0x8B];

	printf "Wakeup0: %08X %08X\n", $dword[0x8C], $dword[0x90];
	printf "Wakeup1: %08X %08X\n", $dword[0x94], $dword[0x98];
	printf "Wakeup2: %08X %08X\n", $dword[0x9C], $dword[0xA0];
	printf "Wakeup3: %08X %08X\n", $dword[0xA4], $dword[0xA8];
	printf "Wakeup4: %08X %08X\n", $dword[0xAC], $dword[0xB0];
	printf "Wakeup5: %08X %08X\n", $dword[0xB4], $dword[0xB8];
	printf "Wakeup6: %08X %08X\n", $dword[0xBC], $dword[0xC0];
	printf "Wakeup7: %08X %08X\n", $dword[0xC4], $dword[0xC8];

	printf "LSBCRC0-7: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
		$regdata[0xCC],
		$regdata[0xCD],
		$regdata[0xCE],
		$regdata[0xCF],
		$regdata[0xD0],
		$regdata[0xD1],
		$regdata[0xD2],
		$regdata[0xD3];

	printf "FLASH = 0x%08X\n", $dword[0xD4];

	printf "Config5 = 0x%02X\n", $regdata[0xD8];
	printf "Reserved (D9) = 0x%02X\n", $regdata[0xD9];
	printf "Reserved (DA) = 0x%02X\n", $regdata[0xDA];
	printf "Reserved (DB) = 0x%02X\n", $regdata[0xDB];

	printf "FER = 0x%08X\n", $dword[0xF0];
	printf "FEMR = 0x%08X\n", $dword[0xF4];
	printf "FPSR = 0x%08X\n", $dword[0xF8];
	printf "FFER = 0x%08X\n", $dword[0xFC];
}

