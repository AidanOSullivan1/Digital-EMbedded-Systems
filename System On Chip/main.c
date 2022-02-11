//------------------------------------------------------------------------------------------------------
// Demonstration program for Cortex-M0 SoC design
// 1)Enable the interrupt for UART - interrupt when character received
// 2)Go to sleep mode
// 3)On interruption, echo character back to the UART and collect into buffer
// 4)When a whole sentence has been received, invert the case and send it back
// 5)Loop forever doing the above.
//
// Version 4 - October 2015
//------------------------------------------------------------------------------------------------------
#include <stdio.h>
#include "DES_M0_SoC.h"

#define BUF_SIZE						100
#define ASCII_CR						'\r'
#define nLOOPS_per_DELAY		1000000

#define ARRAY_SIZE(__x__)       (sizeof(__x__)/sizeof(__x__[0]))
#define MOSI 0x00
#define MISO 0x00
#define write_data 0x0A     // Byte sent on MOSI when writing to ADXL362
#define read_data 0x0B      // Byte sent on MOSI when reading to ADXL362
#define delay_val 10        // For short delays between clock changing state
#define clock_low 0x00      // Used to set the SPI clock low
#define clock_high 0x01     // Used to set the SPI clock high
#define cs_high 0x01        // sed to set Chip select high
#define cs_low 0x00         // Used to set Chip select low
#define deviceID_reg0 0x00  // Device addresses used while testing
#define deviceID_reg1 0x01  // Device addresses used while testing

volatile uint8  counter  = 0; // current number of char received on UART currently in RxBuf[]
volatile uint8  BufReady = 0; // Flag to indicate if there is a sentence worth of data in RxBuf
volatile uint8  RxBuf[BUF_SIZE];

void send_bit(uint8 mosi_bit, uint8 clock_state){    // USed to send bits on MOSI line and simultaneously change state of clock
	pt2GPIO->acc_out = clock_state<<2 | mosi_bit<<1;   // Pointer used to address the bit assigned to clock and MOSI
}

void wait_n_loops(uint32 n) {         // Delay for half period of clock
	volatile uint32 i;
		for(i=0;i<n;i++){
			;
		}
}

void bit_gen(uint8 byte2send){         // Function removes LSB bit from byte to be sent on MOSI
	volatile uint8 i;
	volatile uint8 bit2send;
	for(i=0; i<8; i++){
		bit2send = (byte2send>>(7-i))&1;   // Bit removed from byte and asssigned to bitssend
		send_bit(bit2send, clock_low);     // Bit sent and clock set low
		wait_n_loops(delay_val);           //insert delay to hold state of clock at low for short time
		send_bit(bit2send, clock_high);    // change the state of the clock to HIGH
		wait_n_loops(delay_val);           // Hold clcok state at high for short time before the ext bit is generated and clock will go low again
	}
}

void accel_setup(){               // Used to configure ADXL362
	uint8 pow_con_add = 0x2D;      // Power control register address
	uint8 pow_con_data = 0x12;     // Measurement mode and low noise mode
	pt2GPIO->acc_out = cs_high;    //cs initiliased as high
	wait_n_loops(delay_val);       // Dealy
	pt2GPIO->acc_out = cs_low;     //cs set to 0 to begin data transfer
	bit_gen(write_data);           // Write mode set by sending byte on MOSI
	bit_gen(pow_con_add);          // Address of register to be written to
	bit_gen(pow_con_data);				//measurement mode and low noise mode
	pt2GPIO->acc_out = cs_high;   //cs iniliased as high
}

void mosi_clock(uint8 clock_state){    // Function to change the state of clock while receiving MISO data
	pt2GPIO->acc_out = clock_state<<2;   // clcok changed state
}

uint8 read_miso(){                     // This function returns each bit, MSB first and LSB last and reurns it to the byte_gen() to be assembled
	volatile uint8 r_bit;          
	//clock low, delay
	mosi_clock(clock_low);               // Clock set low
	wait_n_loops(delay_val);             // delays clcok in low state
	r_bit = pt2GPIO->Buttons>>5;         // Assigns r-bit to the address in GPIO input 1 that the MISO signal is assigned
	//clock high_delay 
	mosi_clock(clock_high);              // Clock set high causing the MISO bit to be received
	wait_n_loops(delay_val);
	return r_bit;
}

uint8 byte_gen(){                  // Receives MISO bits and assemebles as a byte, MSB received first and LSB last
	volatile uint8 bit;
	volatile uint8 byte;
	volatile uint8 i;
	for(i=0; i<8; i++){
		bit = read_miso();             // Bit returned from read_miso()
		byte = (byte)| bit<<(7-i);     // Assembled as a byte
	}
	return byte;
}

int32 convert_acc_value(int raw_value){ // Function to take the raw value of the adc on the accelerometer to meaningful values (in mg)
	return raw_value * ( 2000/ 128); // assumes accelerometer mode of operation to have += 2G of tolerance. 
}

void set_display(int32 value){ // Function to write a value (in mGs) to the display. Unfortunately, displays the value in hex. 
	int control;
	value = value<<8; // Move over two hex display places to make space for units
	if (value < 0){
		pt2Display->rawHigh = 0x200; //write minus symbol
		value*= -1; // invert value to make it wasier to work with
		control = 0x3F1C00; // Control acounts for negative sign
	}
	else{
		control = 0x3F3C00; // Control does nto account for negative sign
	}
	pt2Display->rawLow = 0x155BC; //mG
	pt2Display->hexData = value; 
	pt2Display->control = control;
}

//////////////////////////////////////////////////////////////////
// Interrupt service routine, runs when UART interrupt occurs - see cm0dsasm.s
//////////////////////////////////////////////////////////////////
void UART_ISR(){
	char c;
	c = pt2UART->RxData;	 // read a character from UART - interrupt only occurs when character waiting
	RxBuf[counter]  = c;   // Store in buffer
	counter++;             // Increment counter to indicate that there is now 1 more character in buffer
	pt2UART->TxData = c;   // write (echo) the character to UART (assuming transmit queue not full!)
	// counter is now the position that the next character should go into
	// If this is the end of the buffer, i.e. if counter==BUF_SIZE-1, then null terminate
	// and indicate the a complete sentence has been received.
	// If the character just put in was a carriage return, do likewise.
	if (counter == BUF_SIZE-1 || c == ASCII_CR)  {
		counter--;							// decrement counter (CR will be over-written)
		RxBuf[counter] = NULL;  // Null terminate
		BufReady       = 1;	    // Indicate to rest of code that a full "sentence" has being received (and is in RxBuf)
	}
}

void set_LED(int8 acc_val){ // This function taskes the acceleration value from the ADC and determines the LED to illuminate
	int8 acc_value_ranges[] = {-112, -96, -80, -64, -40, -32, -16, 0, 16, 32, 48, 64, 80, 96, 112}; // bins for acceleration ranges
	int x;
	int LED_value = 1; // holds the value of the LED. Loop moves the LED over one by one. 
	for (x=0; x<15; x++){
		if (acc_val > acc_value_ranges[x]){
			LED_value = LED_value<<1; // Moves LED over by one
		}
	}
	pt2GPIO->LED = LED_value; // Write value to LED
}

//////////////////////////////////////////////////////////////////
// Main Function
//////////////////////////////////////////////////////////////////
int main(void) {
	uint32 long_delay = 2000000;
	int8 acc_val;
	uint8 reg0_data;
	int32 scaled_acc;
	uint8 count = 0;
	pt2UART->Control = (1 << UART_RX_FIFO_EMPTY_BIT_INT_POS);		// Enable rx data available interrupt, and no others.
	pt2NVIC->Enable	 = (1 << NVIC_UART_BIT_POS);								// Enable interrupts for UART in the NVIC
	wait_n_loops(nLOOPS_per_DELAY);										// wait a little
	printf("\r\nWelcome to to the Acceleration measurement program\r\n");			  // output welcome message in terminal followed by instructions
	printf("Press the rightmost switch only to measure on the Y-axis\r\n");
	printf("Press the 2nd rightmost switch only to measure on the X-axis\r\n");
	printf("Press the 3rd rightmost switch only to measure on the Z-axis\r\n");
	printf("Press the leftmost switch to continue\r\n");                        
	while ((pt2GPIO->Switches&0x8000) != 0x8000){                               // Holds messages on screen and waits for user input
	}
	accel_setup();                                  // Configures ADXL362
	while(1){			                                  // loop forever]
			uint8 y_data_add = 0x09;                    // Addresses for all 8-bit data register for each axis
			uint8 x_data_add = 0X08;
			uint8 z_data_add = 0X0A;
			pt2GPIO->acc_out = cs_low;                  //cs set to 0 to begin data transfer
			bit_gen(read_data);                         // Tells ADXL362 that data will be read
			//bit_gen(deviceID_reg1);                   // Used to test SPI and confirm that the data was correct as these register's data were known
			//reg1_data = byte_gen();
		  if((pt2GPIO->Switches&0x01) == 0x01){       // Allows user to chose axis on which acceleration will be read
			bit_gen(y_data_add); }
			if((pt2GPIO->Switches&0x02) == 0x02){ 
			bit_gen(x_data_add); } 
			if((pt2GPIO->Switches&0x04) == 0x04){ 
			bit_gen(z_data_add); } 
		  acc_val = (int8)byte_gen();                 // Reads acceleration byte
			pt2GPIO->acc_out = cs_high;                 //cs iniliased as high after data is read
			
		  //printf("acc_val: %d\n\r",acc_val);
			//printf("Device ID: %d\n\r",reg0_data);
			scaled_acc = convert_acc_value(acc_val);      // Data scaled to mG, uint32 bit used as data would later be shifted when put onto the display
			printf("acc_val: %d mG\n\r",scaled_acc);      // Shows the acceleration data in mG in the terminal
			set_display(scaled_acc);                      // Sent to the display
			set_LED(acc_val);															// Set LED based on acceleration value
		if (acc_val == 127 | acc_val == -127){
				count++;
				printf("High impact detected\n");
				printf("Number of impacts detected: %d\n\r", count);
				wait_n_loops(long_delay);
				wait_n_loops(long_delay);
				wait_n_loops(long_delay);
			}
			wait_n_loops(long_delay);
			//wait_n_loops(long_delay);
			
			// ---- end of critical section ----		
			
			pt2NVIC->Enable	 = (1 << NVIC_UART_BIT_POS);		// Enable interrupts for UART in the NVIC

		} // end of infinite loop

}  // end of main


