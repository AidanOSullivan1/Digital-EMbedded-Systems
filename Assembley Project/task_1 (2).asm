;********************************************************************
; Example program for Analog Devices EVAL-ADuC841 board.
; Based on example code from Analog Devices, 
; author        : Josh King, Aidan O'Sullivan
; Date          : Febuary 2021
;
; File          : task_1.asm
;
; Hardware      : ADuC841 with clock frequency 11.0592 MHz
;
; Description   : Produces a square wave on port 3, bit 6 at 3600Hz.
;
;********************************************************************

$NOMOD51			; for Keil uVision - do not pre-define 8051 SFRs
$INCLUDE (MOD841)	; load this definition file instead

LED		EQU	P3.6		; P3.6 is red LED on eval board

;____________________________________________________________________
		; MAIN PROGRAM
CSEG		; working in code segment - program memory

		ORG	0000h		; starting at address 0
WAVE:	CPL LED			; Toggle the LED on or off
		MOV	A, #252		; the delay needs to be 508 cycles. since we can't fit that in A, 508-256=252
		
 
		CALL DELAY		; Call the delay subroutine
		JMP	WAVE   		; repeat indefinately

;____________________________________________________________________
		; SUBROUTINES
		
		; This subroutine should take 508 cycles total
DELAY: 
		MOV R7, #2		; This outer loop is to account for the overflow caused by 508
DLY1:   MOV	R5, A		; set number of repetitions for inner loop. Happens twice
		DJNZ R5, $		; Loop to same instruction. This happens A times (252)
		DJNZ R7, DLY1	; Loop to DLY1. This happens twice
		RET				; return from subroutine

;____________________________________________________________________

END
