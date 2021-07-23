
	ORG	$8000

	PUBLIC	HIGHPAGEADDR, SIO_INT, SIO_RCVBUF, SIO_CNT, SIO_RTS, ORG_H_KEYI

	include	"sio.inc"

HIGHPAGEADDR:	EQU	$

SIO_RCVBUF:
SIO_CNT:	DB	0		; CHARACTERS IN RING BUFFER
SIO_HD:		DW	SIO_BUF		; BUFFER HEAD POINTER
SIO_TL:		DW	SIO_BUF		; BUFFER TAIL POINTER
SIO_BUF:	DS	SIO_BUFSZ, $00	; RECEIVE RING BUFFER

SIO_RTS:	DB	$FF		; 0 => RTS OFF, $FF => RTS ON

ORG_H_KEYI:	DS	5, 0


SIO_INT:
	DI				; INTERRUPTS WILL BE RE-ENABLED BY MSX BIOS

	; CHECK TO SEE IF SOMETHING IS ACTUALLY THERE
	XOR	A			; READ REGISTER 0
	OUT	(CMD_CH), A
	IN	A, (CMD_CH)
	AND	$01			; ISOLATE RECEIVE READY BIT
	RET	Z			; NOTHING AVAILABLE ON CURRENT CHANNEL

SIO_INTRCV1:
	; RECEIVE CHARACTER INTO BUFFER
	IN	A, (DAT_CH)			; READ PORT

	LD	B, A			; SAVE BYTE READ
	LD	HL, SIO_RCVBUF
	LD	A, (HL)			; GET COUNT
	CP	SIO_BUFSZ			; COMPARE TO BUFFER SIZE
	JR	Z, SIO_INTRCV4		; BAIL OUT IF BUFFER FULL, RCV BYTE DISCARDED
	INC	A			; INCREMENT THE COUNT
	LD	(HL), A			; AND SAVE IT

	CP	SIO_BUF_HI			; BUFFER GETTING FULL?
	JR	NZ, SIO_INTRCV2		; IF NOT, BYPASS CLEARING RTS

	LD	A, 5			; SELECTED REGISTER WRITE 5
	OUT	(CMD_CH), A
	LD	A, SIO_RTSOFF
	OUT	(CMD_CH), A

	XOR	A
	LD	(SIO_RTS), A

SIO_INTRCV2:
	INC	HL			; HL NOW HAS ADR OF HEAD PTR
	PUSH	HL			; SAVE ADR OF HEAD PTR
	LD	A, (HL)			; DEREFERENCE HL
	INC	HL
	LD	H, (HL)
	LD	L, A			; HL IS NOW ACTUAL HEAD PTR
	LD	(HL), B			; SAVE CHARACTER RECEIVED IN BUFFER AT HEAD

	INC	HL			; BUMP HEAD POINTER
	POP	DE			; RECOVER ADR OF HEAD PTR
	LD	A, L			; GET LOW BYTE OF HEAD PTR
	SUB	SIO_BUFSZ+4			; SUBTRACT SIZE OF BUFFER AND POINTER
	CP	E			; IF EQUAL TO START, HEAD PTR IS PAST BUF END
	JR	NZ, SIO_INTRCV3		; IF NOT, BYPASS
	LD	H, D			; SET HL TO
	LD	L, E			; ... HEAD PTR ADR
	INC	HL			; BUMP PAST HEAD PTR
	INC	HL
	INC	HL
	INC	HL			; ... SO HL NOW HAS ADR OF ACTUAL BUFFER START
SIO_INTRCV3:
	EX	DE, HL			; DE := HEAD PTR VAL, HL := ADR OF HEAD PTR
	LD	(HL), E			; SAVE UPDATED HEAD PTR
	INC	HL
	LD	(HL), D
	; CHECK FOR MORE PENDING...

	XOR	A
	OUT	(CMD_CH), A		; READ REGISTER 0
	IN	A, (CMD_CH)		;
	RRA				; READY BIT TO CF
	JR	C, SIO_INTRCV1		; IF SET, DO SOME MORE

	LD	A, (SIO_RTS)
	OR	A
	JR	Z, SIO_INTRCV4		; ABORT NOW IF RTS IS OFF

	; TEST FOR NEW BYTES FOR A SHORT PERIOD OF TIME
	LD	B, 40
SIO_MORE:
	IN	A, (CMD_CH)		;
	RRA				; READY BIT TO CF
	JR	C, SIO_INTRCV1		; IF SET, DO SOME MORE
	DJNZ	SIO_MORE

COMMAND_0	EQU	0
COMMAND_1	EQU	0x08
COMMAND_2	EQU	0x10
COMMAND_3	EQU	0x18
COMMAND_4	EQU	0x20
COMMAND_5	EQU	0x28
COMMAND_6	EQU	0x30
COMMAND_7	EQU	0x38

SIO_INTRCV4:
	; NOT SURE WHAT I NEED TO RESET CHANNEL A
	; SOMETHING NOT QUITE RIGHT
	LD	A, 0
	OUT	(SIO0A_CMD), A
	LD	A, COMMAND_3
	OUT	(SIO0A_CMD), A
	RET