{ Sample Program uses USB.PAS                                                }
{ -------------------------------------------------------------------------- }
{ Configures an USB 4-port-HUB-Device and enables its downstream ports       }
{ (c) 1998 by Dieter Pawelczak, <dieterp@bigfoot.de>                         }
{ This is public domain Software - selling this software is prohibeted!      }
{ -------------------------------------------------------------------------- }
{ For more info seek www.usb.org                                             }


uses dutils,pci,usb,dos,crt;

Var DeviceID,VendorID:word;         { PCI-BUS USB Device }
    BusNumber,FunctionNumber:Byte;  { PCI-BUS Address    }
    Description:string;             { PCI-BUS Ddevice Description }

    { Helpers }

    result:longint;
    IOSpace:word;
    hwint:word;
    dummytimer:word;
    hwactive:boolean;
    flbase:framelistpointer;
    flptr:framelist;

    var testbuffer:array[0..1279] of byte;
        const deviceaddress=1;

const preferredHWInterrupt=3;       { if not configured then use this HW-interrupt }
      preferredIOSpace=$120;        { if not configured then use this IO Addressspace }

{Requests to enable HUB}

const SetAddressDescriptor : USBRequest =
       (bmRequestType:$00;bRequest:$5;wValue:$0001;
        { Address for USB device at port 1 }
        wIndex:$000;wLength:$0000);

const GetHubDescriptor : USBRequest =
       (bmRequestType:$A0;bRequest:$06;wValue:$0001;wIndex:$0000;wLength:$0012);

const SetConfiguration : USBRequest =
       (bmRequestType:$00;bRequest:$9;wValue:$0001;wIndex:$0000;wLength:$000);

const GetConfiguration : USBRequest =
       (bmRequestType:$00;bRequest:$8;wValue:$0001;wIndex:$0000;wLength:$000);

const GetHubStatus : USBRequest =
       (bmRequestType:$A0;bRequest:$0;wValue:$000;wIndex:$0000;wLength:$004);

const GetDeviceDescriptor : USBRequest =
       (bmRequestType:$80;bRequest:$06;wValue:$0001;wIndex:$0000;wLength:$008);

const SetHubPortPowerFeature : USBRequest =
       (bmRequestType:$23;bRequest:$3;wValue:$0008;wIndex:$0001;wLength:$000);


var oldUsbInt:pointer;              { store original Interrupt service routine }
    serviceInt:word;                { HW-INT VEC != SW-INT VEC }


procedure slaveInterrupt;interrupt;
begin
  cprint(1,24,' XXX ',48);          { display IRQ call }
  inc(dummytimer);
  port[$20]:=$20;
  port[$A0]:=$20;
end;

procedure masterinterrupt;interrupt;
begin
  inc(dummytimer);
  cprint(1,24,' XXX ',48);          { display IRQ call }
  port[$20]:=$20;
end;

procedure USBStatus;
var status,comm:word;


begin
  status:=UsbReadStatusReg;
  comm:=UsbReadCommandReg;
  writeln('S:',bins8(status),'=',hexs8(status),'  C:',bins8(Comm),'=',hexs8(comm),
  '  FN:',hexs16(USBReadFrameNumberReg),'  FB:',hexs(USBreadFrameBaseReg)+
  '  PS:',bins16(USBreadPort0Reg),'=',hexs8(USBreadPort0Reg));
  USBWriteStatusReg($ffff);
end;

var i:word;
    sampleTD:^transferDescriptor;
    sampleTD1:^transferDescriptor;
    sampleTD2:^transferDescriptor;
    sampleTD3:^transferDescriptor;
    sampleTD4:^transferDescriptor;
    sampleTD5:^transferDescriptor;
Label DoItAgain;

begin
  clrscr;
  assign(output, '');rewrite(output);

  writeln('Testprogramm to detect and initialize the USB controller');
  writeln('and switch through a 4 PORT Hub');
  writeln;

  { Create TD for HUB initialization }

  sampleTD:=CreateTransferDescriptor(true{ Terminate } ,
                                     false { Queue },
                                     false { Depth },
                                     0 { Link },
                                     $0 { Actlen },
                                     $80 { $80 = active Status },
                                     true { IOC },
                                     false { IOS },
                                     false { LS } ,
                                     1 { C_error: },
                                     false { SPD },
                                     $2d  { e1 = OUT 2d { 2d PID = SETUP },
                                     0 , { Deviceaddress }
                                     0 { EndPt },
                                     false { DataToggle },
                                     $7 { MaxLen },
                                     getPtrBase(@SetAddress));
  sampleTD1:=CreateTransferDescriptor(true{ Terminate } ,
                                     false { Queue },
                                     false { Depth },
                                     0{ Link },
                                     $7ff { Actlen },
                                     $80 { $80=active Status },
                                     true { IOC },
                                     false { IOS },
                                     false { LS } ,
                                     1 { C_error: },
                                     false { SPD },
                                     $69 { 69 = PID = IN },
                                     0 { DeviceAddress },
                                     0 { EndPt },
                                     true { DataToggle },
                                     $7ff { MaxLen },
                                     getPtrBase(@testbuffer) { BufferPtr });
  sampleTD2:=CreateTransferDescriptor(true{ Terminate } ,
                                     false { Queue },
                                     false { Depth },
                                     0 { Link },
                                     $0 { Actlen },
                                     $80 { $80 = active Status },
                                     true { IOC },
                                     false { IOS },
                                     false { LS } ,
                                     1 { C_error: },
                                     false { SPD },
                                     $2d  { e1 = OUT 2d { 2d PID = SETUP },
                                     1 , { Deviceaddress }
                                     0 { EndPt },
                                     false { DataToggle },
                                     $7 { MaxLen },
                                     getPtrBase(@SetConfiguration));
  sampleTD3:=CreateTransferDescriptor(true{ Terminate } ,
                                     false { Queue },
                                     false { Depth },
                                     0{ Link },
                                     $7ff { Actlen },
                                     $80 { $80=active Status },
                                     true { IOC },
                                     false { IOS },
                                     false { LS } ,
                                     1 { C_error: },
                                     false { SPD },
                                     $69 { 69 = PID = IN },
                                     1 { DeviceAddress },
                                     0 { EndPt },
                                     true { DataToggle },
                                     $7ff { MaxLen },
                                     getPtrBase(@testbuffer) { BufferPtr });

  sampleTD4:=CreateTransferDescriptor(true{ Terminate } ,
                                     false { Queue },
                                     false { Depth },
                                     0 { Link },
                                     $0 { Actlen },
                                     $80 { $80 = active Status },
                                     true { IOC },
                                     false { IOS },
                                     false { LS } ,
                                     1 { C_error: },
                                     false { SPD },
                                     $2d  { e1 = OUT 2d { 2d PID = SETUP },
                                     1 , { Deviceaddress }
                                     0 { EndPt },
                                     false { DataToggle },
                                     $7 { MaxLen },
                                     getPtrBase(@SetHubPortPowerFeature));
  sampleTD5:=CreateTransferDescriptor(true{ Terminate } ,
                                     false { Queue },
                                     false { Depth },
                                     0{ Link },
                                     $7ff { Actlen },
                                     $80 { $80=active Status },
                                     true { IOC },
                                     false { IOS },
                                     false { LS } ,
                                     1 { C_error: },
                                     false { SPD },
                                     $69 { 69 = PID = IN },
                                     1 { DeviceAddress },
                                     0 { EndPt },
                                     true { DataToggle },
                                     $7ff { MaxLen },
                                     getPtrBase(@testbuffer) { BufferPtr });
  { IF VR-mode then abort }
  if detectvirtualrealmode then
    begin
      writeln('System is running under V-Real Mode - abort');
      halt(3);
    end;
  { if no PCI-Bios then abort }
  if not detectPCIBios then
    begin
      writeln('PCI-BIOS not supported - abort!');
      halt(3);
    end;
  { Get USB-Controller (PCI-Configuration) }
  if USBdetect(DeviceId,VendorId,BusNumber,FunctionNumber,description) then
    begin
      writeln(description);
      writeln('---------------------');
      writeln('DeviceID:',hexs(DeviceId));
      writeln('VendorID:',hexs(VendorId));
      writeln('BusNumber:',hexs(BusNumber));
      writeln('FunctionNumber:',hexs(FunctionNumber));
    end;
  { Detect configured IO-Space Address }
  if USBGetDeviceIOSpace(IOSpace) then
      writeln('Maped I/O-Space: ',hexs(iospace));
  { Install IO-Space Address }
  if iospace<$100 then
  if USBSetDeviceIOSpace(preferredIOSpace) then
    if USBGetDeviceIOSpace(IOSpace) then
      writeln('Maped I/O-Space: ',hexs(iospace));
  { Enable PCI-Master }
  USBStatus;
  writeln('USB enable:',USBEnable);
  USBStatus;
  { Set Software programmable bit to check if IO-Space settings work }
  USBStatus;
  { Allocate page aligned Framelist }
  USBAllocateFrameList(FlPtr,FLbase);

  { Detect configured HW-Interrupt }
  USBGetInterruptNumber(hwint,hwactive);
  writelN('HW-Interrupt:',hwint:2,'  - Active:',hwactive);
  { Install HW-interrupt and Interrupt service routine }

  hwint:=preferredHWInterrupt;
  if hwint>7 then
    begin
      serviceInt:=hwint+$70;
      getintvec(serviceInt,oldUsbInt);
      setintvec(serviceInt,@slaveInterrupt);
    end else
    begin
      serviceInt:=hwint+$8;
      getintvec(serviceInt,oldUsbInt);
      setintvec(serviceInt,@masterInterrupt);
    end;
  USBSetInterruptNumber(9,false);
  USBSetInterruptNumber(hwint,True);
  { Re-Check... }
  USBGetInterruptNumber(hwint,hwactive);
  USBWriteInterruptReg($0f);  { enable all interrupt occurences }

  writelN('HW-Interrupt:',hwint:2,'  - Active:',hwactive);

  usbstatus;

  InsertTransferDescriptorInFrameList(00, SampleTD);
  InsertTransferDescriptorInFrameList(20, SampleTD1);
  InsertTransferDescriptorInFrameList(40, SampleTD2);
  InsertTransferDescriptorInFrameList(60, SampleTD3);
  InsertTransferDescriptorInFrameList(80, SampleTD4);
  InsertTransferDescriptorInFrameList(100, SampleTD5);

  DoItAgain:
  { wait for attachement }
  repeat
  until (usbReadPort0Reg and 1=1) or keypressed;
  usbstatus;
  usbWritePort0Reg(512+8+2);   { Port Reset }
  usbstatus;
  delay(50);
  usbstatus;
  usbWritePort0Reg($95);
  usbWritePort0Reg($4+8+2);
  usbWriteFrameNumberReg(0);
  usbcommandrun; { start from FrameList 0 }
  usbWritePort0Reg($4+8+2);
  usbWritePort0Reg($4+8+2);

  repeat
  usbWritePort0Reg($4+8+2);
       { Port Enable }    begin delay(100);usbstatus; end;
  if (sampleTD5^.status<>$80) then
    begin
     sampleTD5^.status:=$80;
     sampleTD4^.status:=$80;
     inc(SetHubPortPowerFeature.windex);
     if SetHubPortPowerFeature.windex>5 then
       begin
         SetHubPortPowerFeature.windex:=0;
         usbCommandstop;
         sampleTD^.status:=$80;
         sampleTD1^.status:=$80;
         sampleTD2^.status:=$80;
         sampleTD3^.status:=$80;
          goto DoItAgain;
       end;
    end;
  until keypressed;
  delay(10);
  usbcommandstop;
  usbstatus;
  writeln('Calls to USB interrupt:',dummytimer);
  USBWriteInterruptReg($00);  { Disable all interrupt occurences }
  setintvec(serviceInt,oldUsbInt);
  writeln('USB disable:',USBDisable);
  USBdone; { Restore old IRQ values }
  USBprintTD(sampleTD);
  USBprintTD(sampleTD1);
  USBprintTD(sampleTD2);
  USBprintTD(sampleTD3);
  USBprintTD(sampleTD4);
  USBprintTD(sampleTD5);
  writeln;

end.
