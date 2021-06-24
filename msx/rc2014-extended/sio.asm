
; SIO CHIP PROBE
; CHECK FOR PRESENCE OF SIO CHIPS AND POPULATE THE
; SIO_MAP BITMAP (ONE BIT PER CHIP).  THIS DETECTS
; CHIPS, NOT CHANNELS.  EACH CHIP HAS 2 CHANNELS.
; MAX OF TWO CHIPS CURRENTLY.  INT VEC VALUE IS TRASHED!
;
SIO_PROBE:
	; INIT THE INT VEC REGISTER OF ALL POSSIBLE CHIPS
	; TO ZERO.
	XOR	A
	LD	B, 2			; WR2 REGISTER (INT VEC)
	LD	C, SIO0B_CMD		; FIRST CHIP
	CALL	SIO_WR			; WRITE ZERO TO CHIP REG

	; FIRST POSSIBLE CHIP
	LD	C, SIO0B_CMD		; FIRST CHIP CMD/STAT PORT
	CALL	SIO_PROBECHIP		; PROBE IT
	RET
;
SIO_PROBECHIP:
	; READ WR2 TO ENSURE IT IS ZERO (AVOID PHANTOM PORTS)
	CALL	SIO_RD			; GET VALUE
	AND	$F0			; ONLY TOP NIBBLE
	RET	NZ			; ABORT IF NOT ZERO
	; WRITE INT VEC VALUE TO WR2
	LD	A,$FF			; TEST VALUE
	CALL	SIO_WR			; WRITE IT
	; READ WR2 TO CONFIRM VALUE WRITTEN
	CALL	SIO_RD			; REREAD VALUE
	AND	$F0			; ONLY TOP NIBBLE
	CP	$F0			; COMPARE
	RET

SIO_WR:
	OUT	(C),B			; SELECT CHIP REGISTER
	OUT	(C),A			; WRITE VALUE
	RET
;
SIO_RD:
	OUT	(C),B			; SELECT CHIP REGISTER
	IN	A,(C)			; GET VALUE
	RET

	PUBLIC	SIO_RCBBYT
SIO_RCBBYT:
	EI
	LD	A, (RS_IQLN)
	INC	A
	INC	A
	LD	B, A

	LD	HL, (RS_FCB)
	DI				; AVOID COLLISION WITH INT HANDLER
	LD	A, (RS_DATCNT)		; GET COUNT
	DEC	A			; DECREMENT COUNT
	LD	(RS_DATCNT), A		; SAVE UPDATED COUNT
	CP	0			; BUFFER LOW THRESHOLD
	JR	NZ, SIO_IN1		; IF NOT, BYPASS SETTING RTS

	LD	A, 5			; RTS IS IN WR5
	OUT	(CMD_CH), A		; ADDRESS WR5
	LD	A, SIO_RTSON		; VALUE TO SET RTS
	OUT	(CMD_CH), A		; DO IT

	LD	A, 253
	LD	(SIO_RTS), A
SIO_IN1:
	INC	HL
	INC	HL			; HL NOW HAS ADR OF TAIL PTR
	PUSH	HL			; SAVE ADR OF TAIL PTR
	LD	A, (HL)			; DEREFERENCE HL
	INC	HL
	LD	H, (HL)
	LD	L, A			; HL IS NOW ACTUAL TAIL PTR
	LD	C, (HL)			; C := CHAR TO BE RETURNED
	INC	HL			; BUMP TAIL PTR
	POP	DE			; RECOVER ADR OF TAIL PTR
	LD	A, (RS_BUFEND)		; GET BUFEND PTR LOW BYTE
	CP	L			; ARE WE AT BUFF END?
	JR	NZ, SIO_IN2		; IF NOT, BYPASS
	LD	H, D			; SET HL TO
	LD	L, E			; ... TAIL PTR ADR
	INC	HL			; BUMP PAST TAIL PTR
	INC	HL			; ... SO HL NOW HAS ADR OF ACTUAL BUFFER START
SIO_IN2:
	EX	DE, HL			; DE := TAIL PTR VAL, HL := ADR OF TAIL PTR
	LD	(HL), E			; SAVE UPDATED TAIL PTR
	INC	HL
	LD	(HL), D
	EI				; INTERRUPTS OK AGAIN
	LD	A, C			; MOVE CHAR TO RETURN TO A
	OR	A
	RET
