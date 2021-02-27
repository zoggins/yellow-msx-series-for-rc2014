
	PUBLIC	_rp5c01Detect, __rp5c01SetByte, _rp5c01GetByte, _rp5c01GetTime, _rp5c01TestMode, _rp5c01SetHourMode, _rp5c01SetMode

	SECTION code_user

RP5RTC_REG	EQU	$B4
RP5RTC_DAT	EQU	$B5

REG_1SEC	EQU	$00
REG_10SEC	EQU	$01
REG_1MIN	EQU	$02
REG_10MIN	EQU	$03
REG_1HR		EQU	$04
REG_10HR	EQU	$05
REG_DAYWEEK	EQU	$06		; NOT USED BY THIS DRIVER
REG_1DAY	EQU	$07
REG_10DAY	EQU	$08
REG_1MNTH	EQU	$09
REG_10MNTH	EQU	$0A
REG_1YEAR	EQU	$0B
REG_10YEAR	EQU	$0C
REG_MODE	EQU	$0D
REG_TEST	EQU	$0E
REG_RESET	EQU	$0F


REG_12_24	EQU	$0A
REG_LEAPYR	EQU	$0B

MODE_TIMEST	EQU	0
MODE_ALRMST	EQU	1
MODE_RAM0	EQU	2
MODE_RAM1	EQU	3

MD_TIME		EQU	8
MD_ALRM		EQU	4

ERR_RANGE		EQU	-6	; PARAMETER OUT OF RANGE

;
; DETECT RTC HARDWARE PRESENCE
;
; extern uint8_t rp5c01Detect();
; return 255 if found, otherwise 0

_rp5c01Detect:
	LD	C, 0			; NVRAM INDEX 0
	CALL	RP5RTC_GETBYT		; GET VALUE
	LD	A, E			; TO ACCUM
	LD	L, A			; SAVE IT
	XOR	$FF			; FLIP ALL BITS
	LD	E, A			; TO E
	LD	L, 0			; NVRAM INDEX 0
	CALL	RP5RTC_SETBYT		; WRITE IT
	LD	C, 0			; NVRAM INDEX 0
	CALL	RP5RTC_GETBYT		; GET VALUE
	LD	A, L			; GET SAVED VALUE
	XOR	$FF			; FLIP ALL BITS
	CP	E			; COMPARE WITH VALUE READ
	LD	A, 0			; ASSUME OK
	JR	Z, RP5RTC_DETECT1	; IF MATCH, GO AHEAD
	LD	A, $FF			; ELSE STATUS IS ERROR

RP5RTC_DETECT1:
	PUSH	AF			; SAVE STATUS
	LD	A, L			; GET SAVED VALUE
	LD	C, 0 			; NVRAM INDEX 0
	CALL	RP5RTC_SETBYT		; SAVE IT
	POP	AF			; RECOVER STATUS
	LD	L, 255
	OR	A			; SET FLAGS
	RET	Z
	LD	L, 0
	RET

; extern uint16_t rp5c01GetByte(uint8_t index) __z88dk_fastcall
; result:	low byte is output value
; 		high byte is 0 if ok, 255 if index out of range
_rp5c01GetByte:
	LD	C, L
	CALL	RP5RTC_GETBYT
	LD	H, A
	LD	L, E
	RET

;
; RTC GET NVRAM BYTE
;   C: INDEX
;   E: VALUE (OUTPUT)
;   A:0 IF OK, ERR_RANGE IF OUT OF RANGE
;
RP5RTC_GETBYT:
	LD	A, L
	CP	$0D
	JR	NC, RP5RTC_BADIDX

	LD	B, MODE_RAM0
	CALL	RP5RTC_SETMD
	LD	A, C			; SELECT NVRAM INDEX
	OUT	(RP5RTC_REG), A
	IN	A, (RP5RTC_DAT)
	AND	$0F			; RETRIEVE UNIT NIBBLE
	LD	E, A

	LD	B, MODE_RAM1
	CALL	RP5RTC_SETMD
	LD	A, C			; SELECT NVRAM INDEX
	OUT	(RP5RTC_REG), A
	IN	A, (RP5RTC_DAT)
	AND	$0F			; RETRIEVE UNIT NIBBLE
	RLCA
	RLCA
	RLCA
	RLCA
	OR	E
	LD	E, A

	XOR	A			; SIGNAL SUCCESS
	RET				; AND RETURN

RP5RTC_BADIDX:
	LD	E, 00
	LD	A, ERR_RANGE
	RET

; extern uint8_t _rp5c01SetByte(uint16_t r) __z88dk_fastcall
; r low byte is byte to write
; r high byte is index
; result is 0 if ok, 255 is index is out of range
__rp5c01SetByte:
	LD	C, H
	LD	E, L
	CALL	RP5RTC_SETBYT
	LD	L, A
	RET
;
; RTC SET NVRAM BYTE
;   C: INDEX
;   E: VALUE
;   A:0 IF OK, ERR_RANGE IF OUT OF RANGE
;
RP5RTC_SETBYT:
	LD	A, C
	CP	$0D
	JR	NC, RP5RTC_BADIDX

	LD	B, MODE_RAM0
	CALL	RP5RTC_SETMD
	LD	A, C			; SELECT NVRAM INDEX
	OUT	(RP5RTC_REG), A
	LD	A, E
	AND	$0F
	OUT	(RP5RTC_DAT), A

	LD	B, MODE_RAM1
	CALL	RP5RTC_SETMD
	LD	A, C			; SELECT NVRAM INDEX
	OUT	(RP5RTC_REG), A
	LD	A, E
	AND	$F0
	RRCA
	RRCA
	RRCA
	RRCA
	OUT	(RP5RTC_DAT), A

	XOR	A			; SIGNAL SUCCESS
	RET				; AND RETURN


; SET MODE
; MODE IN B (MODE_TIMEST, MODE_ALRMST, MODE_RAM0, MODE_RAM1)
RP5RTC_SETMD:
	LD	A, REG_MODE			; SELECT MODE REGISTER
	OUT	(RP5RTC_REG), A

	IN	A, (RP5RTC_DAT)
	AND	MD_TIME | MD_ALRM
	OR	B
	OUT	(RP5RTC_DAT), A			; ASSIGN MODE
	RET

; extern void rp5c01SetMode(uint8_t mode) __z88dk_fastcall;
_rp5c01SetMode:
	LD	A, REG_MODE			; SELECT MODE REGISTER
	OUT	(RP5RTC_REG), A

	LD	A, L
	OUT	(RP5RTC_DAT), A			; ASSIGN MODE
	RET

;
; RTC GET TIME
;   HL: DATE/TIME BUFFER (OUT)
; BUFFER FORMAT IS BCD: YYMMDDHHMMSS
; 24 HOUR TIME FORMAT IS ASSUMED
;
; extern void _rp5c01GetTime(rtcDateTime*) __z88dk_fastcall
_rp5c01GetTime:
	LD	B, MODE_TIMEST
	CALL	RP5RTC_SETMD

	LD	B, REG_1SEC
	CALL	RP5RTC_RDVL
	LD	(HL), A			; RP5RTC_SS
	INC	HL

	LD	B, REG_1MIN
	CALL	RP5RTC_RDVL
	LD	(HL), A			; RP5RTC_MM
	INC	HL

	LD	B, REG_1HR
	CALL	RP5RTC_RDVL
	LD	(HL), A			; RP5RTC_HH
	INC	HL

	LD	B, REG_1DAY
	CALL	RP5RTC_RDVL
	LD	(HL), A			; RP5RTC_DT
	INC	HL

	LD	B, REG_1MNTH
	CALL	RP5RTC_RDVL
	LD	(HL), A			; RP5RTC_MO
	INC	HL

	LD	B, REG_1YEAR
	CALL	RP5RTC_RDVL
	LD	(HL), A			; RP5RTC_YR

	RET


; READ OUT 2 REGISTERS - 2 NIBBLES TO 1 BYTE
; REGISTER IN B
RP5RTC_RDVL:
	LD	A, B				; SELECT UNIT REGISTER
	OUT	(RP5RTC_REG), A
	IN	A, (RP5RTC_DAT)
	AND	$0F				; RETRIEVE UNIT NIBBLE
	LD	D, A

	INC	B
	LD	A, B				; SELECT TENS REGISTER
	OUT	(RP5RTC_REG), A
	IN	A, (RP5RTC_DAT)
	AND	$0F
	RLCA
	RLCA
	RLCA
	RLCA					; MOVE TO TOP NIBBLE
	OR	D				; MERGE IN LOW NIBBLE
						; A = VALUE AS BCD
	RET

; extern void rp5c01TestMode(uint8_t testBits) __z88dk_fastcall;

_rp5c01TestMode:
	LD	A, REG_TEST		; SELECT TEST REGISTER
	OUT	(RP5RTC_REG), A
	LD	A, L
	OUT	(RP5RTC_DAT), A		; TURN OFF ALL TEST MODE BITS
	RET

; 0 ->12 hour system, 1 -> 24 hour system
; extern void rp5c01SetHourMode(uint8_t mode) __z88dk_fastcall;

_rp5c01SetHourMode:
	LD	B, MODE_ALRMST
	CALL	RP5RTC_SETMD

	LD	A, REG_12_24		; SET TO 24 HOUR CLOCK
	OUT	(RP5RTC_REG), A
	LD	A, L
	OUT	(RP5RTC_DAT), A
	RET
