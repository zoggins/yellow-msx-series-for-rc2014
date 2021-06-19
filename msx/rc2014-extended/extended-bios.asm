


EXTBIO_RS232_ID			EQU	8
EXTBIO_RS2_GET_INFO_TABLE_FN	EQU	0
EXTBIO_RC2014_ID		EQU	214

EXTBIO:
	PUSH	AF
	LD	A, D
	OR	E
	JR	Z, EXTBIO_GET_DEVICE_ID

	LD	A, D
	CP	EXTBIO_RS232_ID
	JR	Z, EXTBIO_RS232

	CP	EXTBIO_RC2014_ID
	JR	Z, EXTBIO_RC2014

	JR	EXBIO_EXIT		; NOT US

EXTBIO_RS232:
	LD	A, E
	CP	EXTBIO_RS2_GET_INFO_TABLE_FN
	JR	Z, EXTBIO_RS2_GET_INFO_TABLE

	JR	EXBIO_EXIT

EXTBIO_RC2014_INSTALL_FOSSIL_FN	EQU	1

EXTBIO_RC2014:
	LD	A, E
	CP	EXTBIO_RC2014_INSTALL_FOSSIL_FN
	JR	Z, EXTBIO_RC_INSTALL_FOSSIL

	JR	EXBIO_EXIT

EXTBIO_GET_DEVICE_ID:
	PUSH	DE
	PUSH	BC
	LD	E, EXTBIO_RS232_ID
	LD	A, B
	PUSH	AF
	CALL	WRSLT
	POP	AF
	PUSH	AF
	INC	HL
	LD	E, 00		; reserved byte
	CALL	WRSLT
	POP	AF
	PUSH	AF
	INC	HL

	LD	E, EXTBIO_RC2014_ID
	CALL	WRSLT
	POP	AF
	PUSH	AF
	INC	HL
	LD	E, 0		; reserved byte
	CALL	WRSLT
	POP	AF
	INC	HL
	POP	BC
	POP	DE

EXBIO_EXIT:
	POP	AF
	JP	MEXBIH


EXTBIO_RS2_GET_INFO_TABLE:
	PUSH	DE
	PUSH	BC
	CALL	GETSL10

	LD	E, A
	LD	A, B
	PUSH	AF
	CALL	WRSLT
	POP	BC
	PUSH	BC
	INC	HL

	LD	E, EXTBIO_RS2_JUMP_TABLE & 255
	POP	AF
	PUSH	AF
	CALL	WRSLT
	POP	BC
	PUSH	BC
	INC	HL

	LD	E, EXTBIO_RS2_JUMP_TABLE / 256
	POP	AF
	CALL	WRSLT
	POP	BC
	INC	HL

	POP	DE
	POP	AF
	JP	MEXBIH



FOSSIL_JUMP_TABLE_REF	EQU	$F3FE
FOSSIL_JUMP_TABLE_SLOT	EQU	$F400
FOSSIL_MARK_1		EQU	$f3fc
FOSSIL_MARK_2		EQU	$f3fd

EXTBIO_RC_INSTALL_FOSSIL:
	LD	HL, (WORK)
	LD	DE, FOSSIL_DRV_START
	EX	DE, HL
	LD	BC, FOSSIL_DRV_LENGTH
	LDIR

	; relocate the fossil driver
	LD	IX, (WORK)
	LD	HL, FOSSILE_DRV_MAP + 2
	LD	BC, (FOSSILE_DRV_MAP)

	LD B,B
	JR $+2
	exx
	ld	de, (WORK)	; DE' => amount required to be added to
	exx

	PUSH	BC

	LD	B, 8
	LD	A, (HL)
NEXT:
	RLCA
	JR	NC, SKIP

	EXX
	LD	L, (IX)
	INC	IX
	LD	H, (IX)
	ADD	HL, DE
	LD	(IX), H
	DEC	IX
	LD	(IX), L
	EXX

SKIP:
	INC	IX
	DEC	B
	JR	NZ, NEXT

	POP	BC
	INC	HL


	; INJECT FOSSIL MARKER
	LD	HL, 'R' + 'S' * 256
	LD	(FSMARK), HL

	; SET FOSSIL JUMP TABLE ADDRESS
	LD	HL, (WORK)
	LD	(FSTABL), HL

	POP	AF
	RET

FOSSIL_DRV_START:
	INCBIN	"bin/fossil_000.bin"
FOSSIL_DRV_LENGTH	EQU	$-FOSSIL_DRV_START

FOSSILE_DRV_MAP:
	INCBIN	"bin/fossil-map.bin"

