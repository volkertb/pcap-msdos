# Set the appropriate architecture you wish to compile for.
#
# Note that Borland's 16-bit compiler does not have a 586 mode.
#
# Settings:     BCC       TASM
#	386	/3	/jP386N
#	486	/4	/jP486N
#
# If you wish Filter to drop all IP multicast packets rather than
# pass them, then add /DDENY_MULTICAST to the end of the CFLAGS=
# line. Whether you should enable this depends on your level
# of paranoia and whether you use routing protocols like OSPF
# which happen to use IP multicast.
#
BCCARCH=/3
TASMARCH=/jP386N

ASMSOURCES=misc.asm
CSOURCES=ip.c main.c bridge.c filter.c potp.c manage.c ndis.c queue.c xms.c stat.c syslog.c
OBJECTS=$(CSOURCES:.c=.obj) $(ASMSOURCES:.asm=.obj)
HEADERS=db.h const.h struct.h proto.h macro.h global.h xms.h
CFLAGS=$(BCCARCH) /ml /Ot /g25 /w-par /i40 $(DOASM)
AFLAGS=$(TASMARCH) /ml

filter.exe: $(OBJECTS)
	echo $(OBJECTS) > temp.rsp
	$(CC) /ml /M /efilter.exe @temp.rsp

$(OBJECTS): $(HEADERS)

clean:
	del temp.rsp
	del *.map
	del *.obj
	del filter.exe

pack:
	del pack\*.*
	cp *.c pack
	cp *.h pack
	cp misc.asm pack
	cp makefile.mak pack
	cp filter.exe pack
	del filter.zip
	cd pack
	pkzip ..\filter.zip *.*
	cd ..
