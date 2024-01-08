
.386
.MODEL FLAT,C

INCLUDE dosx.ah

.CODE

comment ~

*********************************************************************

call_child (LDEXP_BLK *ldblkp, dword ndword_params, dword param1, ...)

Description:
  Does a FAR call to the child program, returns when it returns.
  All regs except EAX are preserved;  EAX contains the value returned
  by the child.

  The child gets all parameter except the ldblkp pointer on the child's
  stack.  (Passing the parameter count allows this routine to avoid
  self-modifying code and therefore be reentrant.
  The child params have to be popped after the child returns in order
  to get to parent context saved on the child's stack).
  Note all parameters are passed to the child by value.

  For example, if ldblk has the addr of func child(), then use
          call_child (&ldblkp, 2, a, b);
  to make a FAR call to child() defined as follows:
          int _far child (ULONG nparams, ULONG param1, ULONG param2);

  NOTE the child must not switch stacks before returning, and must not
  modify the parameter count on the stack.

Calling arguments:
  ldblkp          ptr to param block ret'd by load EXP file system call
  ndword_params   number of 4-byte params (following count) to pass to
                  function

Returned values:
  Value returned by child program

*****************************************************************************
~

PUBLIC @call_child
@call_child PROC
    ARG loadBlockPtr:PTR, num_dwords:DWORD

    push    ebx                        ; save regs
    push    ecx
    push    edx
    push    esi
    push    edi
    push    ds
    push    es
    push    fs
    push    gs

    mov     ebx,loadBlockPtr           ; get ptr to regs struct
    mov     ax,ss                      ; save current stack
    mov     fs,ax
    mov     edx,esp
    lea     esi,num_dwords             ; save ptr to parameters
    lss     esp,pword ptr [ebx].LX_ESP ; set up child's stack
    push    fs                         ; save our old stack on child's stack
    push    edx
    mov     ecx,fs:[esi]               ; get parameter count
    inc     ecx                        ; increment because we pass count also
    mov     eax,ecx                    ; make room on stack for params
    shl     eax,2
    sub     esp,eax
    mov     ax,ss                      ; copy the params to caller's stack
    mov     es,ax
    mov     edi,esp
    rep     movs dword ptr es:[edi],fs:[esi]
    push    cs                         ; FAR return point below
    lea     eax,@ret
    push    eax                        ; child return point
    pushfd                             ; set up entry point in child
    movzx   eax,[ebx].LX_CS
    push    eax
    push    [ebx].LX_EIP
    mov     es,[ebx].LX_ES             ; init rest of child's regs
    mov     fs,[ebx].LX_FS
    mov     gs,[ebx].LX_GS
    mov     ds,[ebx].LX_DS
    xor     eax,eax
    xor     ebx,ebx
    xor     ecx,ecx
    xor     edx,edx
    xor     esi,esi
    xor     edi,edi
    xor     ebp,ebp
    iretd                              ; transfer control to child

@ret:
    mov     ebx,[esp]                  ; pop args off stack (don't wipe out
    inc     ebx                        ; return value in EAX
    shl     ebx,2
    add     esp,ebx
    lss     esp,pword ptr [esp]        ; restore our original stack

    pop     gs                         ; restore regs & return to caller
    pop     fs
    pop     es
    pop     ds
    pop     edi
    pop     esi
    pop     edx
    pop     ecx
    pop     ebx
    pop     ebp
    ret

@call_child endp
end
