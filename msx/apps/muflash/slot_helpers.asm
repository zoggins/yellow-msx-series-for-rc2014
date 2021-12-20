
DEFC SSL_REGS		= 0xFFFF
DEFC PSL_STAT		= 0xA8
DEFC MSX_MUSIC_PAGE 	= 0xE000
DEFC ROM_05555H		= (0x5555-0x4000)
DEFC ROM_02AAAH		= (0x2AAA-0x0000)

	SECTION CODE_COMPILER

; SWITCH MSX-MUSIC ROM PAGE TO PAGE L
; (SWITCH PAGE 3 TO SLOT 3-1 TO ENABLE MSX-MUSIC PAGING ADDRESS)
;
; MSX-MUSIC'S ROM PAGE CAN BE SELECTED BY WRITING TO THE STANDARD MEMORY MAPPED WRITE REGISTER IN PAGE 1 OR PAGE 2
; THIS FUNCTION USES PAGE 3, TO AVOID INTERFERING WITH THE SST39SF040 WRITE COMMAND SEQUENCE
;
; L -> DESIRED MSX-MUSIC ROM PAGE
;
MSX_MUSIC_SET_PAGE:
	; THIS FUNCTION CAN NOT USE THE STACK WHILE PAGE 3 IS SWITCHED OUT

	IN	A, (PSL_STAT)
	LD	D, A			; STORE CURRENT SLOTS ASSIGNMENTS IN D
	OR	A, 0b11000000		; MASK PAGE 3 TO SLOT 3
	OUT	(PSL_STAT), A		; APPLY SLOT ASSIGNMENTS

	LD	A, (SSL_REGS)
	CPL
	LD	E, A			; STORE CURRENT SUB-SLOTS ASSIGNMENTS IN E
	AND	A, 0b00111111		; MASK OUT PAGE 3 SUB-SLOT
	OR	A, 0b01000000		; SET PAGE 3 TO SUB-SLOT 1
	LD	(SSL_REGS), A		; APPLY SUB-SLOT ASSIGNMENTS

	LD	A, L
	LD	(MSX_MUSIC_PAGE), A	; SET MSX-MUSIC PAGE TO DESIRED PAGE

	LD	A, E			; RESTORE SUB-SLOT ASSIGNMENTS
	LD	(SSL_REGS), A

	LD	A, D			; RESTORE SLOT ASSIGNMENTS
	OUT	(PSL_STAT), A
	RET

;
; SET PAGE 1 TO MSX-MUSIC SLOT (3-1)
;
MSX_MUSIC_SET_SLOT:
	IN	A, (PSL_STAT)
	LD	H, A			; STORE CURRENT SLOTS ASSIGNMENTS IN D
	OR	A, 0b11001100		; MASK SLOT 3 FOR PAGE 1 AND 3
	OUT	(PSL_STAT), A		; APPLY SLOT ASSIGNMENTS

	LD	A, (SSL_REGS)
	CPL
	LD	L, A			; STORE CURRENT SUB-SLOTS ASSIGNMENTS IN L
	AND	A, 0b11110011		; MASK OUT PAGE 1 SUB-SLOT
	OR	A, 0b00000100		; SET PAGE 1 TO SUB-SLOT 1
	LD	(SSL_REGS), A		; APPLY SUB-SLOT ASSIGNMENTS

	LD	A, H			; RETRIEVE ORIGINAL SLOT ASSIGNMENTS
	OR	A, 0b00001100		; KEEP PAGE 2 TO SLOT 3
	OUT	(PSL_STAT), A		; APPLY SLOT ASSIGNMENTS

					; H -> ORIGINAL SLOT, L -> ORIGINAL SUB-SLOT
	RET

;
; RESTORE SLOT/SUB-SLOT ASSIGNMENTS
; H -> SLOT ASSIGNEMTNS
; L -> SUB SLOT ASSIGNMENTS
;
MSX_MUSIC_RESTORE_SLOT:
	IN	A, (PSL_STAT)
	OR	A, 0b11000000		; MASK PAGE 3 TO SLOT 3
	OUT	(PSL_STAT), A		; APPLY SLOT ASSIGNMENTS

	LD	A, L			; LOAD ORIGINAL SUB-SLOT ASSIGNMENTS
	LD	(SSL_REGS), A		; RESTORE ORIGINAL SUB-SLOT ASSIGNMENTS

	LD	A, H			; LOAD ORIGINAL SLOT ASSIGNMENTS
	OUT	(PSL_STAT), A		; RESTORE ORIGINAL SLOT MAPPPINGS

	RET

	PUBLIC	_msxMusicEraseROM
;
; void msxMusicEraseROM()
;
; APPLY THE COMMAND SEQUENCE TO ERASE THE ROM
; 5555H=AAH, 2AAAH=55H, 5555H=80H, 5555H=AAH, 2AAAH=55H, 5555H=10H
;
_msxMusicEraseROM:
	DI

	CALL	MSX_MUSIC_SET_SLOT
	LD	(current_page_slots), HL

	LD	L, 1				; 5555H=AAH
	CALL	MSX_MUSIC_SET_PAGE
	LD	A, 0xAA
	LD	(ROM_05555H), A

	LD	L, 0				; 2AAAH=55H
	CALL	MSX_MUSIC_SET_PAGE
	LD	A, 0x55
	LD	(ROM_02AAAH), A

	LD	L, 1				; 5555H=80H
	CALL	MSX_MUSIC_SET_PAGE
	LD	A, 0x80
	LD	(ROM_05555H), A

	LD	A, 0xAA				; 5555H=AAH
	LD	(ROM_05555H), A

	LD	L, 0				; 2AAAH=55H
	CALL	MSX_MUSIC_SET_PAGE
	LD	A, 0x55
	LD	(ROM_02AAAH), A

	LD	L, 1				; 5555H=10H
	CALL	MSX_MUSIC_SET_PAGE
	LD	A, 0x10
	LD	(ROM_05555H), A

	LD	HL, (current_page_slots)
	CALL	MSX_MUSIC_RESTORE_SLOT
	EI
	RET

	SECTION bss_compiler

current_page_slots:	DW	0
