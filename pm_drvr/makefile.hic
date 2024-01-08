#
# Makefile for testing various ideas using HighC + Pharlap
#

LNK_FLAGS = -386 -twocase -nostub -fullwarn                          \
            -attributes class code er                                \
            -attributes class data rw                                \
            -attributes class stack rw                               \
            -maxdata 0 -fullseg -symbols -publist both -purge none * \
            -mapnames 30 -mapwidth 132

all:    test.exp pmsub.rex

test.exp: test.c load_0.obj load_1.obj
        hc386 -g -c -w3 -I$(WATT_ROOT)\inc $*.c
        386link $*.obj load_0.obj load_1.obj -exe $*.exp @&&|
          -lib hc386, hc387, dosx32, exc_hc
          $(LNK_FLAGS) -offset 1000h -pack -unpriv
|

pmsub.rex: pmsub.c
        hc386 -g -c -w3 -I$(WATT_ROOT)\inc $*.c
        386link $*.obj -relexe $*.rex -start main @&&|
          -lib hc386,hc387 $(LNK_FLAGS) -unpriv
|

load_1.obj: load_1.c
        hc386 -g -c -w3 $*.c

load_0.obj: load_0.asm
        tasm /t /mx /l /ie:\dosx\inc $*

