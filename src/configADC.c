/***************************************************************
	Filename:	configADC.c (ADC and Amplification chain Functions)
	Author:		Diogo Aguiam - diogo.aguiam@ist.utl.pt
	Date:		May 2013
	Version:	v1.0
	
	Dependecies:	configADC.h
					
	Purpose:	Set the Gain of the amplification chain.
				Initiate analog to digital conversion and read
			the data.
			
	Usage:	Both ADCs (AD7685) are configurated in a Chained Mode 
		with Busy Indicator (Daisy Chain with interrupt p.23 Rev.C
		of datasheet).
			Amplification chain consists of two 20 dB pre-amplifiers
		in series (AD8421), followed by a -8 dB to +32 dB analog
		variable gain amplifier (VCA821). The analog gain voltage
		of this amplifier is set with the AD5621 DAC, programmed
		through an SPI interface.
				
		Board DAI SRU assignements:
		
			GAIN_!CS	-		DAI_PB13
			GAIN_DATA	-		DAI_PB16
			GAIN_CLK	-		DAI_PB14
			
			ADC_CNV		-		DAI_PB17
			ADC_TRIG	-		DAI_PB19
			ADC_DATA	-		DAI_PB18
			ADC_CLK		-		DAI_PB20
	
	
	Extra:		
		GAIN DAC	-	SPORT4A	 	-	Gain setting voltage
		



***************************************************************/


#include "../h/configADC.h"

/**************************************************************
			EXTERNAL ADC GLOBAL VARIABLES
***************************************************************/



/**************************************************************
			LOCAL ADC GLOBAL VARIABLES
***************************************************************/
/*
union Samples {
	unsigned int Int[MAXSAMPLES];
	unsigned char Char[MAXSAMPLES*4];
} SAMPLES_MEMORY;
*/
unsigned int * SAMPLES_MEMORY;

unsigned int sample_buffer_1[MAXSAMPLES];
unsigned int sample_buffer_2[MAXSAMPLES];

unsigned int samples_memory_index;	// Current index in the samples memory

unsigned int adc_number_of_samples;	// Total number of samples in acquisition run




/************************************************************
	Function:		InitGAIN_IO (void)
	Argument:	
	Description:	Initializes Gain DAC IO pins
	Action:		All	pins are set as Outputs and default HIGH.
				Connect the SPORT2 to communicate with the 
				GAIN DAC.
				
					GAIN_CS -> Frame Sync -> DAI_12
					GAIN_DATA -> SPORT DA -> DAI_17
					GAIN_CLK -> SPORT CLK	-> DAI_16
************************************************************/
void InitGAIN_IO(void){	
    // SPORTx does not provide a gated clock. An internal
    // buffer must be used to supply the GAIN_CLK output.
    // The enable of this buffer corresponds to the 
    // SPORTx frame sync output. This frame sync is
    // configured to be high only during transmission.
    
    // SPORT4 frame sync used to enable the clock output
    // and as chip select
    // Must invert Chip select through MISCA5
    SRU(SPORT2_FS_O, MISCA4_I);
    SRU(SPORT2_FS_O, MISCA3_I);
    //SRU(HIGH, INV_MISCA4_I);
    SRU(LOW, DAI_PB12_I);
    
    // MISC buffer 3 implements a gated clock that depends on frame sync.
    // The clock output enable is supplied by this buffer.
    SRU(MISCA3_O, PBEN12_I);
    SRU(MISCA3_O, PBEN16_I);
	    
    // SPORT4 Data channel A
    SRU(SPORT2_DA_O, DAI_PB17_I);
    
    //SRU(SPORT2_FS_O, DAI_PB12_I);
    SRU(SPORT2_CLK_O, DAI_PB16_I);


//Enabling pins as Outputs. High -> Output, Low -> Input
	// GAIN DAC Chip Select SPORT2_DA_O SPORT2_FS_O
	//SRU(HIGH,PBEN12_I);	
	// GAIN DAC Data
	SRU(HIGH,PBEN17_I);
	// GAIN DAC Clock
	//SRU(HIGH,PBEN16_I);	
	
}





/**********************************************************
	Function:		GAIN_set_Voltage(void)
	Argument:	gain_value - Voltage Value to bet set
				power_down_mode - Which power down mode
	Description:	Masks configuration word and
			starts SPORT2 transfer to GAIN DAC.
	Action:		
			Configures the DAC configuration word:
			[ 2bit power down | 12 bit value | 2 bit X]
			
			SPORT modes: Standard Serial
			Data Transfer type: Standard
			
	Assumptions: Value to be transfered is the gain_value,
		
		
************************************************************/
void GAIN_set_voltage(int gain_value, char power_down_mode)
{
	int GAIN_config_word=0;
	// Masking of the configuration word
	
	GAIN_config_word = (power_down_mode & 0x03)<<14 | (gain_value & 0x0FFF)<<2;
	
	// Waits for free buffer #! Should have another implementation
    while (*pSPCTL2 & DXS1_A);
	// Sets SPORT transmit buffer
	*pTXSP2A = GAIN_config_word; //<- data trasnmist buffer
    
    
}



/**********************************************************
	Function:		GAIN_init()
	Argument:	
	Description:	Configures SPORT2 to be used for DAC
	Action:	Configures the SPORT2 as standard serial and 
		configures the DAC with the default GAIN word.
			
			SPORT modes: Standard Serial
			Data Transfer type: Standard
			
		
		
************************************************************/
void GAIN_init(void)
{	
    //Clear SPORT4 configuration register
    *pSPCTL2 = 0 ;

	    // Clock and frame sync divisor. According to DDS timings.
    *pDIV2 = GAIN_SPORT_CLK_DIV;
    // Configure and enable SPORT 4.
    // #! this config could be set during initialization and new words are added
    // to the transmit buffer
    *pSPCTL2 = (SPTRAN | FSR | LAFS |  IFS  | ICLK  | SLEN16 | SPEN_A );
    
    // Transmit mode
    // Frame Sync Required, Late FS and Internal FS
    // Internal Clock
    // Falling Edge sampling 
    // 16 bit configuration
    // SPORT enable and 

    	// Waits for free buffer #! Should have another implementation
    while (*pSPCTL2 & DXS1_A);

	*pTXSP2A = (GAIN_PD_ON & 0x03)<<14 | (GAIN_default & 0x0FFF)<<2;; //<- data trasnmist buffer
    
    
}




/************************************************************
	Function:		InitADC_IO (void)
	Argument:	
	Description:	Initializes ADC IO pins
	Action:		
				Connect the SPORT3 to communicate with the ADCs.
				
				ADC_CNV		-		DAI_PB18	Output
				ADC_TRIG	-		DAI_PB04	Input
			NC	ADC_DATA	-		DAI_PB18	Input
				ADC_CLK		-		DAI_PB14	Output
************************************************************/
void InitADC_IO(void){	

	
	// ADC_CNV is configured as an output by PWM?
    SRU(HIGH, DAI_PB18_I);
    // ADC_CNV is generated by the PCG Frame Sync
   	SRU (PCG_FSD_O, DAI_PB18_I);		

    // Data received from ADC_DATA goes to SPORT3 DA_I
    //SRU(DAI_PB18_O, SPORT4_DA_I);
    // When using SPORTx as a Receive Master, its internal
    // clock must be fed into its input.
    SRU(SPORT3_CLK_O, SPORT3_CLK_I);
    SRU(SPORT4_FS_O, SPORT4_FS_I);
    
    //#! Frame sync externo.. para apagar
    
   // SRU(SPORT3_FS_O, DAI_PB17_I);
    
    
    
    SRU(SPORT3_CLK_O, DAI_PB14_I);
	// The ADC_TRIG serves as External Frame Sync for the SPORT
    // #!!
//	SRU(DAI_PB19_O, SPORT4_FS_I);
    SRU(DAI_PB04_O, SPORT3_DA_I);
	SRU (DAI_PB04_O, DAI_INT_22_I); 

//Enabling pins as Outputs. High -> Output, Low -> Input
    SRU(LOW, DAI_PB04_I); // just in case, tie it to low

//	SRU(LOW, PBEN18_I);	// DATA
//	SRU(HIGH, DAI_PB18_I); // just in case, tie it to low
	
	SRU(LOW, PBEN04_I);	// DATA
	SRU(HIGH,PBEN18_I);	// CNV
	//SRU(LOW,PBEN14_I);	// CLK
	
    SRU(SPORT3_FS_O, MISCA2_I);
	SRU(MISCA2_O,PBEN14_I);	// CLK


}


/**********************************************************
	Function:		ADC_init(unsigned int sample_period)
	Argument:	sample_period - in microseconds, minimum is 6
		for no sample loss
	Description:	Configures SPORT3 to be used for ADC
	Action:	Configures the SPORT3 as standard serial and
		Receive Master with DMA.
		#!			
		
************************************************************/
void ADC_init(unsigned int sample_period)
{	
    //Clear SPORT3 configuration register
    *pSPCTL3 = 0 ;

    // Configuration the DMA
    // #! should be removed since it is unused.
//   *pIISP3A =  (unsigned int)	SAMPLES_MEMORY;	// Internal DMA memory address
//    *pIMSP3A = sizeof(SAMPLES_MEMORY[0]);		// Address modifier
//    *pCSP3A  = MAXSAMPLES; 				// word count 5 bytes 
    
    
	    // Clock and frame sync divisor. According to DDS timings.
    *pDIV3 = ADC_SPORT_CLK_DIV;
	
    // #! Unnecessary    
//	SRU (HIGH, DPI_PBEN06_I);
//	SRU (TIMER0_O, DPI_PB06_I);


/*#! CHANGED TO USE PCG INSTEAD OF TIMER

	SRU(TIMER0_O, TIMER0_I);

	// Configure Timer0 and its interrupt
    *pTM0CTL = (TIMODEPWM | PULSE | PRDCNT | IRQEN);
    *pTM0PRD = sample_period * TICKS_PER_uSEC;
    *pTM0W = (sample_period * TICKS_PER_uSEC-3); // 10% pulse
	*pTM0STAT = TIM0EN;

	// Allow for an interrupt on the 

	// Interrupt SRU SRU_EXTMISCB0_INT
	SRU (DAI_PB04_O, DAI_INT_22_I); 
    *pDAI_IRPTL_PRI |= SRU_EXTMISCB0_INT;
    *pDAI_IRPTL_RE |= SRU_EXTMISCB0_INT;
*/    
    // Interrupt Dispatchers
   	interrupts(SIG_P0,IRQ_ADC_SampleReady);
    interruptf(SIG_SP3,IRQ_ADC_SampleDone);

	DDS_init();
	DDS_init();
	DDS_WriteData(DDS1_frequency, DDS1_phase, 0, DDS_ch1);
	DDS_WriteData(DDS2_frequency, DDS2_phase, 0, DDS_ch2);
	DDS_WriteData(DDS3_frequency, DDS3_phase, 0, DDS_ch3);
		
	// updates the internal DDS lut increment for the specified frequency
	iDDS_lut_inc = (DDS_inc_Fex - DDS_inc_Flo)*1200;
	// Resets the internal DDS lut accumulator
	iDDS_lut_acc = 0;
	
	
	
//	DDS_update_frequency();	
//	*pPCG_SYNC2 = CLKD_SOURCE_IOP;//|FSD_SOURCE_IOP|CLKD_SYNC;

//	*pPCG_PW2 = ((sample_period*PCG_TICKS_PER_uSEC)-1)<<16;

	//*pPCG_SYNC1 = 0;	
	*pPCG_CTLD1 = PCG_CLKD_DIVIDER; 
//	*pPCG_CTLD0 =  (sample_period*PCG_TICKS_PER_uSEC) | ENFSD | ENCLKD ;
//	*pPCG_PW2 = ((sample_period*PCG_TICKS_PER_uSEC)-1)<<16;
	
	DDS_update_frequency();	
	*pPCG_CTLD0 =  250 | ENFSD | ENCLKD ;
//	*pPCG_CTLD0 =  (1*PCG_TICKS_PER_uSEC) | ENFSD | ENCLKD ;

//printf("ticks: %d\n",sample_period*	PCG_TICKS_PER_uSEC);		

//#!	interrupts(SIG_GPTMR0, IRQ_ADC_AssertConversion);
    
    // Configure and enable SPORT 3.
    // #! this config could be set during initialization and new words are added
    // to the transmit buffer
    
    //####*pSPCTL4 = (FSR | ICLK | CKRE | SLEN32 | SPEN_A );
    
    
    
    // Receive Master mode
    // Frame Sync Required, Early FS and External FS
    // Internal Clock
    // Rising Edge sampling 
    // 2*16 = 32 bit configuration
    // SPORT enable  

    	// Waits for free buffer #! Should have another implementation
    //while (*pSPCTL3 & DXS1_A);

	//*pTXSP2A = (GAIN_PD_ON & 0x03)<<14 | (GAIN_default & 0x0FFF)<<2;; //<- data trasnmist buffer
    
    
}


/************************************************************
	Function:		ADC_StopSampling()
	Argument:		
	Description:	Disables Timer0 and consequently stops the
		ADC sampling.
	Action:	
			Signals global variable adc_end_of_sampling stating
		end of acquisition run
************************************************************/
void ADC_StopSampling(void)
{
//		if(adc_continuous_sampling){
//			adc_buffer_to_send = SAMPLES_MEMORY;
//			adc_number_of_samples_to_send = adc_number_of_samples;
//			adc_send_continuous_samples = 1;
//			ADC_StartSampling(adc_number_of_samples, CNV_uSEC);
//			
//		}else{
			*pPCG_CTLD0 = 0;
			//#! *pTM0STAT = TIM0DIS;
//			adc_end_of_sampling = 1;
//		}
		
		
		
	
}


/************************************************************
	Function:		ADC_SwapBuffer()
	Argument:		
	Description:	Is triggered when a SAMPLES_MEMORY buffer
		is full and swaps it to the next one.
#!		Also signals a adc_samples_buffer_full to possibly send to USB
		
	Action:	
		
		
************************************************************/
void ADC_SwapBuffer(void)
{

/*	
	AR_bufferIndex=0;
	if(AR_buffer == memSamplesBuffer1){
		AR_buffer = memSamplesBuffer2;
	//	printf("samplebuffer2\n");
	}else{
		AR_buffer = memSamplesBuffer1;
	//	printf("samplebuffer1\n");
	}
*/	
}

/************************************************************
	Function:		ADC_FinishedAR()
	Argument:		
	Description:	Is called when an Acquisition Run finishes.
		
	Action:	
		
		
************************************************************/
void ADC_FinishedAR(void)
{
//	adc_buffer_to_send = (unsigned int*)memProcessedBufferChA;//AR_bufferChA;//memSamplesBuffer1;//
	adc_number_of_samples_to_send = AR_bufferIndex;
//	adc_send_continuous_samples = 1;
	
	adc_sample_buffer_full = 1;
//#!	adc_send_continuous_samples = 1;
	
	
	if(AR_continuousSampling){
//		ADC_SwapBuffer();	
		
//		SIG_LED1_ON;
		//Init_FIR(AR_bufferIndex);	
		ADC_StopSampling();
	}else{
		ADC_StopSampling();
		
//		SIG_LED1_ON;
//		Init_FIR(AR_bufferIndex);	

//		Init_IIR(AR_bufferIndex);	

	//	Init_FIR();	
	//	Init_FIR();	
		AR_finishedFlag = TRUE;

	}
	AR_finishedFlag = TRUE;

	
}


/************************************************************
	Function:		ADC_StartSampling(int number_samples, int sample_period, char continuous_sampling)
	Argument:		number_samples
					sample_period
					continuous_sampling boolean
	Description:	Configures a Min 5us or programmable 
		period clock to generate the CNV trigger to start each
		conversion. The minimum period is 5us. After a 
		specific number_samples it stops the generation of this
		CNV trigger.
	Action:	
			Updates global variable adc_number_of_samples
		with total number of samples in this acquisition.
************************************************************/
void ADC_StartSampling(unsigned int number_samples, unsigned int sample_period, char continuous_sampling)
{
	AR_bufferIndex=0;
	AR_totalSamples = number_samples;
	
	if(OpMode == MODE_IF){
		//Init_FIR_BPsoft();
		Init_IIR_BPsoft();
//		AR_totalSamples +=50;
	}
	
	AR_continuousSampling = continuous_sampling;
	
//	Init_IIR_soft();
	
//	printf("StartSampling!\n");
	ADC_init(sample_period);
	
	
	
	
}

/************************************************************
	Function:		IRQ_ADC_AssertConversion(int sig_int)
	Argument:		sig_int
	Description:	 This interrupt occurs by command of the 
		Timer0 Interrupt. It asserts the ADC_CNV line to 
		start a new conversion.
		
			
************************************************************/
void IRQ_ADC_AssertConversion(int sigint)
{
	int i;
	*pTMSTAT &= TIM0IRQ;
	ADC_CNV_L;
	for(i=0;i<10;i++);
	ADC_CNV_H;
	//	IRQ_ADC_SampleReady(0);

}



/************************************************************
	Function:		IRQ_ADC_SampleReady(int sig_int)
	Argument:		sig_int
	Description:	This interrupt occurs when the ADC has 
		finished converting a sample and is ready to send its
		bits. This is triggered by the DAI Pin connected to
		the data channel signals the Busy signal of the AD7685
	Action:	Starts the SPORT4 option on the falling edge 
		sampling.	
			
************************************************************/
void IRQ_ADC_SampleReady(int sig_int)
{
		int temp;
	// Read the latch register
	// Otherwise interrupts will be continuously asserted
	temp = *pDAI_IRPTL_H ;
//	*pSPCTL4 = (FSR | ICLK | CKRE | SLEN32 | SPEN_A | 0 );
	
    //(*pDAI_IRPTL_RE) = (SRU_EXTMISCA1_INT  | SRU_EXTMISCA2_INT);    //make sure interrupts latch on the rising edge

	
	//printf("dai interrupt\n");
//	for(i=0;i<10000;i++);	
    //SRU(SPORT4_CLK_O, DAI_PB20_I); //#!
    
    //SRU(SPORT3_FS_O, DAI_PB17_I);
	// Starts the SPORT interface
	// #!! IFS, not fsr, not ckre
	adc_sample_irq =1;

   	*pSPCTL3 = (0 | 0 | IFS | ICLK | 0 | SLEN32 | SPEN_A | 0 );
	/*
		Frame Sync Required (and gates the clock
		Internal Frame Sync, Early Mode to bypass the first bit
		SLEN32
	
	*/
//	SRU(HIGH, DAI_PB19_I); 
//	SRU(HIGH, PBEN19_I);	// TRIG
}




/************************************************************
	Function:		IRQ_ADC_SampleDone(int sig_int)
	Argument:		sig_int
	Description:	End of SPORT sample reception.
	Action:	After receiving a full sample, this interrupt
		should stop the SPORT interface and save the sample
		in memory.
			
************************************************************/
void IRQ_ADC_SampleDone(int sig_int)
{
	//*pSPCTL4 = 0;
	unsigned int k,i,sample;
	 float a1,a2,a3;
	int a,b;
	//for(i=0; i<4;i++);
	
	// Waits for sample in the SPORT buffer
	while ((*pSPCTL3 & DXS1_A)==0);
	// Reads sample from SPORT buffer
	sample = *pRXSP3A;

	// Disables the SPORT interface.
	*pSPCTL3 = 0;


	
	// Saves to current Acquisition Run samples buffer memory
//	AR_buffer[AR_bufferIndex%MAX_SAMPLES_BUFFER_SIZE] = sample;
//	a = (sample&0xffff);
//	b = ((sample>>16)&0xffff);
//	printf("cha A: %d, chB, %d\n",a,b);

	
	//#! Changed for iDDS run time demodulation
	AR_bufferChA[AR_bufferIndex%MAX_SAMPLES_BUFFER_SIZE] = (((int)(sample>>16)&0xffff)-CAL_CHB_DECIMAL)*2.5/65536;// - CAL_chB_calibration;

	// In IF Mode only Channel A is needed.
	// In IQ Mode both ADC channels are used.
	if(OpMode == MODE_IF){
		signal_QuadratureDemodulation_InternalLO_PtbyPt(AR_bufferChA,AR_bufferChB,AR_bufferIndex);
//		signalIIR_bandpassfilter(&AR_bufferChA[AR_bufferIndex%(MAX_SAMPLES_BUFFER_SIZE)],&AR_bufferChB[AR_bufferIndex%MAX_SAMPLES_BUFFER_SIZE]);
	}else{
		AR_bufferChB[AR_bufferIndex%(MAX_SAMPLES_BUFFER_SIZE)] = (((int)sample&0xffff)-CAL_CHA_DECIMAL)*2.5/65536;// - CAL_chA_calibration;
		
	}



	
	
	// If the expected number of samples has been reached.
	if(AR_bufferIndex==AR_totalSamples){
	//	ADC_StopSampling();
	
	//	interrupt(SIG_P0,IRQ_FIR);

	
	
		ADC_FinishedAR();
		
	}else{
		if(AR_continuousSampling==FALSE){
				
			AR_bufferIndex++;	

		}
	}

	
}


