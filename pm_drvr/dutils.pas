{ DUTILS - Turbo Pascal UTILITIES                      }
{ (c) 1994 by Dieter Pawelczak                         }

{$R-}
{$D+}
{$S-}
unit DUTILS;
interface
uses DOS,crt;
const { Month names and number of days - used to display the date }
  MonthStr: array[1..12] of string[3] = ('Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun','Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec');
  DayStr: Array[0..6] of String[3] = ('Su.','Mo.','Tu.','We.','Th.','Fr.','Sa.');
  MonatStr: array[1..12] of string[3] = ('Jan', 'Feb', 'Mar', 'Apr', 'Mai', 'Jun','Jul', 'Aug', 'Sep', 'Okt', 'Nov', 'Dez');
  TagStr: Array[0..6] of String[3] = ('So.','Mo.','Di.','Mi.','Do.','Fr.','Sa.');
  MonthLen: Array[1..12] of Byte=(31,28,31,30,31,30,31,31,30,31,30,31);

var
  Path: PathStr;
  scrtyp:Word;


(*    File - Procedures                             *)

procedure copyfile(Source,Dest:String);
function Filelength(pth:string):LongInt;
procedure IsExt(VAR Filename:String;Ext:String);
FUNCTION GetExefilesize(Exename:String): LONGINT;
function getprgdir(prg:string):String;(* read directory of prg file      *)
function fileexist(fn:String):Boolean;

(*    CRT routines                                  *)

procedure cursoroff;
procedure cursoron;

procedure color(ccl,cch:Byte);        (* color (Fore -,background)       *)
function Bigletters(bl:String):String;(* German upcase                   *)


Procedure Twin(x1,y1,x2,y2:Byte);
Procedure Twin2(x1,y1,x2,y2:Byte);  (* Double          *)
Procedure Twin1(x1,y1,x2,y2:Byte);  (* Single          *)
Procedure Cwin2(x1,y1,x2,y2,attr:Byte);  (* Double with colour attributes     *)
Procedure Cwin1(x1,y1,x2,y2,attr:Byte);  (* single                            *)
Procedure tback;                    (* draw background                        *)
procedure shad(xx,yy:WORD);
procedure print(x1,y1:word;t:string);(* print with no attributes *)
procedure cprint(x1,y1:word;t:string;attr:byte);(* print with attributes *)

(* Number Conversions                               *)

procedure twodecout(xx:real);
procedure hexout(xx:word);          (* print hex number                   *)
function bins16(xx:word):String;
function bins8(xx:byte):String;
function bins(xx:longint):String;
 function hexs(xx:longint):String;
function hexs8(xx:byte):String;
function hexs16(xx:word):String;
function twodecs(xx:real):String;   (* convert real to string             *)
function decs(xx:longint;format:Byte):String;(* convert integer to String *)
function hextodec(s:string):longint;(* convert Hex to Longint             *)

(* Date and Time                                    *)

function getweekday(d,m,y:word):integer; (* d-day m-month y-year          *)
function date(typ:boolean):String;  (* convert date to string             *)
procedure stime;                      (* show time                       *)
function time:String;               (* get time               HH.MM.SS    *)
function timeexact:String;          (* get time               HH.MM.SS.hh *)

(* Keyboard  *)

function shiftpressed:Boolean;
function strpressed:Boolean;
function Altpressed:Boolean;
function altgrpressed:Boolean;




implementation


function shiftpressed:Boolean;
var std:byte;
begin
asm
  mov ah,2;
  int 16h
  mov std,al
  end;
shiftpressed:=(std and 1=1)or(std and 2=2);
end;

function strpressed:Boolean;
var std:byte;
begin
asm
  mov ah,2;
  int 16h
  mov std,al
  end;
strpressed:=(std and 4=4);
end;

function Altpressed:Boolean;
var std:byte;
begin
asm
  mov ah,2;
  int 16h
  mov std,al
  end;
Altpressed:=(std and 8=8);
end;

function altgrpressed:Boolean;
var std:byte;
begin
asm
  mov ah,12h;
  int 16h
  mov std,ah
  end;
altgrpressed:=(std and 8=8);
end;





function getweekday(d,m,y:word):integer;
var h1,s:longint;
    ii:Byte;
    iss:boolean;

begin
iss:=false;
if y mod 4=0 then iss:=true;
if y mod 100=0 then if (y mod 400<>0) then iss:=false;
s:=0;if y>1 then s:=(y-1) div 4-(y div 100)+(y div 400);
h1:=y+s;
if m>1 then for ii:=1 to m-1 do h1:=h1+longint(Monthlen[ii]);
if m>2 then if iss then h1:=h1+1;
h1:=h1+d;
getweekday:=h1 mod 7;
end;

function date(Typ:Boolean):String;
VAR yy,mm,dd,dw:WORD;
    TG:String;
begin
getdate(yy,mm,dd,dw);
if not typ then tg:=TagStr[dw] else tg:=daystr[dw];
date:=tg+', '+decs(dd,2)+'.'+decs(mm,2)+'.'+decs(yy,4);
end;

function time:String;
VAR hh,mm,ss,dw:WORD;
    TG,TI:String;
begin
gettime(hh,mm,ss,dw);
ti:=decs(mm,2);if ti[1]=' ' then ti[1]:='0';
tg:=decs(hh,2)+'.'+ti;
ti:=decs(ss,2);if ti[1]=' ' then ti[1]:='0';
tg:=tg+'.'+ti;time:=tg;
end;

function timeexact:String;
VAR hh,mm,ss,dw:WORD;
    TG,TI:String;
begin
gettime(hh,mm,ss,dw);
ti:=decs(mm,2);if ti[1]=' ' then ti[1]:='0';
tg:=decs(hh,2)+'.'+ti;
ti:=decs(ss,2);if ti[1]=' ' then ti[1]:='0';
tg:=tg+'.'+ti;
ti:=decs(dw,2);if ti[1]=' ' then ti[1]:='0';
tg:=tg+'.'+ti;timeexact:=tg;
end;

{$I-}
function fileexist(fn:String):Boolean;
var ff:text;
   i:integer;
begin
i:=ioresult;
Assign(ff,fn);reset(ff);
if ioresult=0 then
  begin close(ff); fileexist:=true end else fileexist:=false;


end;

function single(s:char):longint;
var i:longint;
begin
if ord(s)>58 then i:=ord(s)-55 else i:=ord(s)-48;
single:=i;
end;

function hextodec(s:string):longint;
var i:longint;
begin
s:=bigletters(s);
if pos('$',s)=1 then s:=copy(s,2,length(s)-1);
while s[length(s)]=' ' do s:=copy(s,1,length(s)-1);
i:=0;
while length(s)<>0
do
begin
if length(s)=5 then i:=i+65536*single(s[1]);
if length(s)=4 then i:=i+4096*single(s[1]);
if length(s)=3 then i:=i+256*single(s[1]);
if length(s)=2 then i:=i+16*single(s[1]);
if length(s)=1 then i:=i+single(s[1]);
s:=copy(s,2,length(s)-1);
end;
hextodec:=i;
end;

procedure cprint(x1,y1:word;t:string;attr:byte);
var h2,h3:WORD;
begin
t:=t+#0;
h2:=ofs(t)+1;
h3:=seg(t);
      asm
      push ds
      push es
      push si
      push di
      mov ax,h3
      mov es,ax
      mov ax,y1
      dec ax
      mov dx,$00A0
      mul dx
      mov bx,ax
      mov ax,x1
      dec ax
      shl ax,1
      add ax,bx
      mov di,ax
      mov si,h2
      mov ax,ScrTyp;
      mov ds,ax;
      mov bh,attr
@002:
      mov bl,es:[si]
      cmp bl,0
      je @003
      moV ds:[di],bx
      inc di
      inc di
      inc si
      jmp @002
@003:
      pop di
      pop si
      pop es
      pop ds
      end;
end;
procedure print(x1,y1:word;t:string);
var h2,h3:WORD;
begin
t:=t+#0;
h2:=ofs(t)+1;
h3:=seg(t);
      asm
      push ds
      push es
      push si
      push di
      mov ax,h3
      mov es,ax
      mov ax,y1
      dec ax
      mov dx,$00A0
      mul dx
      mov bx,ax
      mov ax,x1
      dec ax
      shl ax,1
      add ax,bx
      mov di,ax
      mov si,h2
      mov ax,ScrTyp;
      mov ds,ax;
@002:
      mov bl,es:[si]
      cmp bl,0
      je @003
      moV ds:[di],bl
      inc di
      inc di
      inc si
      jmp @002
@003:
      pop di
      pop si
      pop es
      pop ds
      end;
end;

procedure color(ccl,cch:Byte);
begin
textcolor(ccl);TextBackground(cch);
end;

function NumStr(N, D: Integer): String;
begin
  NumStr[0] := Chr(D);
  while D > 0 do
  begin
    NumStr[D] := Chr(N mod 10 + Ord('0'));
    N := N div 10;
    Dec(D);
  end;
end;

function getprgdir(prg:string):String;
var nam:Namestr;
    ext:Extstr;
    pth:pathstr;
    umg:dirstr;
    s:string;
begin
  s:=FSEARCH(prg,'*.*');
  if s='' then s:=FSearch(prg,getenv('PATH'));
  pth:=s;
  Fsplit(pth,umg,nam,ext);
  getprgdir:=umg;
end;

procedure IsExt(VAR Filename:String;Ext:String);
begin
if pos('.',Filename)=0 then Filename:=Filename+Ext;
end;

function Filelength(pth:string):LongInt;
var i:longint;
    fi:file;
begin
{$I-}
assign(fi,pth);reset(fi,1);i:=0;
IF IOResult=0 then i:=FileSize(fi);
{$I+}
close(fi);
Filelength:=i;
end;



procedure stime;
var
hour,min,sec,sec100:word;
z:string[3];
zeit:string[10];

begin
  gettime(hour,min,sec,sec100);
        zeit:='';
        str(hour,z);if hour<10 then z:='0'+z;
        zeit:=z+':';
        str(min,z);if min<10 then z:='0'+z;
        zeit:=zeit+z+':';
        str(sec,z);if sec<10 then z:='0'+z;
        zeit:='<'+zeit+z+'>';
        print(70,25,zeit);
end;
function Bigletters(bl:String):String;
var i:Byte;
begin
for i:=1 to length(bl) do
begin
if (bl[i]>='a') and (bl[i]<='z') then bl[i]:=CHR(ord(bl[i])-32);
if bl[i]='„' then bl[i]:='Ž';
if bl[i]='”' then bl[i]:='™';
if bl[i]='' then bl[i]:='š';
end;
Bigletters:=bl;
end;





Procedure Shad(xx,yy:Word);
BEGIN
if xx<>80 then
asm
  mov ax,ScrTyp;
  mov es,ax
  mov ax,yy
  mov dx,160
  mul dx
  mov bx,ax
  mov ax,xx
  mov dx,2
  mul dx
  add ax,bx
  mov di,ax
  mov al,es:[di]
  cmp al,$B2
  jne @002
  mov al,$B0
  mov es:[di],al
@002:
  end;
end;

Procedure Twin(x1,y1,x2,y2:Byte);
var b,c,d:String[80];
    i:Byte;
begin
  b:='';c:='';d:='';if y2>25 then y2:=25;if x2>80 then x2:=80;
  for i:=x1 to x2-2 do
    begin
      b:=b+chr(32);d:=d+'Í';
    end;
c:='É'+d+'»';gotoxy(x1,y1);write(c);
  b:='º'+b+'º';
  for i:=y1 to y2-2 do
    begin
      gotoxy(x1,i+1);write(b);
    end;
c:='È'+d+'¼';gotoxy(x1,y2);write(c);
  end;
Procedure Twin2(x1,y1,x2,y2:Byte);
var b,c,d:String[80];
    i:Byte;
begin
  b:='';c:='';d:='';if y2>25 then y2:=25;if x2>80 then x2:=80;
  for i:=x1 to x2-2 do
    begin
      b:=b+chr(32);d:=d+'Í';
    end;
c:='É'+d+'»';gotoxy(x1,y1);write(c);shad(x2,y1);
  b:='º'+b+'º';
  for i:=y1 to y2-2 do
    begin
      gotoxy(x1,i+1);write(b);shad(x2,i+1);
    end;
c:='È'+d+'¼';gotoxy(x1,y2);write(c);
for i:=x1+1 to x2 do shad(i,y2);
  end;
Procedure Twin1(x1,y1,x2,y2:Byte);
var b,c,d:String[80];
    i:Byte;
begin
  b:='';c:='';d:='';if y2>25 then y2:=25;
  if x2>80 then x2:=80;
  for i:=x1 to x2-2 do
    begin
      b:=b+chr(32);d:=d+'Ä';
    end;
c:='Ú'+d+'¿';shad(x2,y1-1);shad(x2,y1);
gotoxy(x1,y1);write(c);
  b:='³'+b+'³';
  for i:=y1 to y2-2 do
    begin
      gotoxy(x1,i+1);write(b);shad(x2,i+1);
    end;
  c:='À'+d+'Ù';
  gotoxy(x1,y2);write(c);
  for i:=x1+1 to x2 do shad(i,y2);
end;
Procedure Cwin2(x1,y1,x2,y2,attr:Byte);
var b,c,d:String[80];
    i:Byte;
begin
  b:='';c:='';d:='';if y2>24 then y2:=24;if x2>80 then x2:=80;
  for i:=x1 to x2-2 do
    begin
      b:=b+chr(32);d:=d+'Í';
    end;
c:='É'+d+'»';cprint(x1,y1,c,attr);shad(x2,y1);
  b:='º'+b+'º';
  for i:=y1 to y2-2 do
    begin
      cprint(x1,i+1,b,attr);shad(x2,i+1);
    end;
c:='È'+d+'¼';cprint(x1,y2,c,attr);
for i:=x1+1 to x2 do shad(i,y2);
  end;
Procedure Cwin1(x1,y1,x2,y2,attr:Byte);
var b,c,d:String[80];
    i:Byte;
begin
  b:='';c:='';d:='';if y2>24 then y2:=24;
  if x2>80 then x2:=80;
  for i:=x1 to x2-2 do
    begin
      b:=b+chr(32);d:=d+'Ä';
    end;
c:='Ú'+d+'¿';shad(x2,y1-1);shad(x2,y1);
cprint(x1,y1,c,attr);
  b:='³'+b+'³';
  for i:=y1 to y2-2 do
    begin
      cprint(x1,i+1,b,attr);shad(x2,i+1);
    end;
  c:='À'+d+'Ù';
  cprint(x1,y2,c,attr);
  for i:=x1+1 to x2 do shad(i,y2);
end;

Procedure TBack;
begin
asm
  mov ax,ScrTyp;
  mov es,ax
  mov di,160
  mov al,$B2
@001:
  mov es:[di],al
  inc di
  inc di
  cmp di,3840
  jb @001
end;
end;
function twodecs(xx:real):String;
var h1,h2,h3:longint;
s,ss:String[8];
begin
xx:=xx*100;
h1:=trunc(xx);
h2:=h1 div 100;
h3:=h1 mod 100;
if h3<0 then h3:=-h3;
str(h2,ss);
str(h3,s);if h3<10 then s:='0'+s;
twodecs:=ss+'.'+s;
end;

procedure twodecout(xx:real);
begin
write(twodecs(xx));
end;
procedure hexout(xx:word);
begin
write(hexs(xx));
end;
function decs(xx:longint;format:Byte):String;
var s:string[10];
begin
str(xx,s);
while length(s)<format do s:=' '+s;
decs:=s;
end;

function hexs(xx:longint):String;
var ss:String[8];
    h1,h2:Byte;
begin
ss:='';
  h1:=32;
  repeat
    dec(h1,4);
    h2:=(xx shr h1) and $f;
    if h2>9 then h2:=h2+7;
    h2:=h2+48;
    ss:=ss+chr(h2);
  until h1=0;
hexs:=ss;
end;

function hexs16(xx:word):String;
var ss:String[8];
    h1,h2:Byte;
begin
ss:='';
  h1:=16;
  repeat
    dec(h1,4);
    h2:=(xx shr h1) and $f;
    if h2>9 then h2:=h2+7;
    h2:=h2+48;
    ss:=ss+chr(h2);
  until h1=0;
hexs16:=ss;
end;

function hexs8(xx:byte):String;
var ss:String[8];
    h1,h2:Byte;
begin
ss:='';
  h1:=8;
  repeat
    dec(h1,4);
    h2:=(xx shr h1) and $f;
    if h2>9 then h2:=h2+7;
    h2:=h2+48;
    ss:=ss+chr(h2);
  until h1=0;
hexs8:=ss;
end;


function bins(xx:longint):String;
var ss:String[32];
    h1,h2:Byte;
begin
ss:='';
  h1:=32;
  repeat
    dec(h1);
    h2:=(xx shr h1) and $1;
    h2:=h2+48;
    ss:=ss+chr(h2);
  until h1=0;
bins:=ss;
end;

function bins16(xx:word):String;
var ss:String[32];
    h1,h2:Byte;
begin
ss:='';
  h1:=16;
  repeat
    dec(h1);
    h2:=(xx shr h1) and $1;
    h2:=h2+48;
    ss:=ss+chr(h2);
  until h1=0;
bins16:=ss;
end;

function bins8(xx:byte):String;
var ss:String[32];
    h1,h2:Byte;
begin
ss:='';
  h1:=8;
  repeat
    dec(h1);
    h2:=(xx shr h1) and $1;
    h2:=h2+48;
    ss:=ss+chr(h2);
  until h1=0;
bins8:=ss;
end;

procedure copyfile(Source,Dest:String);
var FROMF, TOF:FILE;
Numread,Numwrite:WORD;
BUF:array[0..3071] of CHAR;
var ds,ss:String;
begin
ds:=Dest;ss:=source;
if (ds='')  then ds:=copy(ss,1,pos('.',ss))+'bak';
assign(FRomF,Source);Reset(FROMF,1);
assign(TOF,Ds);Rewrite(ToF,1);
writeLn('Copy file(s):'+Source+' to:'+dest);
repeat
  Blockread(FromF,Buf,SizeOf(Buf),Numread);
  Blockwrite(TOF,Buf,Numread,Numwrite);
until (Numread=0) or (Numwrite<>Numread) or (Numread<>SizeOF(BUF));
Close(Fromf);
Close(Tof);
end;

FUNCTION GetExefilesize(Exename:String): LONGINT;
VAR
  ExeFile: FILE OF BYTE;
  IDByte : ARRAY[1..2] OF BYTE;
  g      : ARRAY[1..4] OF BYTE;
  g1,g2,g3,g4:longint;
  Ioerror:integer;
BEGIN
  Assign(ExeFile, ExeName);
  Reset(ExeFile);
  IOError := IOResult;
  IF IOError <> 0 THEN exit;
  Read(ExeFile, IDByte[1]);
  Read(ExeFile, IDByte[2]);
  IF (Chr(IDByte[1]) = 'M') AND (Chr(IDByte[2]) = 'Z') THEN
                     BEGIN                            (* EXE *)
    Read(ExeFile, g[1]);g1:=g[1];
    Read(ExeFile, g[2]);g2:=g[2];
    Read(ExeFile, g[3]);g3:=g[3];
    Read(ExeFile, g[4]);g4:=g[4];
    Close(ExeFile);
    IF (g[1] = 0) AND (g[2] = 0) THEN
      Getexefilesize :=
          g4 * 256 + g3
    ELSE
      Getexefilesize :=
        (g4 * 256 + g3 - 1) * 512 + (g2 * 256 +
          g1);
  END
  ELSE
  Getexefilesize := 0;
END;

procedure cursoroff; assembler;
 asm
   mov AH,01h
   mov CH,32
   mov CL,1
   int 10h
 end;

procedure cursoron; assembler;
 asm
   mov AH,01h
   mov CH,0
   mov CL,1
   int 10h
 end;

begin
ScrTyp:=$B800;Randomize;
if Byte(Ptr($40,$49)^)=7 then ScrTyp:=$b000;
end.