; Foreground-session allocation observer. No DOS/file calls from hooks.
; 8086, NASM flat COM. Removed after the launched batch returns.
bits 16
org 100h
jmp install
old21 dd 0
old67 dd 0
old2f dd 0
old27 dd 0
xms dd 0
first_mcb dw 0
busy db 0
armed db 0
; Stable wire format, words only. Version, size, resident paragraphs,
; valid bits (conventional/EMS/XMS), baseline free/min free (paragraphs/KB),
; DOS/EMS/XMS allocation failures and sample count (saturating).
stats:
dw 1, 28, (resident_end-$$+100h+15)/16
valid dw 0
base_conv dw 0
min_conv dw 0
base_ems dw 0
min_ems dw 0
base_xms dw 0
min_xms dw 0
fail_dos dw 0
fail_ems dw 0
fail_xms dw 0
samples dw 0

save_regs:
; unused: macros below keep all intercepted inputs/outputs intact.
%macro save 0
 pushf
 push ax
 push bx
 push cx
 push dx
 push si
 push di
 push bp
 push ds
 push es
 push cs
 pop ds
%endmacro
%macro restore 0
 pop es
 pop ds
 pop bp
 pop di
 pop si
 pop dx
 pop cx
 pop bx
 pop ax
 popf
%endmacro

; Bounded direct MCB walk. Only called at DOS API boundaries, never timer IRQ.
; Aggregate conventional free paragraphs. Corrupt/incomplete chains invalidate.
conv_sample:
 push ax
 push bx
 push cx
 push dx
 push si
 push es
 mov ax,[first_mcb]
 mov cx,4096
 xor dx,dx
.next:
 cmp ax,40h
 jb .bad
 cmp ax,0a000h
 jae .bad
 mov es,ax
 mov bl,[es:0]
 cmp bl,'M'
 je .typeok
 cmp bl,'Z'
 jne .bad
.typeok:
 mov si,[es:3]
 cmp word [es:1],0
 jne .used
 add dx,si
 jc .bad
.used:
 add ax,si
 jc .bad
 inc ax
 cmp ax,0a000h
 ja .bad
 cmp bl,'Z'
 je .done
 cmp ax,0a000h
 je .done
 loop .next
.bad:
 and word [valid],0fffeh
 jmp .exit
.done:
 cmp byte [armed],0
 jne .running
 mov [base_conv],dx
 mov [min_conv],dx
 or word [valid],1
 jmp .exit
.running:
 cmp dx,[min_conv]
 jae .exit
 mov [min_conv],dx
.exit:
 pop es
 pop si
 pop dx
 pop cx
 pop bx
 pop ax
 ret

; Driver calls are made only at foreground API boundaries, not in timer IRQs.
extra_sample:
 cmp word [old67+2],0
 je .xms
 mov ah,42h
 pushf
 call far [old67]
 or ah,ah
 jnz .emsbad
 cmp bx,4095
 ja .emsbad
 mov cl,4
 shl bx,cl
 cmp byte [armed],0
 jne .emsrunning
 mov [base_ems],bx
 mov [min_ems],bx
 or word [valid],2
 jmp .xms
.emsrunning:
 cmp bx,[min_ems]
 jae .xms
 mov [min_ems],bx
 jmp .xms
.emsbad:
 and word [valid],0fffdh
.xms:
 cmp word [xms+2],0
 je .exit
 mov ah,8
 call far [xms]
 or bl,bl
 jz .xok
 cmp bl,0a0h
 jne .xbad
.xok:
 cmp byte [armed],0
 jne .xrunning
 mov [base_xms],dx
 mov [min_xms],dx
 or word [valid],4
 ret
.xrunning:
 cmp dx,[min_xms]
 jae .exit
 mov [min_xms],dx
.exit:
 ret
.xbad:
 and word [valid],0fffbh
 ret

observe:
 cmp byte [armed],2
 je .exit
 cmp byte [busy],0
 jne .exit
 mov byte [busy],1
 call conv_sample
 call extra_sample
 cmp word [samples],65535
 je .full
 inc word [samples]
.full:
 mov byte [busy],0
.exit:
 ret

%macro interrupt_return 0
 push bp
 mov bp,sp
 push ax
 pushf
 pop ax
 and ax,1
 and word [ss:bp+6],0fffeh
 or [ss:bp+6],ax
 pop ax
 pop bp
 iret
%endmacro

hook21:
 cmp ah,4bh
 jne .ordinary
 cmp al,0
 jne .ordinary
 ; The collector is always launched by its absolute, upper-case path.
 push ax
 push si
 push di
 push es
 push ds
 pop es
 mov si,dx
 mov di,collector_name
.checkname:
 mov al,[es:si]
 cmp al,[cs:di]
 jne .target
 inc si
 inc di
 or al,al
 jnz .checkname
 mov byte [cs:armed],2
 jmp .execdone
.target:
 cmp byte [cs:armed],0
 jne .execdone
 save
 call observe
 mov byte [armed],1
 restore
.execdone:
 pop es
 pop di
 pop si
 pop ax
 jmp far [cs:old21]
.ordinary:
 cmp ah,31h
 jne .nottsr
 cmp byte [cs:armed],1
 jne .nottsr
 or word [cs:valid],8000h
.nottsr:
 ; Sample before memory release and exit so short-lived allocations are seen.
 ; Avoid wrapping EXEC: its frame survives the entire child and DOS may bypass it.
 cmp ah,48h
 je .allocation
 cmp ah,4ah
 je .allocation
 cmp ah,49h
 je .sample
 cmp ah,4ch
 je .sample
 jmp far [cs:old21]
.sample:
 save
 call observe
 restore
 jmp far [cs:old21]
.allocation:
 pushf
 call far [cs:old21]
 jc .failed
 save
 call observe
 restore
 interrupt_return
.failed:
 save
 cmp word [fail_dos],65535
 je .counted
 inc word [fail_dos]
.counted:
 call observe
 restore
 interrupt_return

hook27:
 cmp byte [cs:armed],1
 jne .chain
 or word [cs:valid],8000h
.chain:
 jmp far [cs:old27]

hook67:
 cmp ax,0de00h
 jne .notvcpi
 or word [cs:valid],4000h
.notvcpi:
 ; Only observe memory management, not hot page mapping/memory moves.
 cmp ah,43h
 je .allocation
 cmp ah,45h
 je .sample
 cmp ah,51h
 je .allocation
 jmp far [cs:old67]
.sample:
 save
 call observe
 restore
 jmp far [cs:old67]
.allocation:
 pushf
 call far [cs:old67]
 save
 or ah,ah
 jz .ok
 cmp word [fail_ems],65535
 je .ok
 inc word [fail_ems]
.ok:
 call observe
 restore
 interrupt_return

hookxms:
 cmp ah,9
 je .allocation
 cmp ah,0fh
 je .allocation
 cmp ah,0ah
 je .sample
 jmp far [cs:xms]
.sample:
 save
 call observe
 restore
 jmp far [cs:xms]
.allocation:
 call far [cs:xms]
 save
 or ax,ax
 jnz .ok
 cmp word [fail_xms],65535
 je .ok
 inc word [fail_xms]
.ok:
 call observe
 restore
 retf

hook2f:
 cmp ax,1687h
 jne .notdpmi
 or word [cs:valid],4000h
.notdpmi:
 cmp ax,4310h
 je .getxms
 cmp ax,0d74dh
 jne .chain
 cmp bx,4c4dh
 jne .chain
 ; Private query; CX=1 requests uninstall. Refuse if another hook owns a vector.
 cmp cx,1
 je .remove
.reply:
 mov ax,4d4ch
 push cs
 pop es
 mov di,stats
 iret
.remove:
 save
 xor ax,ax
 mov es,ax
 cli
 test word [valid],8000h
 jnz .refuse
 cmp word [es:27h*4],hook27
 jne .refuse
 mov ax,cs
 cmp [es:27h*4+2],ax
 jne .refuse
 cmp word [es:21h*4],hook21
 jne .refuse
 mov ax,cs
 cmp [es:21h*4+2],ax
 jne .refuse
 cmp word [es:2fh*4],hook2f
 jne .refuse
 cmp [es:2fh*4+2],ax
 jne .refuse
 cmp word [old67+2],0
 je .unhook
 cmp word [es:67h*4],hook67
 jne .refuse
 cmp [es:67h*4+2],ax
 jne .refuse
.unhook:
 ; Do not sample collector's allocation; the target already exited.
 mov ax,[old27]
 mov [es:27h*4],ax
 mov ax,[old27+2]
 mov [es:27h*4+2],ax
 mov ax,[old21]
 mov [es:21h*4],ax
 mov ax,[old21+2]
 mov [es:21h*4+2],ax
 mov ax,[old2f]
 mov [es:2fh*4],ax
 mov ax,[old2f+2]
 mov [es:2fh*4+2],ax
 cmp word [old67+2],0
 je .removed
 mov ax,[old67]
 mov [es:67h*4],ax
 mov ax,[old67+2]
 mov [es:67h*4+2],ax
.removed:
 restore
 mov cx,0
 jmp .reply
.refuse:
 restore
 mov cx,1
 jmp .reply
.getxms:
 cmp word [cs:xms+2],0
 je .chain
 mov bx,hookxms
 push cs
 pop es
 iret
.chain:
 jmp far [cs:old2f]
collector_name db 'C:\DOSSHELL\MEMLEARN.EXE',0
resident_end:

install:
 mov ax,0d74dh
 mov bx,4c4dh
 xor cx,cx
 int 2fh
 cmp ax,4d4ch
 je already
 ; Release inherited environment and shrink COM before baseline observation.
 mov es,[2ch]
 mov ah,49h
 int 21h
 push cs
 pop es
 mov bx,(program_end-$$+100h+15)/16+64
 mov ah,4ah
 int 21h
 mov ax,3521h
 int 21h
 mov [old21],bx
 mov [old21+2],es
 mov ax,3527h
 int 21h
 mov [old27],bx
 mov [old27+2],es
 mov ax,352fh
 int 21h
 mov [old2f],bx
 mov [old2f+2],es
 mov ah,52h
 int 21h
 mov ax,[es:bx-2]
 mov [first_mcb],ax
 mov ax,3567h
 int 21h
 ; EMS signature at offset 10 in the handler's segment.
 mov si,10
 mov di,ems_signature
 mov cx,8
.sig:
 mov al,[es:si]
 cmp al,[di]
 jne .noems
 inc si
 inc di
 loop .sig
 mov [old67],bx
 mov [old67+2],es
 jmp .emsdone
.noems:
 or word [valid],2
.emsdone:
 mov ax,4300h
 int 2fh
 cmp al,80h
 jne .noxms
 mov ax,4310h
 int 2fh
 mov [xms],bx
 mov [xms+2],es
 jmp .xmsdone
.noxms:
 or word [valid],4
.xmsdone:
 ; Baseline is refreshed at first EXEC after resident sizing completes.
 call conv_sample
 call extra_sample
 mov dx,hook21
 mov ax,2521h
 int 21h
 mov dx,hook27
 mov ax,2527h
 int 21h
 mov dx,hook2f
 mov ax,252fh
 int 21h
 cmp word [old67+2],0
 je .stay
 mov dx,hook67
 mov ax,2567h
 int 21h
.stay:
 mov dx,(resident_end-$$+100h+15)/16
 mov ax,3100h
 int 21h
already:
 mov ax,4c01h
 int 21h
ems_signature db 'EMMXXXX0'
program_end:
