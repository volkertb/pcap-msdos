{$A+,B-,D+,E+,F-,G+,I+,L+,N-,O-,P-,Q-,R-,S+,T-,V+,X+,Y+}

{ ************************************************************************ }
{ USB.PAS:   SB BASIC Routines by Dieter R. Pawelczak <dieterp@bigfoot.de> }
{ ======================================================================== }
{                                                                          }
{              Unit to initialize and address the USB Host Controller      }
{                                                                          }
{ (c) 1998 by Dieter Pawelczak, <dieterp@bigfoot.de>                       }
{ This is public domain Software - selling this software is prohibeted!    }
{                                                                          }
{ function DetectVirtualRealMode(..) detects virtual real mode
{ function USBdetect(..);     detects an PCI USB Controller - done by INIT
{ function USBEnable          enables USB Controller PCI-BUS Master
{ function USBGetDeviceIOSpace(..)        reads I/O port address
{ function USBSetDeviceIOSpace(..)        sets I/O port address
{ function USBReadCommandReg  read access IOCommandRegister
{ procedure USBWriteCommandReg write access IOCommandRegister
{
{ Initialization should first check the USB device (test USBdetected),
{ read the I/O address space and if zero set an unused port address space
{ (32 byte), now PIC-BUS Master Control can be enabled by USBenable
{                                                                          }
{ ************************************************************************ }

{$define DEBUG} { Enables DEBUG Output }

unit usb;

interface

{$IFDEF DEBUG}
uses PCI,dutils;
{$ELSE}
uses PCI;
{$ENDIF}





const { USB-Command Regiser }

      MAXP=$80;
      CF=$40;
      SWDBG=$20;
      FGR=$10;
      EGSM=$08;
      GRESET=$04;
      HCRESET=$02;
      RS=$01;



type FrameListPointer = longint;            { 32 bit address + Q & T flag }
type FrameListPointerArray = Array[0..1023] of FrameListPointer;
type FrameList = ^FrameListPointerArray;


type LinkPointer = longint;            { 32 bit address + Vf & Q & T flag }
type BufferPointer = longint;          { 32 bit address of the data buffer }

type TransferDescriptor = record
        next:LinkPointer;                   { points to next T/Q descriptor or indicates termination by T-flag }
        ActLen: word;                       { presents the actual length (11 bit ) = and 2047 }
        Status: byte;                       { presents the Status }
        Flags:Byte;                         { Flags SPD(5), C_ERR(4,3), LS(2), ISO(1), IOC (0)}
        Token: longint;                   { MaxLen/R/R/EndPt/DeviceAddr/PID to be sent }
        BufferPtr: BufferPointer;              { Buffer Pointer for Data }
        res:array[0..3] of longint;
     end;
type transmitstatusptr=^byte;
type QueueHeadLinkPointer = longint;        { 32 bit address + Q & T flag }
type QueueHeadElementPointer = longint;     { 32 bit address + Q & T flag, bit 2 is undefined (write as 0) }

type QueueHead = record
        next:QueueHeadLinkPointer;          { points to next T/Q descriptor or indicates termination by T-flag }
        Element:QueueHeadElementPointer;    { points to next Queue Operation or indicates termination by T-flag }
     end;

type PortState = (port_disabled, port_newAttached, port_enabled, port_configured);

type USBRequest = record
        bmRequestType:Byte;
        bRequest:byte;
        wValue:word;
        wIndex:word;
        wLength:word;
    end;

const GetDeviceDescriptor : USBRequest =
       (bmRequestType:$80;bRequest:$06;wValue:$0001;wIndex:$0000;wLength:$0012);
const SetAddress : USBRequest =
       (bmRequestType:$00;bRequest:$05;wValue:$0001;wIndex:$0000;wLength:$0000);


var USBDeviceID,USBVendorID:word;           { PCI-Identification USB Controller }
    USBBusNumber,USBFunctionNumber:Byte;
    USBDescription:string;                  { Text Description }
    USBIOSpace:word;                        { I/O address space set/read }
    USBdetected:boolean;                    { Flag set by INIT }


var ISADeviceID,ISAVendorID:word;           { PCI-Identification PCI-ISA Controller }
    ISABusNumber,ISAFunctionNumber:Byte;    { needed by USBSetInt }
    ISADescription:string;                  { Text Description }
    ISAdetected:boolean;                    { Flag set by INIT }

var FrameListPtr:FrameList;                 { pointer to the frame list }
    FrameListBase:longint;                  { frame list 32 bit base address}
    FrameListHandle:pointer;                { TP memory handle }


{ LOW LEVEL: USB-Host Controller and PCI functions }
function USBdetect(Var DeviceID,VendorID:word;VAR BusNumber,FunctionNumber:Byte;var Description:string):boolean;
function USBEnable:boolean;
function USBDisable:boolean;
function USBGetDeviceIOSpace(var IOSpace:word):boolean;
function USBSetDeviceIOSpace(IOSpace:word):boolean;
function USBReadCommandReg:word;
function USBReadStatusReg:word;
function USBReadPort0Reg:word;
function USBReadPort1Reg:word;
function USBReadInterruptReg:word;
function USBReadFrameBaseReg:longint;
function USBReadFrameNumberReg:word;
procedure USBWriteCommandReg(value:word);
procedure USBWriteStatusReg(value:word);
procedure USBWriteInterruptReg(value:word);
procedure USBWriteFrameNumberReg(value:word);
procedure USBWritePort0Reg(value:word);
procedure USBWritePort1Reg(value:word);
function USBSetInterruptNumber(IntNo:word; active:boolean):boolean;
function USBGetInterruptNumber(Var IntNo:word;Var active:boolean):boolean;
function USBAllocateFrameList(Var FList:FrameList;VAR FLBase:FrameListPointer):boolean;
procedure USBCommandRun;
procedure USBCommandStop;
procedure USBDone;
procedure usbclearframelist;

{ General USB Functions }

function AllocateTransferDescriptor:pointer; { Returns pointer to TD or nil }
procedure FreeTransferDescriptor(p:pointer);
function CreateTransferDescriptor(Terminate,Queue,Depth:boolean;Link:linkpointer;
                                  Actln:word;State:word;IOC,IOS,LS:Boolean;C_error:byte;SPD:boolean;
                                  PID,DeviceAddress,EndPt:Byte;DataToggle:boolean;MaxLen:word;
                                  BPtr:BufferPointer):pointer; { Allocates and configures TD - Returns pointer to TD or nil }
procedure AlterTransferDescriptor(p:pointer;Actln:word;State:word;IOC,IOS,LS:Boolean;C_error:byte;SPD:boolean);

procedure InsertQueueDescriptorInFrameList(Number:word;p:pointer);
procedure InsertTransferDescriptorInFrameList(Number:word;p:pointer);
function GetLinkPointerFromTransferDescriptor(p:pointer):LinkPointer;
function GetLinkPointerFromFrameList(number:word):LinkPointer;
function GetTransferDescriptorFromFrameList(number:word):pointer;
function GetTransferDescriptorFromLinkPointer(l:linkpointer):pointer;
{ Helpers }

function DetectVirtualRealMode:boolean;
function GetPtrBase(p:pointer):longint;
function GetBasePtr(b:longint):pointer;


{$IFDEF DEBUG}
procedure USBprintTD(P:pointer);
procedure USBprintFrameList;
{$ENDIF}

implementation

function USBdetect(Var DeviceID,VendorID:word;VAR BusNumber,FunctionNumber:Byte;var Description:string):boolean;
var i:byte;
    error,found:boolean;
begin
  BusNumber:=0;FunctionNumber:=0;
  USBdetect:=false;
  i:=0;
  error:=false;
  found:=false;
  repeat { check different possible USB-PCI devices ... }
  case i of
    0: begin deviceId:=$7020;vendorId:=$8086; description:='Intel 82371SB USB controller'; end;
    1: begin deviceId:=$7112;vendorId:=$8086; description:='Intel PIIX4 USB controller'; end;
    2: begin deviceId:=$0571;vendorId:=$1106; description:='VIA AMD-645 USB controller'; end;
    3: begin deviceId:=$A0F8;vendorID:=$1045; description:='Opti 82C750 (Vendetta) USB controller'; end;
    4: begin deviceId:=$C861;vendorID:=$1045; description:='Opti 82C861/871 (Firelink/FireBlast) USB controller'; end;
  end;
  inc(i);
  found:=detectPCIdevice(deviceId,vendorId,BusNumber,FunctionNumber);
  error:=i>4;
  until error or found;
  USBdetect:=found;
end;

function ISAdetect(Var DeviceID,VendorID:word;VAR BusNumber,FunctionNumber:Byte;var Description:string):boolean;
var i:byte;
    error,found:boolean;
begin
  BusNumber:=0;FunctionNumber:=0;
  ISAdetect:=false;
  i:=0;
  error:=false;
  found:=false;
  repeat { check different possible ISA-PCI devices ... }
  case i of
    0: begin deviceId:=$7000;vendorId:=$8086; description:='Intel 82371SB ISA-PCI controller'; end;
    1: begin deviceId:=$7110;vendorId:=$8086; description:='Intel PIIX4 ISA-PCI controller'; end;
{    2: begin deviceId:=$0571;vendorId:=$1106; description:='VIA AMD-645 USB controller'; end;
    3: begin deviceId:=$A0F8;vendorID:=$1045; description:='Opti 82C750 (Vendetta) USB controller'; end;
    4: begin deviceId:=$C861;vendorID:=$1045; description:='Opti 82C861/871 (Firelink/FireBlast) USB controller'; end; }
  end;
  inc(i);
  found:=detectPCIdevice(deviceId,vendorId,BusNumber,FunctionNumber);
  error:=i>2;
  until error or found;
  ISAdetect:=found;
end;

function USBGetDeviceIOSpace(var IOSpace:word):boolean;
var okay:boolean;
    result:word;
    setiospace:word;
begin
  okay:=false;
  if readPCIRegisterWord($20,USBBusNumber,USBFunctionNumber,result) then
    begin
      okay:=true;
      IOSpace:=result and $FFFE;
      USBIOSpace:=IOSpace;
    end;
  USBGetDeviceIOSpace:=okay;
end;

function USBSetDeviceIOSpace(IOSpace:word):boolean;
var okay:boolean;
    result:word;
    setiospace:word;
begin
  okay:=false;
  IOSpace:=IOSpace ;
  if writePCIRegisterWord($20,USBBusNumber,USBFunctionNumber,IOSpace) then
    begin
      okay:=true;
      USBIOSpace:=IOSpace;
    end;
  USBSetDeviceIOSpace:=okay;
end;

function USBReadCommandReg:word;
begin
  USBReadCommandReg:=portw[USBIOSpace];
end;

function USBReadStatusReg:word;
begin
  USBReadStatusReg:=portw[USBIOSpace+2];
end;

function USBReadInterruptReg:word;
begin
  USBReadInterruptReg:=portw[USBIOSpace+4];
end;

function USBReadPort0Reg:word;
begin
  USBReadPort0Reg:=portw[USBIOSpace+16];
end;

procedure USBWritePort0Reg(value:word);
begin
  portw[USBIOSpace+16]:=value;
end;

function USBReadPort1Reg:word;
begin
  USBReadPort1Reg:=portw[USBIOSpace+18];
end;

procedure USBWritePort1Reg(value:word);
begin
  portw[USBIOSpace+18]:=value;
end;

procedure USBWriteInterruptReg(value:word);
begin
  portw[USBIOSpace+4]:=value;
end;

procedure USBWriteStatusReg(value:word);
begin
  portw[USBIOSpace+2]:=value;
end;

procedure USBWriteCommandReg(value:word);
begin
  portw[USBIOSpace]:=value;
end;

procedure USBWriteFrameNumberReg(value:word);
begin
  portw[USBIOSpace+6]:=value;
end;

function USBReadFrameNumberReg:word;
begin
  USBReadFrameNumberReg:=portw[USBIOSpace+6];
end;

function USBReadFrameBaseReg:longint;
begin
  asm
        mov dx,USBIOSpace
        add dx,08h
        db 66h; in ax,dx                       { in dx,eax  }
        db 66h; mov word ptr FrameListBase,ax  { mov ..,eax }
  end;
  USBReadFrameBaseReg:=FrameListBase;
end;

function USBEnable:boolean;
var okay:boolean;
    command:word;
begin
  okay:=false;
  if usbdetected and (USBIOspace<>0) then
    if readPCIRegisterWord($4,USBBusNumber,USBFunctionNumber,command) then
      begin
        okay:=command and 5=5;
        command:=command or 5;
        if writePCIRegisterWord($4,USBBusNumber,USBFunctionNumber,command) then
          okay:=true;
      end;
  USBenable:=okay;
end;

function USBGetInterruptNumber(Var IntNo:word;Var active:boolean):boolean;
var okay:boolean;
    command:longint;
    command2:longint;
begin
  okay:=false;
  active:=false;
  if isadetected then
      if readPCIRegisterDWord($60,ISABusNumber,ISAFunctionNumber,command) then
        begin
          intno:=command shr 24;
          active:=intno and 128=0;
          intno:=intno and 15;
          okay:=true;
        end;
   USBGetInterruptNumber:=okay;
end;

function USBSetInterruptNumber(IntNo:word;active:boolean):boolean;
var okay:boolean;
    command:byte;
    command2:word;
    dummy:word;
begin
  okay:=false;
  asm
    cli
  end;
  if isadetected then
      if readPCIRegisterByte($63,ISABusNumber,ISAFunctionNumber,command) then
        begin
          { Redirect IRQD = Register 63h  to ISA-BUS IRQ }
          command:=IntNo+ord(not active)*128; { Set interrupt Number to MSB }
          command2:=1 shl intno;
          if command2>255 then
              begin
                dummy:=port[$4d1] and (not (command2 shr 8));
                port[$4d1]:=port[$4d1] and (not (command2 shr 8));
              end else
              begin
                dummy:=port[$4d0] and (not (command2 shr 8));
                port[$4d0]:=port[$4d0] and (not (command2));
              end;
          dummy:=port[$21];
          if writePCIRegisterByte($63,ISABusNumber,ISAFunctionNumber,command) then
            begin
              dummy:=port[$21];
              { Set Interrupt Sensitive Mode }
                okay:=true;
            end;
          if intno>7 then
             begin
               asm
                 in al,0a1h
                 mov cl,byte ptr intno
                 sub cl,8
                 mov dl,1
                 shl dl,cl
                 not dl
                 and al,dl
                 out 0a1h,al
                 in al,021h
                 mov dl,2
                 not dl
                 and al,dl
                 out 021h,al
               end;
             end else
             begin
               asm
                 in al,021h
                 mov cl,byte ptr intno
                 mov dl,1
                 shl dl,cl
                 not dl
                 and al,dl
                 out 021h,al
               end;
             end;

          end;
   asm
     sti
   end;
   USBSetInterruptNumber:=okay;
  end;

function USBDisable:boolean;
var okay:boolean;
begin
  okay:=false;
  if usbdetected and (USBIOspace<>0) then
    if WritePCIRegisterWord($4,USBBusNumber,USBFunctionNumber,0) then
      begin
        okay:=true;
      end;
  USBDisable:=okay;
end;

procedure usbclearframelist;
var i:word;
begin
      for i:=0 to 1023 do
        FrameListPtr^[i]:= 1; { Set Terminate }
end;

function USBAllocateFrameList(Var FList:FrameList;VAR FLBase:FrameListPointer):boolean;
var okay:boolean;
    i:word;
begin
  if memavail>8192 then
    begin
      getmem(FrameListHandle,8192);
      FrameListBase:=longint(seg(FrameListHandle^)) shl 4+longint(ofs(FrameListHandle^));
      { 4K alignment }
      FrameListBase:=longint(FrameListbase + 4096) and $fffff000;
      FrameListPtr:=getbaseptr(FrameListBase);
      FList:=FrameListPtr;
      FLBase:=FrameListBase;
      USBWriteFrameNumberReg(0);
      for i:=0 to 1023 do
        FrameListPtr^[i]:= 1; { Set Terminate }
      asm
        mov dx,USBIOSpace
        add dx,08h
        db 66h; mov ax, word ptr FrameListBase  { mov eax, ... }
        db 66h; out dx,ax                       { out dx,eax  }
      end;
      USBWriteFrameNumberReg(0);
      okay:=true;
    end;
  USBAllocateFrameList:=okay;
end;

procedure InsertTransferDescriptorInFrameList(Number:word;p:pointer);
begin
  FrameListPtr^[Number]:= getPtrBase(p) and $fffffffc;
end;

function GetLinkPointerFromFrameList(number:word):LinkPointer;
begin
  GetLinkPointerFromFrameList:=FrameListPtr^[Number] and $fffffffc;
end;

procedure InsertQueueDescriptorInFrameList(Number:word;p:pointer);
begin
  FrameListPtr^[Number]:= getPtrBase(p) and $fffffffc +2;
end;



procedure USBCommandRun;
var value:word;
begin
  value:=USBReadCommandReg;
  value:=value or 1;
  USBWriteCommandReg(value);
end;

procedure USBCommandStop;
var value:word;
begin
  value:=USBReadCommandReg;
  value:=value and $fe;
  USBWriteCommandReg(value);
end;


function DetectVirtualRealMode:boolean;assembler;
asm
  smsw ax
  and ax,1
end;

function GetPtrBase(p:pointer):longint;
begin
  GetPtrBase:=longint(seg(p^)) shl 4 + longint(ofs(p^));
end;

function GetBasePtr(b:longint):pointer;
var h1,h2:longint;
begin
  h1:=b shr 4;
  h2:=b and $f;
  GetbasePtr:=Ptr(h1,h2);
end;

function AllocateTransferDescriptor:pointer;
var i,j,k:word;
    p1:^transferdescriptor;
    PA:array[1..1000] of pointer;
begin
  p1:=nil;
  getmem(p1,32);
  if ofs(p1^) and $f<>0 then
     begin
        j:=0;
        repeat
          inc(j);
          freemem(p1,32);p1:=NIL;
          getmem(pa[j],1);
          getmem(p1,32);
        until (j=1000) or (ofs(p1^)=0);
        if j=1000 then
          begin
            writeln('Fatal: Allocating TD memory error...');
            halt(3);
          end;
        for k:=1 to j do Freemem(pa[k],1);
    end;
  if p1<>NIL then
  with p1^ do
        begin
          next:=0;
          ActLen:=0;
          Status:=0;
          Flags:=0;
          token:=0;
          BufferPtr:=0;
         end;
  AllocateTransferDescriptor:=p1;
end;

procedure FreeTransferDescriptor(p:pointer);
var td:^TransferDescriptor;
begin
  freemem(p,32);
end;

function GetLinkPointerFromTransferDescriptor(p:pointer):LinkPointer;
begin
  GetLinkPointerFromTransferDescriptor:=getptrbase(p);
end;

function GetTransferDescriptorFromLinkPointer(l:linkpointer):pointer;
begin
  GetTransferDescriptorFromLinkPointer:=getbaseptr(l and $fffffffc) ;
end;

function GetTransferDescriptorFromFrameList(number:word):pointer;
begin
  GetTransferDescriptorFromFrameList:=getbaseptr(FrameListPtr^[Number] and $fffffffc);
end;

procedure AlterTransferDescriptor(p:pointer;Actln:word;State:word;IOC,IOS,LS:Boolean;C_error:byte;SPD:boolean);
var td:^TransferDescriptor;
begin
  td:=p;
  if td<>nil then with td^ do
    begin
      Actlen:=Actln;
      flags:=ord(IOC)+ord(IOS) shl 1 +ord(ls) shl 2+(c_error and 3) shl 3+ord(spd) shl 5;
      Status:=state;
    end;
end;

function CreateTransferDescriptor(Terminate,Queue,Depth:boolean;Link:linkpointer;
                                  Actln:word;State:word;IOC,IOS,LS:Boolean;C_error:byte;SPD:boolean;
                                  PID,DeviceAddress,EndPt:Byte;DataToggle:boolean;MaxLen:word;
                                  BPtr:BufferPointer):pointer; { Allocates and configures TD - Returns pointer to TD or nil }
var td:^TransferDescriptor;
begin
  td:=AllocateTransferDescriptor;
  if td<>nil then with td^ do
    begin
      next:=link and $fffffff0+ord(Terminate)+ord(Queue) shl 1+ord(Depth) shl 2;
      Actlen:=Actln;
      Status:=state;
      flags:=ord(IOC)+ord(IOS) shl 1 +ord(ls) shl 2+(c_error and 3) shl 3+ord(spd) shl 5;
      token:=pid+longint(DeviceAddress) shl 8+longint(EndPt) shl 15+longint(ord(DataToggle)) shl 19+longint(maxlen) shl 21;
      bufferPtr:=Bptr;

    end;
  CreateTransferDescriptor:=td;
end;

{$IFDEF DEBUG}
procedure USBprintLinkPtr(L:LinkPointer);
var h:longint;
    i:word;
begin
      h:=l and $fffffff0;
      write('LinkPtr:  ');
      if h=0 then write('-EMPTY- [');
      write('- [',hexs(h));
      if l and 4=4 then write('] Vf ') else write('] -- ');
      if l and 2=2 then write(' Q ') else write(' - ');
      if l and 1=1 then write(' T ') else write(' - ');
      writeln;
end;

procedure USBprintFrameList;
var i,j:word;
    l:longint;
begin
  write('FrameList---------------[',hexs(FrameListBase),']---------------------------------------');
  for i:=0 to 1023 do
    begin
      if i mod 6=0 then writeln;
      l:=FrameListPtr^[i];
      write('[',hexs(l),']');
      if l and 2=2 then write('Q') else write('-');
      if l and 1=1 then write('T') else write('-');
      write(' ');
    end;
  writeln;
  writeLn('-------------------------------------------------------------------------');
end;

procedure USBprintTD(P:pointer);
var td:^TransferDescriptor;
    i:word;
    h:longint;
    hp:^byte;
begin
  td:=p;
  with td^ do
    begin
      writeLn('Transfer Descriptor-----[',hexs(GetPtrBase(p)),']---------------------------------------');
      USBprintLinkPtr(next);
      write('Control:  ');
      if flags and 32=32 then write(' SP ') else write(' -- ');
      write('C_ERROR: ',chr(48+ord(flags and 16=16)),chr(48+ord(flags and 8=8)));
      if flags and 4=4 then write(' LS ') else write(' -- ');
      if flags and 2=2 then write(' ISO ') else write(' --- ');
      if flags and 1=1 then write(' ICO ') else write(' --- ');
      write(' Status: ',bins8(status));
      writeln(' Len: ',Actlen);
      write('Token:    MaxLen: ',Token shr 21 and $7ff);
      write('  Toggle: ',(Token shr 19) and 1);
      WRite('  EndPt:',hexs8((Token shr 15) and $f));
      WRite('  DevAddr:',hexs8((Token shr 8) and $7f));
      WRite('  PID:',hexs8((Token) and $ff));
      writeln;
      write('BufferPtr:',hexs(bufferptr));
      if bufferptr<>0 then
        begin
          write(' - ');
          hp:=getBasePtr(bufferptr);
          for i:=1 to 8 do
            begin
              write(hexs8(hp^),' ');
              inc(hp);
            end;
        end;
      writeln;


      writeLn('-------------------------------------------------------------------------');
    end;
  end;



{$ENDIF}


var oldmasterintmask:byte;
    oldslaveintmask:byte;
    old_port4d0:byte;
    old_port4d1:byte;
    old_pirqd:byte;

procedure USBDone;
begin
       port[$4d0]:=old_port4d0;
       port[$4d1]:=old_port4d1;
       WritePCIRegisterByte($63,ISABusNumber,ISAFunctionNumber,old_pirqd);
    asm
      mov al,oldslaveintmask
      out 0a1h,al
      mov al,oldmasterintmask
      out 021h,al
    end;
end;

begin
  USBdetected:=false;
  if detectPCIbios then
    begin
       USBdetected:=USBdetect(USBDeviceId,USBVendorId,USBBusNumber,USBFunctionNumber,USBdescription);
       ISAdetected:=ISAdetect(ISADeviceId,ISAVendorId,ISABusNumber,ISAFunctionNumber,ISAdescription);
       old_port4d0:=port[$4d0];
       old_port4d1:=port[$4d1];
       readPCIRegisterByte($63,ISABusNumber,ISAFunctionNumber,old_pirqd);
       asm
        in al,0a1h
        mov oldslaveIntMask,al
        in al,021h
        mov oldmasterIntMask,al
       end;
    end;
end.
