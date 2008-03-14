;
; worlda.asm
; x86 assembly-language server testing stuff
;
; this file uses NASM syntax.
; $Id: worlda.asm,v 1.4 2008-03-14 09:08:13 sezero Exp $
;

%idefine offset

SEGMENT .data

Ltemp dd 0


SEGMENT .text
 ALIGN 4

 global SV_HullPointContents
SV_HullPointContents:
 push edi
 mov eax, dword [8+4+esp]
 test eax,eax
 js Lhquickout
 push ebx
 mov ebx, dword [4+8+esp]
 push ebp
 mov edx, dword [12+12+esp]
 mov edi, dword [0+ebx]
 mov ebp, dword [4+ebx]
 sub ebx,ebx
 push esi
Lhloop:
 mov ecx, dword [0+edi+eax*8]
 mov eax, dword [4+edi+eax*8]
 mov esi,eax
 ror eax,16
 lea ecx, [ecx+ecx*4]
 mov bl, byte [16+ebp+ecx*4]
 cmp bl,3
 jb Lnodot
 fld  dword [0+ebp+ecx*4]
 fmul  dword [0+edx]
 fld  dword [0+4+ebp+ecx*4]
 fmul  dword [4+edx]
 fld  dword [0+8+ebp+ecx*4]
 fmul  dword [8+edx]
 fxch st1
 faddp st2,st0
 faddp st1,st0
 fsub  dword [12+ebp+ecx*4]
 jmp Lsub
Lnodot:
 fld  dword [12+ebp+ecx*4]
 fsubr  dword [edx+ebx*4]
Lsub:
 sar eax,16
 sar esi,16
 fstp  dword [Ltemp]
 mov ecx, dword [Ltemp]
 sar ecx,31
 and esi,ecx
 xor ecx,0FFFFFFFFh
 and eax,ecx
 or eax,esi
 jns Lhloop
Lhdone:
 pop esi
 pop ebp
 pop ebx
Lhquickout:
 pop edi
 ret

