; Demo Program - using timer interrupts.
; Written for ADuC841 evaluation board, with UCD extras.
; Generates a square wave on P3.6.
; Brian Mulkeen, September 2016

; Include the ADuC841 SFR definitions
$NOMOD51
$INCLUDE (MOD841)
	
SOUND	EQU  	P3.6		; P3.6 will drive a transducer
LED		EQU		P3.4		; P3.4 is red LED on eval board

CSEG
		ORG		0000h		; set origin at start of code segment
		JMP		MAIN		; jump to start of main program
		
		ORG		002Bh		; Timer 2 overflow interrupt address
		JMP		TF2ISR		; jump to interrupt service routine
		
		ORG		003h		; External Interrupt 0 address
		JMP		INT0ISR		; jump to interrupt service routine

		ORG		0060h		; set origin above interrupt addresses*/	
		LOWER: DB 66h, 33h, 0CDh, 09Ah, 0AEh, 066h, 0EAh, 04Dh           ;Storing RCAP2L values in a lookup table
		HIGHER: DB 0EAh, 0F5h, 0F8h, 0FAh, 0FBh, 0FCh, 0FCh, 0FDh		 ;Storing RCAP2H values in a lookup table
		LED_table:	DB 0FEh, 0FDh, 0FBh, 0F7h, 0EFh, 0DFh, 0BFh, 7Fh	 ;Storing LED values in a lookup table
MAIN:	
; ------ Setup part - happens once only ----------------------------
		MOV A, #0FFh
		MOV P2, A     ;P2 set as input port to read values from the switches
		;Timer 2 configuration to create the signal wave
		MOV	T2CON, #00h	 ; Timer 2 as a timer
		SETB EA          ;Enable all interrupts
		SETB ET2         ;Enable timer 2 interrupt
		SETB TR2	     ; start Timer 2
		MOV IP, 20h     ; Timer 2 interrupt set as high priority, external interrupt 0 is low priority so it doesn't interfere with the signal
		
		;External interrupt 0 to turn LED ON/OFF if INT0 is pressed
		SETB IT0        ;Interrupt on a 1- to-0 transition
		SETB EX0	    ; enable External interrupt 0
		SETB C          ; used to keep the flashing LED OFF if the button was pressed or otherwise continue flashing
		
; ------ Loop forever -------------
LOOP:	
		JNC LOW_func     ;if the INT0 button was pressed C will be 0 and the following 2 commands will not be executed, keeping P3.4 OFF
		CPL LED          ;toggles the state of the LED
		CALL DELAY       ;blinks LED on P3.4
		LOW_func:	         ;set low reload values
		MOV A, P2            ; reads switch values from P2
		ANL A, #07h          ; ANDs the switch value with 00000111 to eliminate switchs 4-8, by making them 0s
		MOV DPTR, #LOWER      ; gets address of first value from look up table LOWER and saves in the pointer DPTR 
		MOVC A, @A + DPTR     ; Offsets 1st address by the value of the switch
		MOV RCAP2L, A  		  ; Saves the offset value into the RCAP2L register
		HIGH_func:            ;sets high reload values
		MOV A, P2			  ; reads switch values from P2
		ANL A, #07h			  ; ANDs the switch value with 00000111 to eliminate switchs 4-8, by making them 0s
		MOV DPTR, #HIGHER     ; gets address of first value from look up table HIGHER and saves in the pointer DPTR
		MOVC A, @A + DPTR     ; Offsets 1st address by the value of the switch
		MOV RCAP2H, A         ; Saves the offset value into the RCAP2H register
		LED_func:             ; sets LEDs on P0
		MOV A, P2             ;AS before with the LED_table look-up table
		ANL A, #07h  
		MOV DPTR, #LED_table
		MOVC A, @A + DPTR
		MOV P0, A
		
		JMP	LOOP
		
DELAY:	; delay for time R5 x 10 ms = 500 ms
		MOV	R5, #50		; set number of repetitions for outer loop
DLY2:	MOV	R6, #144		; middle loop repeats 144 times         
DLY1:	MOV	R7, #255		; inner loop repeats 255 times      
		DJNZ	R7, $		; inner loop 255 x 3 cycles = 765 cycles            
		DJNZ	R6, DLY1		; + 5 to reload, x 144 = 110880 cycles
		DJNZ	R5, DLY2		; + 5 to reload = 110885 cycles = 10.0264 ms
		RET			

	
; ------ Interrupt service routine ---------------------------------	
TF2ISR:		; Timer 2 overflow interrupt service routine
		CPL	SOUND		; change state of output pin
		CLR TF2             ; clear flag
		RETI				; return from interrupt
; ------------------------------------------------------------------	
; ------ Interrupt service routine ---------------------------------	

INT0ISR:		        ; Timer 0 overflow interrupt service routine
		SETB	LED		; change state of output pin to OFF (LED is off when it's at state  = 1)
		CPL C           ; Cahnge state of C so the LED will either remain OFF or start flashing until the button is pressed again causing another interrupt
		RETI				; return from interrupt
		
		
; ------------------------------------------------------------------	
		
END