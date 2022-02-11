#include <ADUC841.H>
#include "SigGenFunctions.h"
#define T1 0x01                                //T1 (P3.5) set as input
#define LOW_HALF 0x0F                          // When changing TMOD for Timer 1, doesn't affect timer 0
#define RESET_CONSTANT 1000		                 //Constant for how many adc readings to sample
#define LOW_BYTE 	0xFF		                     // mask to select lowest byte
#define MODE_SELECT P2  
#define LED_BANK P0

typedef unsigned char uint8;				// 8-bit unsigned integer
typedef unsigned short int uint16;	// 16-bit unsigned integer
typedef unsigned long int uint32;	  // 16-bit unsigned integer

// all other pins taken care of by the hardware
sfr16 RCAP2 = 0xCA;                 //timer 2 reload set to address
uint16 delayVal = 300;              // delay value for SPI 
uint16 adc_data = 0;                // initialized so that the main loop can run even with no reading
sbit LOAD = P3^0;                   //P3.0 will output to the Load input pin on the MAX display
uint32 calculated_freq;             // Variable to hold the value of frequency to be displayed onto display. Set as uint32 to show that values exceeding the max limit will produce an error
uint32 count_meas;                  // Averaged timer 1 signal edge count value. Set as uint32 to show that values exceeding the max limit will produce an error
uint8 display_write_flag;


void SPI_init(void)                                  // Function to congigure the SPI 
{                                         
	SPICON = 0x33;                                     // SPI and master mode enable, clock idles low, SPI bit rate = SCLK/16
}
void SPI_transmit(uint8 High_add, uint8 Low_data){   // Function for writing SPI data to the MAX display
	uint16 i = 0;                                      
	LOAD = 0;                                          // Load is in high at idle state, it is put low to begin a transfer
	SPIDAT = High_add;                                 // The high bits repersent a register's address that is sent to the display after being assigned to the 8 MSB of the SPIDAT register
	while(ISPI == 0){                                  // Wait until the ISPI flag goes high and the transfer is succesful before proceeding
	}
	ISPI = 0;                                          // Reset the flag to 0 so another transfer can occur
	for (i = 0; i < delayVal; i++)                     // Ensures that Din is stable for some time before the next tranfer
    {
    }
	SPIDAT = Low_data;	                                // put low byte containing the data that will be displayed in the 8 LSB of the SPIDAT register 
	while(ISPI == 0){                                   // Wait until the ISPI flag goes high and the transfer is succesful before proceeding
	}
	ISPI = 0;                                           // Resets the flag to 0
	LOAD = 1;                                           // Load goes high to cause the data to be accepted by the display
}
void ADC_setup(void){
	uint8 inputs;
  inputs = MODE_SELECT & 0x07;
																	// Set up timer 2 in timer mode, auto reload, no external control
  TR1 = 0;             						//turn off timer 1, not needed for voltage
	if (inputs == 0x01){ 						// Default mode, getting average
		RCAP2 = 0xEAA6;    						//timer 2 reload value for 0.5 second refresh time
	} 
	else{
		RCAP2 = 0x4B23;    						//timer 2 reload value for 5 seconds refresh time for min and max modes
	}
	EADC = 1;			       						//enable adc interrupt
	ADCCON1 = 0x8E;      						// set up adccon1, adccon2
	ADCCON2 = 0x10;
}

void MAX_setup(void){
	uint8 Intensity_reg_data = 0x07;
	uint8 Intensity_reg_ad = 0xA;  			//D11-D8 set to 1010, D7- D4 set as 0s, D2 - D0 set to 1s for max intensity
	uint8 Scan_reg_data = 0x07;
	uint8 Scan_reg_ad = 0x0B;       		//D11-D8 set to 1011, D7 to D3 set to 0, D2-D0 set to 1s to scan all digits
	uint8 Shutdown_reg_data = 0x01;
	uint8 Shutdown_reg_ad = 0x0C;   		//D11-D8 set to 1100, D7 to D1 set to 0, D0 set to 1 for normal operation

	SPI_transmit(Intensity_reg_ad, Intensity_reg_data);
	SPI_transmit(Scan_reg_ad, Scan_reg_data);
	SPI_transmit(Shutdown_reg_ad, Shutdown_reg_data);
}

void timer2 (void) interrupt 5        // interrupt vector at address 2Bh = 43 = 3 + 8n: n = 5 
{
	static uint16 overflow_count;      // Static retains the value between interrupts
	overflow_count++;                  // total amount of cycles needed = 65536 * 85 interrupts = 5570560 (85 was calculated to be close to target of 0.5 seconds)
	if (overflow_count == 85){         //(5570560)*(1/clock frequency) = 0.5037 is the total time between each averaged frequency being passed to main for display
	count_meas = (TH1 << 8) | TL1;     // On the 85th interrupt the count value from Timer 1 is read and stored in count_meas, which is declared gloabaly and available in the main
  //count_meas = 0x7D00;              // this mode is used to test hypothetically to see if a frequency out of range (above 65,536 Hz) will write "ERR" to the screen, as there is no high frequency option in the signal generator
	TL1 = 0;                            
	TH1 = 0;                           // Timer 1 count registers set back to 0
	overflow_count = 0;                // 85 interrupts have occured and so overflow_count is reset to 0 to repeat the process
	display_write_flag = 1;            //flag for writing to display in main. FLag means that the dispalay will only be written to once per frequency measurement, improving efficiency and use of the processor
	}
	else{
	}
	TF2 = 0;					               // clear Timer 2 interrupt flag
}


uint16 scale_voltage(uint32 input_volt){
    // Scale raw ADC input to a voltage
    // 12 bit all 1 (4096) corresponds to 2500mV
	input_volt = input_volt * 2500;
	input_volt = input_volt/4096;
	return input_volt; //returns in millivolts
}

void Write_to_Display(uint16 display_freq){
	uint8 Data1_reg_data;                                  // Holds data to be sent to the 1st display register (same for registers below)
	uint8 Data1_reg_ad = 0x01;                             // Address for 1st display register set (same for registers below)
	uint8 Data2_reg_data;
	uint8 Data2_reg_ad = 0x02;
	uint8 Data3_reg_data;
	uint8 Data3_reg_ad = 0x03;
	uint8 Data7_reg_data;
	uint8 Data7_reg_ad = 0x07;
	uint8 Data8_reg_data;
	uint8 Data8_reg_ad = 0x08;
	uint8 reg_ad;
	uint8 reg_data;
	uint8 freq_mode = MODE_SELECT & 0x08; //this bit is 1 only if we are displaying frequencies. Else, voltage

	uint16 data_bit_significance = (freq_mode) ? 10000 : 1000;                  // set as 10,000 as max frequency  to be displayed = 65536 Hz (same number of digits)
	
    Data8_reg_data = 0x0F;                                 //clears leftmost digit     
	SPI_transmit(Data8_reg_ad, Data8_reg_data);            // Transmits data to the leftmort digit 

    if (!freq_mode){
        // Voltage display
			uint8 Decode_reg_data = 0xFC;
			uint8 Decode_reg_ad = 0x09;   // D11-D8 set to 1001, D7 - D2 set to 1 for decode mode, D1-D0 set to 0s, not in Decode mode
			SPI_transmit(Decode_reg_ad, Decode_reg_data);
    	Data7_reg_data = 0x0F;                        //clears second leftmost digit     
    	SPI_transmit(Data7_reg_ad, Data7_reg_data);
	    for(reg_ad = 0x06; reg_ad > 0x02; reg_ad--){          // for measurement values, digits 7-3
	    	reg_data = display_freq/data_bit_significance;
	    	SPI_transmit(reg_ad, reg_data);
	    	display_freq %= data_bit_significance;
	    	data_bit_significance /= 10; //TODO: this is a slow operation. May not be best way? Could also do a lookup of data_bit_significance values
	    }
		  Data2_reg_data = 0x6A;                              //write unit 'm'
		SPI_transmit(Data2_reg_ad, Data2_reg_data);
          
		Data1_reg_data = 0x3E;                              //write unit 'V'
		SPI_transmit(Data1_reg_ad, Data1_reg_data);
    } else{
        // Frequency display
	    if(display_freq <= 0xEA60){                            // Limit set as freq = 60000 HZ. Error will be displayed if this is exceeded
           uint8 Decode_reg_data = 0xFE;                          // Has to be included in case the display was previously showing an error and decode mode was switched off and then the frequency was put back in range
           uint8 Decode_reg_ad = 0x09;                            // D11-D8 set to 1001, D7 - D1 set to 1 for everything in decode mode except D0
           SPI_transmit(Decode_reg_ad, Decode_reg_data);
           for(reg_ad = 0x07; reg_ad > 0x02; reg_ad--){             // Measurement value displayed on digits 7-3, where up to 5 digit numerical frequency will be written
               reg_data = display_freq/data_bit_significance;       // Indiviual integer from each position of the frequency acquired
               SPI_transmit(reg_ad, reg_data);                      // Integer transferred to be displayed
               display_freq %= data_bit_significance;               // Modulo operation to remove the current first digit of the frequency, that has just been displayed
               data_bit_significance /= 10;                         // variable reduced by a factor of 10 so the next highest order digit can be retrieved and displayed
           }

           Data2_reg_data = 0x0C;                              // Write unit 'H'
           SPI_transmit(Data2_reg_ad, Data2_reg_data);

           Data1_reg_data = 0x6D;                              // Write unit 'Z'
           SPI_transmit(Data1_reg_ad, Data1_reg_data);
           display_write_flag = 0;                             // Flag prevents the display from updating until a new frequency is calculated
       }
       else{                                                 //When the frequency is out of range "Err" is written to the screen
           uint8 Decode_reg_data = 0xFC;
           uint8 Decode_reg_ad = 0x09;                         // D11-D8 set to 1001, D7 - D2 in decode mode, D1 - D0 are 0s and not in decode mode
           SPI_transmit(Decode_reg_ad, Decode_reg_data);
           for(reg_ad = 0x07; reg_ad > 0x03; reg_ad--){          // Turn off digits 7 to 4, not needed
               reg_data = 0x0F;                            
               SPI_transmit(reg_ad, reg_data);
           }
           Data3_reg_data = 0x0B;                              //write unit 'E'
           SPI_transmit(Data3_reg_ad, Data3_reg_data);
	
           Data2_reg_data = 0x05;                              //write unit 'r'
           SPI_transmit(Data2_reg_ad, Data2_reg_data);

           Data1_reg_data = 0x05;                              //write unit 'r'
           SPI_transmit(Data1_reg_ad, Data1_reg_data);
           display_write_flag = 0;                             //Display will not be updated until a new frequency is calculated
       }
    }
}

void adc_interrupt(void) interrupt 6{ 
	static uint32 sum_of_voltages;
	static uint16 counter = 0;
	uint32 new_value = (ADCDATAH << 8) | ADCDATAL & 0x0FFF; //concat readings to single value. Also makes sure most significant 4 bits are 0
    uint8 inputs;
    inputs = MODE_SELECT & 0x07;
	if (inputs == 0x01){                                    // Default mode, getting average
		sum_of_voltages += new_value; 
	} else if(inputs == 0x02){                              // Min mode, checking for lower value
		if (counter == 0) sum_of_voltages = 0xFFF;            // Added as minimum wouldn't display anything because sum_of_voltages begins at 0 and will always remain the minimum and cause the display to be stuck at 0 mV
		if (sum_of_voltages > new_value)
			sum_of_voltages = new_value; 
	} else if(inputs == 0x04){                              // Max mode, checking for higher value
		if (sum_of_voltages < new_value)
			sum_of_voltages = new_value;
		
	}
    counter++;
    if (counter >= RESET_CONSTANT){
		if (inputs == 0x01){
			sum_of_voltages = sum_of_voltages / counter;
		}
        adc_data = sum_of_voltages;
        counter = 0;
        sum_of_voltages = 0;
    }
    TF2 = 0;                                              // reset timer2 overflow
}

void main (void) {
  uint8 inputs;
	uint16 voltage;
	TMOD = (TMOD & LOW_HALF) | 0x50;                   //Timer 1 set as a 16-bit counter in mode 1, Timer 0 unaltered    | means a bitwise OR operation 
	EA = 1;                                            // All interrupts enabled
	T2CON = 0x04;                                      // Timer 2 started
	TR2 = 1;                                           // Timer 2 enabled
	ET2 = 1;                                           // Timer 2 interrupt enabled 
	SPI_init();                                        // SPI intiated
  MAX_setup();                                       // MAX display set up function called

	while (1){
		uint8 freq_mode = MODE_SELECT & 0x08;
		inputs = MODE_SELECT & 0x0F;                     // mask zeros out unused significant bits
		LED_BANK = ~inputs;                              //sets LED to whatever mode we are in// Loop forever, repeating tasks 
                                                     //if (inputs & 0x08 == 0x08) // Checks if P2.3 is set
		if (freq_mode)
        { //frequency
			      TR1 = 1;                                           // Timer 1 enabled
            SigGenSetup();		                                 // Initialise the signal generator
            EADC = 0;
            RCAP2 = 0x0;
            if(display_write_flag == 1){                       //flag used to only display when a new value is retrieved after every averaging reset
                calculated_freq = count_meas/0.5037;           // Total cycles counted by Timer 1, obtained during 85 interrupts, divided by time taken for 85 interrupts to give the measured frequency 
                Write_to_Display(calculated_freq);             //write to lcd
            }
        }
        else {                                                 // This should only trigger when P2.0 - P2.2 is set.
            // voltage
            ADC_setup();
            voltage = scale_voltage(adc_data);
            Write_to_Display(voltage);
        }
    }
}