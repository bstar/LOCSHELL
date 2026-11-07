bits 16
org 100h
mov sp,stacktop
mov bx,(stacktop-$$+100h+15)/16
mov ah,4ah
int 21h
jc bad
mov ax,cs
mov [block+4],ax
mov [block+8],ax
mov [block+12],ax
mov dx,program
mov bx,block
mov ax,4b00h
int 21h
jc bad
mov ah,4dh
int 21h
mov ah,4ch
int 21h
bad: mov ax,4c02h
int 21h
program db 'CHILD.EXE',0
block dw 0,tail,0,5ch,0,6ch,0
tail db 0,13
 times 128 db 0
stacktop:
