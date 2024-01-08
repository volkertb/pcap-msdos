Program NDIS_test;   { Network Driver Interface Spec. ver 2.0.1 }
                     { Implemented by protocol manager          }
{$M 4096, 0, 0}

Uses Dos, TpASCIIZ, TpBCD, TpString;


Type Arr16B      = Array [0..15]  of Byte;
     Arr3B       = Array [0..2]   of Byte;


     RequestType = Record
                     Func   : Word;
                     Status : Word;
                     Ptr1   : Pointer;
                     Ptr2   : Pointer;
                     RetVal : Word;
                   end;

     CCTtype = Record                    { Common Characteristics Table }
                 Size         : Word;    { found by NDIS function 2     }
                 NDIS_MajVer  : Byte;
                 NDIS_MinVer  : Byte;
                 Reserved     : Word;
                 ModMajVer    : Byte;
                 ModMinVer    : Byte;
                 ModFunctions : LongInt;
                 ModName      : Arr16B;
                 UpperBPL     : Byte;
                 UpperBIT     : Byte;
                 LowerBPL     : Byte;
                 LowerBIT     : Byte;
                 ModuleID     : Word;
                 ModuleDS     : Word;
                 SysReqEntry  : Pointer;
                 ServSpecChar : Pointer;
                 ServSpecStat : Pointer;
                 UpperDT      : Pointer;
                 LowerDT      : Pointer;
                 Reserved0a   : LongInt; { must be 0 }
                 Reserved0b   : LongInt; { must be 0 }
             end;

     MAC_SSCTtype = Record { MAC Service Specific Characteristics Table     }
                           { found by NDIS function 2 and CCT.ServSpecChar^ }

                      Size              : Word;
                      TypeName          : Arr16B;
                      StationAdrLen     : Word;
                      StationAdrPerm    : Arr16B;
                      StationAdrCurrent : Arr16B;
                      AdapterAdrCurrent : Pointer;
                      MC_address        : Pointer;
                      LinkSpeed         : LongInt;
                      ServFlags         : LongInt;
                      MaxFrameSize      : Word;
                      Total_TXbuffer    : Longint;
                      Alloc_TXbuffer    : Word;
                      Total_RXbuffer    : Longint;
                      Alloc_RXbuffer    : Word;
                      IEEE_vendor       : Arr3B;
                      VendorAdapterCode : Byte;
                      VendorDesc        : Pointer;
                      AdapterIRQ        : Word;
                      TX_queueDepth     : Word;
                      Max_RBD           : Word;
                      VendorInfo        : Arr16B;  { infact N bytes, use }
                    end;                           { Size to calculate   }

     MAC_SSSTtype = Record { MAC Service Specific Status Table              }
                           { found by NDIS function 2 and CCT.ServSpecStat^ }
                      Size            : Word;
                      LastDiagnostics : LongInt;
                      Status          : LongInt;
                      PacketFilter    : Word;
                      MediaStatus     : Pointer;
                      LastClearStat   : LongInt;
                      NumRX_Frames    : LongInt;
                      NumRX_CRC       : LongInt;
                      NumRX_Bytes     : LongInt;
                      NumDiscarded    : LongInt;
                      MC_RX_Frames    : LongInt;
                      BC_RX_Frames    : LongInt;
                      NumFrameError   : LongInt;
                      TooLargeFrames  : LongInt;
                      TooSmallFrames  : LongInt;
                      NumMC_Bytes     : LongInt;
                      NumBC_Bytes     : LongInt;
                      NumFrameDiscard : LongInt;
                      NumTX_Frames    : LongInt;
                      NumTX_Bytes     : LongInt;
                      MC_TX_Frames    : LongInt;
                      BC_TX_Frames    : LongInt;
                      BC_TX_Bytes     : LongInt;
                      MC_TX_Bytes     : LongInt;
                      TX_Timeout      : LongInt;
                      TX_HardwErr     : LongInt;
                      VendorInfo      : Arr16B;
                    end;

     NBP_SSCTtype = Record { NetBIOS Service Specific Characteristics Table }
                           { found by NDIS function 2 and CCT.ServSpecChar^ }
                      Size       : Word;
                      ModuleName : Arr16B;
                      ModuleCode : Word;
                      VendorInfo : Arr16B;
                    end;


Const UpperBPLstr : Array [1..5] of String[20] =
                    ('MAC','Data Link','Network','Transport','Session');
      LowerBPLstr : Array [0..5] of String[20] =
                    ('Physical','MAC','Data Link','Network','Transport','Session');

      MAC           : Boolean    = False;
      NBP           : Boolean    = False;
      NBP_Module    : String[24] = 'C:\LAN\DRIVERS\NBP.EXE'+#0;
      UserConfig    : String[21] = 'C:\LAN\PROTOCOL.INI'+#0;

Var ProtMan    : File;
    Req        : RequestType;
    CCT        : ^CCTtype;
    MAC_SSCT   : ^MAC_SSCTtype;
    MAC_SSST   : ^MAC_SSSTtype;
    NBP_SSCT   : ^NBP_SSCTtype;
    L          : LongInt;
    Level1     : Byte;
    Level2     : Byte;
    i          : Byte;


{**************************************************************************}

PROCEDURE WriteNDISstatus;
Var R : String;
begin
  Case Req.Status of
   $0000 : R := 'Success';
   $0001 : R := 'Wait for release';
   $0002 : R := 'Request queued';
   $0003 : R := 'Frame not recognized';
   $0004 : R := 'Frame rejected';
   $0005 : R := 'Frame could not be forwarded';
   $0006 : R := 'Out of resource';
   $0007 : R := 'Invalid parameter';
   $0008 : R := 'Invalid function';
   $0009 : R := 'Not supported';
   $000A : R := 'Hardware error';
   $000B : R := 'Transmit error';
   $000C : R := 'Unrecognized destination';
   $000D : R := 'Buffer too small';
   $0020 : R := 'Already started';
   $0021 : R := 'Binding incomplete';
   $0022 : R := 'Driver not initialized';
   $0023 : R := 'Hardware not found';
   $0024 : R := 'Hardware failure';
   $0025 : R := 'Configuration failure';
   $0026 : R := 'Interrupt conflict';
   $0027 : R := 'MAC incompatible';
   $0028 : R := 'Initialization failed';
   $0029 : R := 'No binding';
   $002A : R := 'Network may be disconnected';
   $002B : R := 'Incompatible DOS version';
   $002C : R := 'Already registered';
   $002D : R := 'Path not found';
   $002E : R := 'Insufficient memory';
   $002F : R := 'Information not found';
   $00FF,
   $F000..$FFFF : R := 'General failure';
   else           R := 'Unexpected error';
  end;
  Writeln;
  Writeln('* ',Req.Func,' NDIS: ',R);
end;

{**************************************************************************}

FUNCTION ASCIIZ2Str(P:Pointer) : String;
Var Temp : ASCIIZ;
begin
  Move(P^,Temp,MaxASCIIZ);
  ASCIIZ2Str := TpASCIIZ.Asc2Str(Temp);
end;

{**************************************************************************}

PROCEDURE DoRequest (Func:Word);
Var R : Registers;
begin
   Req.Status := $100;
   Req.Func   := Func;
   R.DS := Seg(Req);
   R.DX := Ofs(Req);
   R.AX := $4402;
   R.BX := FileRec(ProtMan).Handle;  { filehandle for PROTMAN$  }
   R.CX := 14;                       { size of request block    }
   MSDOS(R);
   WriteNDISstatus;
end;

{**************************************************************************}

PROCEDURE ClrScr; Assembler;
ASM
    mov ax,0600h
    mov bh, 16*7
    mov dx, 256*50+80
    xor cx,cx
    int 10h
end;

{**************************************************************************}

PROCEDURE Abort(S:String); begin Writeln(S); Halt(1); end;

{**************************************************************************}

PROCEDURE GetProtocolManagerInfo;
begin
  Req.Ptr1 := @UserConfig;
  DoRequest (1);
end;

{**************************************************************************}

PROCEDURE RegisterModule;
begin
  Req.Ptr1 := NIL;
  Req.Ptr2 := @NBP_Module;
  DoRequest (2);
end;

{**************************************************************************}

PROCEDURE BindAndStart;
begin
  DoRequest (3);
end;

{**************************************************************************}

PROCEDURE GetProtocolManagerLinkage;
begin
  Req.Ptr1 := NIL;
  Req.Ptr2 := NIL;
  DoRequest(4);
  Writeln('Linker''s dispatch point : ',HexPtr(Req.Ptr1));
  Writeln('PROTMAN''s DS            : ',HexW(Req.RetVal));
end;

{**************************************************************************}

PROCEDURE GetProtocolIniPath;
Var IniPath : ASCIIZ;
begin
  Req.Ptr1 := @IniPath;
  DoRequest(5);
  Writeln('PROTOCOL.INI path : ',Asc2Str(IniPath));
end;

{**************************************************************************}

PROCEDURE RegisterProtocolManagerInfo;
begin
  Req.Ptr2 := @UserConfig;
  DoRequest(6);
end;

{**************************************************************************}

PROCEDURE InitAndRegister;
begin
  Req.Ptr2 := @NBP_module;
  DoRequest(7);
end;

{**************************************************************************}

PROCEDURE UnbindAndStop;
begin DoRequest(8); end;

{**************************************************************************}

PROCEDURE BindStatus;
begin
  Req.Ptr1 := NIL;
  DoRequest(9);
end;

{**************************************************************************}

PROCEDURE RegisterStatus;
begin
  Req.Ptr1 := NIL;
  Req.Ptr2 := @NBP_Module;
  Req.RetVal := 0;
  DoRequest(10);
end;

{**************************************************************************}

begin

   FileMode := 0;                  { read-only filemode }
   Assign(ProtMan,'PROTMAN$');
   {$I-}
   Reset(ProtMan,1);
   {$I+}
   if IOresult <> 0 then
      Abort('NDIS Protocol manager not found.');

   GetProtocolManagerInfo;

   i := LO(Req.RetVal);
   Writeln('NDIS ver. ',HI(Req.RetVal),'.'
           ,i shr 4,'.',i and 15,
           ' (based on PROTMAN)');

   GetProtocolManagerLinkage;
   GetProtocolIniPath;
   RegisterStatus;
   BindStatus;
   CCT := Req.Ptr1;       { Common Characteristics Table }

   Writeln('NDIS ver. '  ,CCT^.NDIS_MajVer,'.',CCT^.NDIS_MinVer);
   Writeln('Module ver. ',CCT^.ModMajVer,  '.',CCT^.ModMinVer);
   Write('Module functions: ');

   Writeln('Module name: ',ASCIIZ2Str(@CCT^.ModName));

   L := CCT^.ModFunctions;
   if (L and 1)=1 then
      Writeln('Binding at upper boundary level supported.');
   if (L and 2)=2 then
      Writeln('Binding at lower boundary level supported.');
   if (L and 4)=4 then
      Writeln('Dynamically boundary supported.');

   Level1 := CCT^.UpperBPL;
   Write('Upper Boundary Protocol Level: ');
   if Level1 = $FF then
        Writeln('not specified')
   else Writeln(Level1,' (',UpperBPLstr[Level1],').');

   Write('Upper Boundary Interface Level: ');
   Level2 := CCT^.UpperBIT;
   Case Level1 of
     1   : if Level2 = 1 then Writeln('MAC');
     5   : if Level2 = 1 then Writeln('NetBIOS');
     2,4 : Writeln('to be defined');
   end;

   Level1 := CCT^.LowerBPL;
   Write('Lower Boundary Protocol Level: ');
   if Level1 = $FF then
        Writeln('not specified')
   else Writeln(Level1,' (',LowerBPLstr[Level1],').');

   Write('Lower Boundary Interface Level: ');
   Level2 := CCT^.LowerBIT;
   Case Level1 of
     1   : if Level2 = 1 then Writeln('MAC');
     5   : if Level2 = 1 then Writeln('NetBIOS');
     2,4 : Writeln('to be defined');
   end;

   Writeln('Module ID: ',CCT^.ModuleID);
   Writeln('Module DS: ',HexW(CCT^.ModuleDS));
   Writeln('System Request Entry point: ',HexPtr(CCT^.SysReqEntry));

   if (CCT^.UpperBPL = 1) and (CCT^.UpperBIT = 1) then
   begin
     MAC      := True;
     MAC_SSCT := CCT^.ServSpecChar;
     MAC_SSST := CCT^.ServSpecStat;
   end;

   if (CCT^.UpperBPL = 5) and (CCT^.UpperBIT = 1) then
   begin
     NBP      := True;
     NBP_SSCT := CCT^.ServSpecChar;
   end;

   if MAC then
   begin
     Writeln('MAC Service-Specific Characteristics Table:');
     if (MAC_SSCT=Ptr(0,0)) then Writeln(' not found')
     else
     begin
       Writeln('MAC type: ',ASCIIZ2Str(@MAC_SSCT^.TypeName));
       Writeln('Permanent station address: ');
       for i:=0 to (MAC_SSCT^.StationAdrLen-1) do
          Write(HexB(MAC_SSCT^.StationAdrPerm[i]),' ');
       Writeln;
       Writeln('Current station address: ');
       for i:=0 to (MAC_SSCT^.StationAdrLen-1) do
          Write(HexB(MAC_SSCT^.StationAdrCurrent[i]),' ');
       Writeln;
       Writeln('Current functional adapter address: ',
               HexPtr(MAC_SSCT^.AdapterAdrCurrent));
       Writeln('Multicast address at ',
               HexPtr(MAC_SSCT^.MC_Address));
       Writeln('Link speed (bit/sec): ',MAC_SSCT^.LinkSpeed);
       L := MAC_SSCT^.ServFlags;
       Write('Supports: ');
       if (L and    1)=   1 then Write('Broadcast, ');
       if (L and    2)=   2 then Write('Multicast, ');
       if (L and    4)=   4 then Writeln('Group addressing,');
       if (L and    8)=   8 then Write('Promiscuous mode, ');
       if (L and   16)=  16 then Write('Setting station address, ');
       if (L and   32)=  32 then Write('Fresh statistics, ');
       if (L and   64)=  64 then Write('InitiateDiagnostics, ');
       if (L and  128)= 128 then Write('Loopback test, ');
       if (L and  256)= 256 then Write('ReceiveChain indication, ');
       if (L and  256)= 256 then Write('IBM source routing, ');
       if (L and  256)= 256 then Write('MAC reset, ');
       if (L and  512)= 512 then Write('Open/close adapter, ');
       if (L and 1024)=1024 then Write('Interrupt request, ');
       if (L and 2048)=2048 then Write('Source roting bridge, ');
       if (L and 4096)=4096 then Write('GDT virtual addresses, ');
       if (L and 4096)=4096 then Write('Multiple data transfer, ');
       if (L and 8192)=8192 then Write('FrameSize=0 in ReceiveLookAhead, ');
     end;

     Writeln('MAC Service-Specific Status Table:');
     if (MAC_SSST=Ptr(0,0)) then
        Writeln(' not found')
     else
     begin

     end;
   end;

   if NBP then
   begin
     Writeln('NetBIOS Service-Specific Status Table:');
     if (NBP_SSCT=Ptr(0,0)) then
        Writeln(' not found')
     else
     begin

     end;
   end;
   Close(ProtMan);
end.
