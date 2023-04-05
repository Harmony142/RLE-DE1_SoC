#include "hps_soc_system.h"
#include "socal.h"
#inlcude "hps.h"
#include "address_map_arm.h"
#include <stdio.h>
#include <time.h>

// #include "address_map_arm.h"

#define KEY_BASE              0xFF200050
#define VIDEO_IN_BASE         0xFF203060
#define FPGA_ONCHIP_BASE      0xC8000000
#define SW_BASE				  0xFF200040
#define SDRAM_BASE			  0xC0000000
#define FPGA_CHAR_BASE		  0xC9000000

/* This program demonstrates the use of the D5M camera with the DE1-SoC Board
 * It performs the following: 
 * 	1. Capture one frame of video when any key is pressed.
 * 	2. Display the captured frame when any key is pressed.		  
*/

/* Note: Set the switches SW1 and SW2 to high and rest of the switches to low for correct exposure timing while compiling and the loading the program in the Altera Monitor program.
*/

/*
* capture the image (DONE)
* convert to one-bit-per-pixel B&W (DONE)
* compress image
* store in SDRAM
* decompress image
* display image
*/
int avg, sum, x, y;

int comp = 0;
int pix_byte[(320*240)/8];

int main(void)
{
	/*volatile int * KEY_ptr		= (int *) KEY_BASE;*/
	volatile int * Video_In_DMA_ptr	= (int *) VIDEO_IN_BASE;
	volatile int * SDRAM_PTR		= (int *) SDRAM_BASE;
	volatile int * HPS_GPI01        = (int *) HPS_GPI01_BASE;
	volatile int * SW_ptr			= (int *) SW_BASE
	volatile short * Video_Mem_ptr	= (short *) FPGA_ONCHIP_BASE;

	*(Video_In_DMA_ptr + 3)	= 0x4;				// Enable the video

	while (1)
	{
		if (*SW_ptr != 0x1)						// check if any KEY was pressed
		{
			*(Video_In_DMA_ptr + 3) = 0x0;			// Disable the video to capture one frame
			while (*SW_ptr != 0x0);				// wait for pushbutton KEY release
			break;
		}
		
		if (*SW_ptr == 0x03){
			black_and_white();
		}
		
	}


	/*for (y = 0; y < 240; y++) {
		for (x = 0; x < 320; x++) {
			short temp2 = *(Video_Mem_ptr + (y << 9) + x);
			*(Video_Mem_ptr + (y << 9) + x) = temp2;
		}
	}*/

}

void black_and_white(){
	
	for(y = 0; y < 240; y++){
		for(x = 0; x < 320; x++){
			sum = sum + *(Video_Mem_ptr + (y << 9) + x);
			/* Sum each pixel value on screen */
		}
	}
	
	avg = sum/(320*240); /*Find the Average pixel value*/
	
	for(y = 0; y < 240; y++){
		for (x = 0; x < 320; x++){
			/* Go through each pixel on screen */
			if(*(Video_Mem_ptr + (y << 9) + x) < avg){
				*(Video_Mem_ptr + (y << 9) + x) = 0x0; 
				
				/* If the pixel value is less than the average color, 
				   display the pixel as black */
			}
			else{
				*(Video_Mem_ptr + (y << 9) + x) = 0xFFFF;
				
				/* If the pixel value is greater than or equal to the
				   average color, display the pixel as white */
			}
		}
	}
	
	sum= 0;
	
	OBPP(); //Call One Bit Per Pixel
	
}

void OBPP(){ //Convert to One Bit Per Pixel
	
	int count = 0;
	int index = 0;
	int data; //bit stream to be put thorugh RLE and compression
	
	
	for(y = 0; y < 240; y++){
		for(x = 0; x < 320; x ++){
			
			if(count < 8){
				if(*(Video_Mem_ptr + (y << 9) + x) == 0x0 ){ //If the pixel is black
					
					data >> 0;
					count++;
					
					
				}	
				else{
					
					data >> 1;
					count++;
				}
			}
			else{
				pix_byte[index] = data;
				index++;
				data << 8;
				count = 0;
			}
		}
	}
}

void ENC_and_COMP(){ /* Encode each byte of data using RLE then compress the image*/
	
	int IndexCt = 0;
	
	while(IndexCt<=(9600)){ //make sure count is less than 9600...screen dimension in bytes
			alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST+FIFO_IN_WRITE_REQ_PIO_BASE,1); /* Write bitstream to the FIFO*/
			alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST+ODATA_PIO_BASE, pix_byte[IndexCt]); /* Out data through RLE */
			alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST+FIFO_IN_WRITE_REQ_PIO_BASE,0); /* Finish writing to the FIFO*/
			
			IndexCt++; /* Increase the index count for pix_byte to get next byte to put through RLE */
			
		if(alt_read_byte(RESULT_READY_PIO_BASE+ALT_FPGA_BRIDGE_LWH2F_OFST)==0){
			alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST+FIFO_OUT_READ_REQ_PIO_BASE,1); /* Take bitstream from FIFO */
			deCOMP(alt_read_word(ALT_FPGA_BRIDGE_LWH2F_OFST+IDATA_PIO_BASE)); /* Decompress bit stream */
			alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST+FIFO_OUT_READ_REQ_PIO_BASE,0);	 /* Finish taking bit stream from FIFO */			
		}
	}
	
	/* Reset everything to prepare for next decompression */
	
	alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + RLE_FLUSH_PIO_BASE, 1); 
	alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + RLE_FLUSH_PIO_BASE, 0); 
	alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + FIFO_OUT_READ_REQ_PIO_BASE,1); 
	deCOMP(alt_read_word(ALT_FPGA_BRIDGE_LWH2F_OFST + IDATA_PIO_BASE));
	alt_write_byte(ALT_FPGA_BRIDGE_LWH2F_OFST + FIFO_OUT_READ_REQ_PIO_BASE,0);
	sdram_index=0; //reset index for SDRAM
	
	
	for (y=0;y<240; y++) {
		for (x=0;x<320; x++) {
			if(*(SDRAM_ptr+x+320*y)== 1{
				*(Video_Mem_ptr + (y << 9) + (x-8)) = 0xFFFF;
				
			}
			else{
				*(Video_Mem_ptr + (y << 9) + (x-8)) = 0x0; 
		}
	}
	float comp_in_bytes=(float)comp/8; //need compressed count in bytes
	float ratio = (float)9600/comp_in_bytes; // compression ratio
	comp = 0;
}

void deCOMP(RLE_out){ //Decompression
	char bitVal = (RLE_out & 0x00800000)>>23; //Only keep top 24th bit
	int numOfBits=(RLE_out & 0x007FFFFF); //only keep 23 bottom bits
	int i;
	while(i < numOfBits) {
		*(SDRAM_ptr +sdram_index)=bitVal; //assign bitVal to the sdram equal to quantity # of times
		sdram_index++;
		i++;
	}

}