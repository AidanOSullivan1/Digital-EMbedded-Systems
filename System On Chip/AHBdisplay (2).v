`timescale 1ns / 1ns
//////////////////////////////////////////////////////////////////////////////////
// Company: UCD School of Electrical and Electronic Engineering
// Engineer: Brian Mulkeen
// 
// Create Date:     6 November 2015 
// Design Name:     Cortex-M0 DesignStart system
// Module Name:     AHBdisplay
// Description: 	Provides AHB interface to drive multiplexed 8-digit 
//                  7-segment display on Nexys-4 board.
//		Address 0 - 32-bit read/write, raw data for segments on digits 3 to 0
//		Address 4 - 32-bit read/write, raw data for segments on digits 7 to 4 
//		Address 8 - 32-bit read/write, number to be displayed in hexadecimal
//		Address C - 32-bit read/write, control register, viewed as 4 bytes
//			byte 3 - leftmost byte, not used, will read as 0. 
//			byte 2 - Enable for digits 7 to 0: 1 = enabled, 0 = off.
//			byte 1 - Mode control for digits 7 to 0: 1 = hexadecimal, 0 = raw. 
//			byte 0 - Dot control for digits 7 to 0 in hex mode, 1 = ON.
//		This version only handles 32-bit bus transactions.
//
//		The display refresh rate is the clock frequency divided by
//		2^D_WIDTH.  D_WIDTH is a parameter, with default value 20, which sets
//		number of bits in a counter that controls cycling through the digits.
//		With a 50 MHz clock, D_WIDTH = 20 gives refresh rate ~48 Hz, so the 
//		display cycles through all 8 digits every ~21 ms.
//
// Revision: March 2021 - comments updated, some internal signal names changed.
//
//////////////////////////////////////////////////////////////////////////////////
module  AHBdisplay #(D_WIDTH = 20) (
			// Bus signals
			input wire HCLK,			// bus clock
			input wire HRESETn,			// bus reset, active low
			input wire HSEL,			// selects this slave
			input wire HREADY,			// indicates previous transaction completing
			input wire [31:0] HADDR,	// address
			input wire [1:0] HTRANS,	// transaction type (only bit 1 used)
			input wire HWRITE,			// write transaction
//			input wire [2:0] HSIZE,		// transaction width ignored
			input wire [31:0] HWDATA,	// write data
			output wire [31:0] HRDATA,	// read data from slave
			output wire HREADYOUT,		// ready output from slave
			// LED output signals
    		output reg [7:0] digit,	    // digit enable lines, active low, 0 on right
			output reg [7:0] segment	// segment lines, active low, ABCDEFGP
    );

//================================  AHB-Lite Bus Interface =============================
	 // Address bits for registers
	localparam [1:0] RAWL = 2'h0, RAWH = 2'h1, HEXD = 2'h2, CTRL = 2'h3;
	
	// Registers to hold signals from address phase
	reg [1:0] rHADDR;			// only need two bits of address
	reg rWrite;					// write enable signal

	// Internal signals
	reg [31:0] readData;       // ouptut of read multiplexer

 	// Capture bus signals in address phase
	always @ (posedge HCLK)
		if (!HRESETn)
			begin
				rHADDR <= 2'b0;
				rWrite <= 1'b0;
			end
		else if (HREADY)	// previous bus transaction is completing
            begin
                rHADDR <= HADDR[3:2];  // capture address bits for for use in data phase
                rWrite <= HSEL & HWRITE & HTRANS[1]; // slave selected for write transfer       
            end

	// Registers visible on the AHB-Lite bus, as described above
	reg [31:0] rawLow, rawHigh, hexData;
	reg [23:0] control;
	always @ (posedge HCLK)
		if (!HRESETn) 
			begin
				rawLow  <= 32'd0;
				rawHigh <= 32'd0;
				hexData <= 32'd0;
				control <= 24'b0;
			end
		else if (rWrite)  // writing to a register
			case (rHADDR)
				RAWL:   rawLow  <= HWDATA[31:0];
				RAWH:   rawHigh <= HWDATA[31:0];
				HEXD:   hexData <= HWDATA[31:0];
				CTRL:   control <= HWDATA[23:0];
			endcase

	// Bus read data
	always @(rawLow, rawHigh, hexData, control, rHADDR)
		case (rHADDR)		// select on word address (stored from address phase)
			RAWL:		readData = rawLow;	// read back raw low register
			RAWH:		readData = rawHigh;	
			HEXD:		readData = hexData;    
			CTRL:		readData = {8'b0, control};	// control register with 0 bits
		endcase
		
	assign HRDATA = readData;	
	assign HREADYOUT = 1'b1;	// always ready - transaction is never delayed
	
//================================  Display Interface ===============================

	reg [D_WIDTH-1:0] clkCount; // counter to scan digits
	wire [2:0] digitSel;   // 3-bit value to select one digit
	reg [7:0]  pattern;    // raw pattern for this digit
	reg [3:0]  value;      // 4-bit value for this digit
	reg        dot;        // decimal point for this digit
	wire [6:0] hexPattern; // pattern to represent hex digit
	reg [1:0]  digitMode;  // mode for this digit:  0x = off, 10 = raw, 11 = hex
	
	// Scanning counter driven by clock
	always @ (posedge HCLK)        
		if (!HRESETn) clkCount <= {D_WIDTH{1'b0}};
		else clkCount <= clkCount + 1'b1;

	// 3-bit signal to select the active digit
	assign digitSel = clkCount[D_WIDTH-1:D_WIDTH-3];  // 3 MSB of counter	

	// 3 to 8 decoder, active low outputs, to enable one digit of display
	always @ (digitSel)
		case(digitSel)
            3'b000:  digit = 8'b11111110;  // enable rightmost digit
            3'b001:  digit = 8'b11111101;
            3'b010:  digit = 8'b11111011;
            3'b011:  digit = 8'b11110111;
            3'b100:  digit = 8'b11101111;
            3'b101:  digit = 8'b11011111;
            3'b110:  digit = 8'b10111111;
            3'b111:  digit = 8'b01111111;  // enable leftmost digit
        endcase  // no need for default, as all possibilities covered

	// Multiplexer to select the pattern for use in raw mode
	always @ (digitSel or rawLow or rawHigh)
        case (digitSel)
            3'b000:  pattern = rawLow[7:0];  // pattern for rightmost digit
            3'b001:  pattern = rawLow[15:8];
            3'b010:  pattern = rawLow[23:16];
            3'b011:  pattern = rawLow[31:24];
            3'b100:  pattern = rawHigh[7:0];
            3'b101:  pattern = rawHigh[15:8];
            3'b110:  pattern = rawHigh[23:16];
            3'b111:  pattern = rawHigh[31:24];  // pattern for leftmost digit
        endcase  

	// Multiplexer to select the value for use in hexadecimal mode
	always @ (digitSel or hexData)
        case (digitSel)
            3'b000:  value = hexData[3:0];  // value for rightmost digit
            3'b001:  value = hexData[7:4];
            3'b010:  value = hexData[11:8];
            3'b011:  value = hexData[15:12];
            3'b100:  value = hexData[19:16];
            3'b101:  value = hexData[23:20];
            3'b110:  value = hexData[27:24];
            3'b111:  value = hexData[31:28];  // value for leftmost digit
        endcase  

	// Multiplexer to select the dot for use in hexadecimal mode
	always @ (digitSel or control)
        case (digitSel)
            3'b000:  dot = control[0];  // point for rightmost digit
            3'b001:  dot = control[1]; 
            3'b010:  dot = control[2]; 
            3'b011:  dot = control[3]; 
            3'b100:  dot = control[4]; 
            3'b101:  dot = control[5]; 
            3'b110:  dot = control[6]; 
            3'b111:  dot = control[7];   // point for leftmost digit
        endcase  

	// Multiplexer to select mode for this digit: 0x = off, 10 = raw mode, 11 = hex mode
    always @ (digitSel or control)
        case (digitSel)
            3'b000:  digitMode = {control[16], control[8]};  // mode for rightmost digit
            3'b001:  digitMode = {control[17], control[9]}; 
            3'b010:  digitMode = {control[18], control[10]}; 
            3'b011:  digitMode = {control[19], control[11]}; 
            3'b100:  digitMode = {control[20], control[12]}; 
            3'b101:  digitMode = {control[21], control[13]}; 
            3'b110:  digitMode = {control[22], control[14]}; 
            3'b111:  digitMode = {control[23], control[15]};   // mode for leftmost digit
        endcase  

	// Instantiate block to convert 4-bit value to 7-segment pattern
	hex2seg LUT ( .number( value ),		
		.pattern( hexPattern ) );

	// Multiplexer to choose segment pattern according to mode
    always @ (digitMode or pattern or hexPattern or dot)
        case (digitMode)
            2'b00, 2'b01:   segment = 8'hFF;    // all segments off
            2'b10:          segment = ~pattern;  // raw mode
            2'b11:          segment = {hexPattern, ~dot}; // hex mode
        endcase
   
endmodule
