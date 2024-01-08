{ ************************************************************************ }
{ PCI.PAS:  PCI BIOS Routines by Dieter R. Pawelczak <dieterp@bigfoot.de>  }
{ ======================================================================== }
{                                                                          }
{              Unit to detect PCI-Devices and to read/write to             }
{              its configuration registers                                 }
{                                                                          }
{ (c) 1998 by Dieter Pawelczak, <dieterp@bigfoot.de>                       }
{ This is public domain Software - selling this software is prohibeted!    }
{                                                                          }
{ ************************************************************************ }

{$G+}


unit PCI;
interface

function detectPCIBios:boolean;
function detectPCIdevice(DeviceID:Word;VendorID:Word;VAR BusNumber:Byte;VAR FunctionNumber:Byte):boolean;
function readPCIRegisterByte(RegisterNumber:word;BusNumber:Byte;FunctionNumber:Byte;var result:byte):boolean;
function readPCIRegisterWord(RegisterNumber:word;BusNumber:Byte;FunctionNumber:Byte;var result:Word):boolean;
function readPCIRegisterDWord(RegisterNumber:word;BusNumber:Byte;FunctionNumber:Byte;var result:longint):boolean;
function writePCIRegisterByte(RegisterNumber:word;BusNumber:Byte;FunctionNumber:Byte;input:byte):boolean;
function writePCIRegisterWord(RegisterNumber:word;BusNumber:Byte;FunctionNumber:Byte;input:word):boolean;
function writePCIRegisterDWord(RegisterNumber:word;BusNumber:Byte;FunctionNumber:Byte;input:longint):boolean;

implementation

function detectPCIBios:boolean;assembler;
asm
    mov ax,0b101h
    int 1ah
    jc @nopcibios
    mov ax,1
    ret
  @nopcibios:
    xor ax,ax
    ret
end;

function detectPCIdevice(DeviceID:Word;VendorID:Word;VAR BusNumber:Byte;VAR FunctionNumber:Byte):boolean;
var found:boolean;
    bn:byte;
    fn:byte;
begin
  bn:=0;fn:=0;
  found:=false;
  asm
    db 66h;pusha
    mov cx,DeviceID
    mov dx,VendorID
    mov ax,0b102h
    xor si,si
    int 1ah
    jc @nodevice
    mov found,true
    mov bn,BH
    mov fn,BL
  @nodevice:
    db 66h;popa
  end;
  BusNumber:=bn;FunctionNumber:=fn;
  detectPCIdevice:=found;
end;

function readPCIRegisterByte(RegisterNumber:word;BusNumber:Byte;FunctionNumber:Byte;var result:byte):boolean;
var okay:boolean;
    res:byte;
begin
  okay:=false;
  res:=0;
  asm
    db 66h; pusha
    mov AX,0B108h
    mov BH,busNumber
    mov BL,functionNumber
    mov DI,RegisterNumber
    int 1Ah
    jc @noaction
    mov res,cl
    mov okay,true
  @noaction:
    db 66h; popa
  end;
  result:=res;
  readPCIRegisterByte:=okay;
end;

function readPCIRegisterWord(RegisterNumber:word;BusNumber:Byte;FunctionNumber:Byte;var result:Word):boolean;
var okay:boolean;
    res:word;
begin
  okay:=false;
  res:=0;
  asm
    db 66h; pusha
    mov AX,0B109h
    mov BH,busNumber
    mov BL,functionNumber
    mov DI,RegisterNumber
    int 1Ah
    jc @noaction
    mov res,cx
    mov okay,true
  @noaction:
    db 66h; popa
  end;
  result:=res;
  readPCIRegisterWord:=okay;
end;

function readPCIRegisterDWord(RegisterNumber:word;BusNumber:Byte;FunctionNumber:Byte;var result:longint):boolean;
var okay:boolean;
    res:longint;
begin
  okay:=false;
  res:=0;
  asm
    db 66h; pusha
    mov AX,0B10ah
    mov BH,busNumber
    mov BL,functionNumber
    mov DI,RegisterNumber
    int 1Ah
    jc @noaction
    db 66h; mov word ptr res,cx { MOV RES, ECX }
    mov okay,true
  @noaction:
    db 66h; popa
  end;
  result:=res;
  readPCIRegisterDword:=okay;
end;

function writePCIRegisterByte(RegisterNumber:word;BusNumber:Byte;FunctionNumber:Byte;input:byte):boolean;
var okay:boolean;
begin
  okay:=false;
  asm
    db 66h; pusha
    mov AX,0B10bh
    mov BH,busNumber
    mov BL,functionNumber
    mov DI,RegisterNumber
    mov CL,input
    int 1Ah
    jc @noaction
    mov okay,true
  @noaction:
    db 66h; popa
  end;
  writePCIRegisterByte:=okay;
end;

function writePCIRegisterWord(RegisterNumber:word;BusNumber:Byte;FunctionNumber:Byte;input:word):boolean;
var okay:boolean;
begin
  okay:=false;
  asm
    db 66h; pusha
    mov AX,0B10ch
    mov BH,busNumber
    mov BL,functionNumber
    mov DI,RegisterNumber
    mov CX,input
    int 1Ah
    jc @noaction
    mov okay,true
  @noaction:
    db 66h; popa
  end;
  writePCIRegisterWord:=okay;
end;

function writePCIRegisterDWord(RegisterNumber:word;BusNumber:Byte;FunctionNumber:Byte;input:longint):boolean;
var okay:boolean;
    res:byte;
begin
  okay:=false;
  res:=0;
  asm
    db 66h; pusha
    mov AX,0B10dh
    mov BH,busNumber
    mov BL,functionNumber
    mov DI,RegisterNumber
    db 66h; mov CX, word ptr input
    int 1Ah
    jc @noaction
    mov okay,true
  @noaction:
    db 66h; popa
  end;
  writePCIRegisterDWord:=okay;
end;

end.
