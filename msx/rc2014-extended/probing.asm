

PROBE_HARDWARE:
	LD	DE, MSG.SIO
	CALL	PRINT

	CALL	SIO_PROBE
	JR	Z, SIO_FOUND

	LD	DE, MSG.NOT
	CALL	PRINT

SIO_FOUND:
	LD	DE, MSG.SIO_PRESENT
	CALL	PRINT

	LD	DE, MSG.RTC
	CALL	PRINT

	CALL	RP5RTC_PROBE
	JR	Z, RTC_FOUND

	LD	DE, MSG.NOT
	CALL	PRINT

RTC_FOUND:
	LD	DE, MSG.PRESENT
	CALL	PRINT

	LD	DE, MSG.MUSIC
	CALL	PRINT

	CALL	MSX_MUSIC_PROBE
	JR	Z, MSX_MUSIC_NOT_FOUND

	LD	DE, MSG.NOT
	CALL	PRINT

MSX_MUSIC_NOT_FOUND:
	LD	DE, MSG.PRESENT
	CALL	PRINT

	LD	DE, MSG.GAME
	CALL	PRINT

	CALL	GAME_PROBE
	JR	Z, GAME_NOT_FOUND

	LD	DE, MSG.NOT
	CALL	PRINT

GAME_NOT_FOUND:
	LD	DE, MSG.PRESENT
	CALL	PRINT

	LD	DE, MSG.NEWLINE
	JP	PRINT

MSG.SIO
	DB	"SIO/2 Module:    ", 0

MSG.RTC
	DB	"RTC/F4 Module:   ", 0

MSG.MUSIC
	DB	"MSX MUSIC:       ", 0

MSG.GAME
	DB	"MSX GAME:        ", 0

MSG.PRESENT
	DB	"PRESENT"
	
MSG.NEWLINE
	DB	13, 10, 0

if BUS_CLK EQ 307200
MSG.SIO_PRESENT:
	DB	"PRESENT (19200)", 13, 10, 0	; ; MARK MAKE BAUD RATE CONFIG
endif

if BUS_CLK EQ 614400
MSG.SIO_PRESENT:
	DB	"PRESENT (38400)", 13, 10, 0	; ; MARK MAKE BAUD RATE CONFIG
endif

MSG.NOT
	DB	"NOT ", 0


; DETECT RTC HARDWARE PRESENCE
; RTC HARDWARE IS NORMALL MANAGED BY THE BIOS
; SO WE ONLY NEED TO TEST IF PRESENT.
;
RP5RTC_PROBE:
	LD	C, 00100000b
	LD	IX, REDCLK
	CALL	CALSUB
	AND	0Fh
	CP	0Ah
	RET

; 
MSX_MUSIC_PROBE:
	CALL	MSX_MUSIC_SET_SLOT
	PUSH	HL
	LD	HL, 0x4000
	LD	DE, MSX_MARKER_1
	LD	B, 2
	CALL	TEST_MARKER
	JR	NZ, MSX_MUSIC_PROBE_1

	LD	HL, 0x401C
	LD	DE, MSX_MARKER_2
	LD	B, 4
	CALL	TEST_MARKER

MSX_MUSIC_PROBE_1:
	EX	AF, AF'
	POP	HL
	CALL	MSX_MUSIC_RESTORE_SLOT
	EX	AF, AF'
	RET

TEST_MARKER:
	LD	A, (DE)
	CP	(HL)
	RET	NZ
	INC	DE
	INC	HL
	DJNZ	TEST_MARKER
	RET


MSX_MARKER_1:
	DEFB	"AB"
MSX_MARKER_2:
	DEFB	"OPLL"

MSX_MUSIC_SET_SLOT:
	DI
	IN	A, (PSL_STAT)
	LD	H, A			; STORE CURRENT SLOTS ASSIGNMENTS IN D
	OR	11001100B		; MASK SLOT 3 FOR PAGE 1 AND 3
	OUT	(PSL_STAT), A		; APPLY SLOT ASSIGNMENTS
	LD	D, A

	LD	A, (SSL_REGS)
	CPL
	LD	L, A			; STORE CURRENT SUB-SLOTS ASSIGNMENTS IN L
	AND	11110011B		; MASK OUT PAGE 1 SUB-SLOT
	OR	00000100B		; SET PAGE 1 TO SUB-SLOT 1
	LD	E, A
	LD	(SSL_REGS), A		; APPLY SUB-SLOT ASSIGNMENTS

	LD	A, H			; RETRIEVE ORIGINAL SLOT ASSIGNMENTS
	OR	00001100B		; KEEP PAGE 2 TO SLOT 3
	OUT	(PSL_STAT), A		; APPLY SLOT ASSIGNMENTS

					; H -> ORIGINAL SLOT, L -> ORIGINAL SUB-SLOT
	EI
	RET

MSX_MUSIC_RESTORE_SLOT:
	DI
	IN	A, (PSL_STAT)
	OR	11000000B		; MASK PAGE 3 TO SLOT 3
	OUT	(PSL_STAT), A		; APPLY SLOT ASSIGNMENTS

	LD	A, L			; LOAD ORIGINAL SUB-SLOT ASSIGNMENTS
	LD	(SSL_REGS), A		; RESTORE ORIGINAL SUB-SLOT ASSIGNMENTS

	LD	A, H			; LOAD ORIGINAL SLOT ASSIGNMENTS
	OUT	(PSL_STAT), A		; RESTORE ORIGINAL SLOT MAPPPINGS

	EI
	RET

AY_RSEL		EQU	$A0
AY_RDAT		EQU	$A1
AY_RIN		EQU	$A2

GAME_PROBE:
	DI
	LD	A, 2			; SELECT REG 2 FOR WRITING
	OUT	(AY_RSEL), A
	LD	A, $55			; WRITE $55
	OUT	(AY_RDAT), A

	LD	A, 2			; RE-SELECT REG 2 FOR READING
	OUT	(AY_RSEL), A		; WANT TO READ
	IN	A, (AY_RIN)		; READ SELECTED REGISTER
	CP	$55
	EI
	RET
