/*
             LUFA Library
     Copyright (C) Dean Camera, 2009.
              
  dean [at] fourwalledcubicle [dot] com
      www.fourwalledcubicle.com
*/

/*
  Copyright 2009  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Altered for USBVirtualSerial_SPI by Opendous Inc. 2009-09
  For more information visit:  www.Micropendous.org/SPI

  Permission to use, copy, modify, and distribute this software
  and its documentation for any purpose and without fee is hereby
  granted, provided that the above copyright notice appear in all
  copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaim all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/*
	Communications Device Class demonstration application.
	Allows you to use standard serial port software to send and receive
	data over SPI.  This is a USB Serial Port to SPI gateway.

	Changing the "Line Coding"/"bytesize" will change the SPI mode:
		Mode0 = 8 bits, Mode1 = 7 bits, Mode2 = 6 bits, Mode3 = 5 bits
	Note that data is still sent in 8-bit bytes over SPI, this is just a way to
	overlay SPI functionality over UART functionality.  Default is Mode0.

	Changing "Baud-Rate" will change the SPI speed.  Defualt SPI clock speed
	is 8Mhz / 4 = 2MHz.  8Mhz is the device clock speed.  This is the setting at
	9600 baud.  Each step up in baud-rate will _HALVE_ the clock speed:
		9600 = 125kHz, 14400 = 4MHz, 19200 = 2MHz, 38400 = 1MHz,
		57600 = 500kHz, 115200 = 250kHz, other = 62.5kHz

	Changing "Stop-Bits" determines whether data will be MSB or LSB.  If the
	default one stop bit is selected, data will be transmitted MSB first.  Else LSB.

	Before running, you will need to install the INF file that
	is located in the USBtoSerial project directory. This will enable
	Windows to use its inbuilt CDC drivers, negating the need
	for special Windows drivers for the device.
*/


#include "USBVirtualSerial_SPI.h"

/* Globals: */
/** Contains the current baud rate and other settings of the virtual serial port.
 *
 *  These values are set by the host via a class-specific request, and the physical USART should be reconfigured to match the
 *  new settings each time they are changed by the host.
 */
CDC_Line_Coding_t LineEncoding = { .BaudRateBPS = 0,
                                   .CharFormat  = OneStopBit,
                                   .ParityType  = Parity_None,
                                   .DataBits    = 8            };

/** Ring (circular) buffer to hold the RX data - data from the host to the attached device on the serial port. */
RingBuff_t Rx_Buffer;

/** Ring (circular) buffer to hold the TX data - data from the attached device on the serial port to the host. */
RingBuff_t Tx_Buffer;

/** Flag to indicate if the USART is currently transmitting data from the Rx_Buffer circular buffer. */
volatile bool Transmitting = false;

/* some global variables */
volatile uint8_t tempIOreg = 0;
volatile uint8_t firstRun = 1;

/** Main program entry point. This routine configures the hardware required by the application, then
 *  starts the scheduler to run the application tasks.
 */
int main(void)
{
	SetupHardware();

	/* Ring buffer Initialization */
	Buffer_Initialize(&Rx_Buffer);
	Buffer_Initialize(&Tx_Buffer);

	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
	
	for (;;)
	{
		CDC_Task();
		USB_USBTask();
		Main_Task();
	}
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void)
{
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);

	/* disable JTAG to allow corresponding pins to be used - PF4, PF5, PF6, PF7 */
	#if ((defined(__AVR_AT90USB1287__) || defined(__AVR_AT90USB647__) ||  \
			defined(__AVR_AT90USB1286__) || defined(__AVR_AT90USB646__) ||  \
			defined(__AVR_ATmega16U4__)  || defined(__AVR_ATmega32U4__) ||  \
			defined(__AVR_ATmega32U6__)))
		// note the JTD bit must be written twice within 4 clock cycles to disable JTAG
		// you must also set the IVSEL bit at the same time, which requires IVCE to be set first
		// port pull-up resistors are enabled - PUD(Pull Up Disable) = 0
		MCUCR = (1 << JTD) | (1 << IVCE) | (0 << PUD);
		MCUCR = (1 << JTD) | (0 << IVSEL) | (0 << IVCE) | (0 << PUD);
	#endif

	/* Hardware Initialization */
	/* For more information look over the corresponding AVR's datasheet in the
		'I/O Ports' Chapter under subheading 'Ports as General Digital I/O' */
	#if (defined(__AVR_AT90USB162__)  || defined(__AVR_AT90USB82__))
		DDRD = 0;
		PORTD = 0;
		DDRB = 0;
		PORTB = 0;
		DDRC = 0;
		PORTC |= (0 << PC2) | (0 << PC4) | (0 << PC5) | (0 << PC6) | (0 << PC7); //only PC2,4,5,6,7 are pins
		// be careful using PortC as PC0 is used for the Crystal
	#endif

	#if (defined(__AVR_ATmega16U4__)  || defined(__AVR_ATmega32U4__))
		DDRD = 0;
		PORTD = 0;
		DDRB = 0;
		PORTB = 0;
		DDRC = 0;
		PORTC |= (0 << PC6) | (0 << PC7); //only PC6,7 are pins
		DDRE = 0;
		PORTE |= (0 << PE2) | (0 << PE6); //only PE2,6 are pins
		DDRF = 0;
		PORTF |= (0 << PF0) | (0 << PF1) | (0 << PF4) | (0 << PF5) | (0 << PF6) | (0 << PF7); // only PF0,1,4,5,6,7 are pins
	#endif

	#if (defined(__AVR_AT90USB1287__) || defined(__AVR_AT90USB647__) ||  \
			defined(__AVR_AT90USB1286__) || defined(__AVR_AT90USB646__) ||  \
			defined(__AVR_ATmega32U6__))
		DDRA = 0;
		PORTA = 0;
		DDRB = 0;
		PORTB = 0;
		DDRC = 0;
		PORTC = 0;
		DDRD = 0;
		PORTD = 0;
		DDRE = (1 << PE6);	// set PE6 to HIGH to disable external SRAM, if connected
		PORTE = (1 << PE6);	// set PE6 to HIGH to disable external SRAM, if connected
		DDRF = 0;
		PORTF = 0;
	#endif

	/* Initialize stdout and stdin for printf and scanf */
	fdevopen(sendData, getData);

	/* Hardware Initialization */
	// Prepare PortB for SPI - set PB0(^SS), PB1(SCK), PB2(MOSI) as output as well as all other pins except PB3(MISO)
	DDRB = (1 << PB0) | (1 << PB1) | (1 << PB2) | (0 << PB3) | (1 << PB4) | (1 << PB5) | (1 << PB6) | (1 << PB7);
	PORTB |= (1 << PB0); // make sure the ^SS select pin is high (disabled)

	LEDs_Init();
	USB_Init();
}

/** Event handler for the USB_Connect event. This indicates that the device is enumerating via the status LEDs and
 *  starts the library USB task to begin the enumeration and USB management process.
 */
void EVENT_USB_Device_Connect(void)
{
	/* Indicate USB enumerating */
	LEDs_SetAllLEDs(LEDMASK_USB_ENUMERATING);
}

/** Event handler for the USB_Disconnect event. This indicates that the device is no longer connected to a host via
 *  the status LEDs and stops the USB management and CDC management tasks.
 */
void EVENT_USB_Device_Disconnect(void)
{	
	/* Reset Tx and Rx buffers, device disconnected */
	Buffer_Initialize(&Rx_Buffer);
	Buffer_Initialize(&Tx_Buffer);

	/* Indicate USB not ready */
	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
}

/** Event handler for the USB_ConfigurationChanged event. This is fired when the host set the current configuration
 *  of the USB device after enumeration - the device endpoints are configured and the CDC management task started.
 */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	/* Indicate USB connected and ready */
	LEDs_SetAllLEDs(LEDMASK_USB_READY);

	/* Setup CDC Notification, Rx and Tx Endpoints */
	if (!(Endpoint_ConfigureEndpoint(CDC_NOTIFICATION_EPNUM, EP_TYPE_INTERRUPT,
		                             ENDPOINT_DIR_IN, CDC_NOTIFICATION_EPSIZE,
	                                 ENDPOINT_BANK_SINGLE)))
	{
		LEDs_SetAllLEDs(LEDMASK_USB_ERROR);
	}
	
	if (!(Endpoint_ConfigureEndpoint(CDC_TX_EPNUM, EP_TYPE_BULK,
		                             ENDPOINT_DIR_IN, CDC_TXRX_EPSIZE,
	                                 ENDPOINT_BANK_SINGLE)))
	{
		LEDs_SetAllLEDs(LEDMASK_USB_ERROR);
	}							   

	if (!(Endpoint_ConfigureEndpoint(CDC_RX_EPNUM, EP_TYPE_BULK,
		                             ENDPOINT_DIR_OUT, CDC_TXRX_EPSIZE,
	                                 ENDPOINT_BANK_SINGLE)))
	{
		LEDs_SetAllLEDs(LEDMASK_USB_ERROR);
	}

	/* Reset line encoding baud rate so that the host knows to send new values */
	LineEncoding.BaudRateBPS = 0;
}

/** Event handler for the USB_UnhandledControlRequest event. This is used to catch standard and class specific
 *  control requests that are not handled internally by the USB library (including the CDC control commands,
 *  which are all issued via the control endpoint), so that they can be handled appropriately for the application.
 */
void EVENT_USB_Device_UnhandledControlRequest(void)
{
	/* Process CDC specific control requests */
	switch (USB_ControlRequest.bRequest)
	{
		case REQ_GetLineEncoding:
			if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
			{	
				/* Acknowledge the SETUP packet, ready for data transfer */
				Endpoint_ClearSETUP();

				/* Write the line coding data to the control endpoint */
				Endpoint_Write_Control_Stream_LE(&LineEncoding, sizeof(LineEncoding));
				
				/* Finalize the stream transfer to send the last packet or clear the host abort */
				Endpoint_ClearOUT();
			}
			
			break;
		case REQ_SetLineEncoding:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				/* Acknowledge the SETUP packet, ready for data transfer */
				Endpoint_ClearSETUP();

				/* Read the line coding data in from the host into the global struct */
				Endpoint_Read_Control_Stream_LE(&LineEncoding, sizeof(LineEncoding));

				/* Finalize the stream transfer to clear the last packet from the host */
				Endpoint_ClearIN();
				
				/* Reconfigure the USART with the new settings */
				Reconfigure();
			}
	
			break;
		case REQ_SetControlLineState:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
			{				
				/* Acknowledge the SETUP packet, ready for data transfer */
				Endpoint_ClearSETUP();
				
				/* NOTE: Here you can read in the line state mask from the host, to get the current state of the output handshake
				         lines. The mask is read in from the wValue parameter in USB_ControlRequest, and can be masked against the
						 CONTROL_LINE_OUT_* masks to determine the RTS and DTR line states using the following code:
				*/

				Endpoint_ClearStatusStage();
			}
	
			break;
	}
}

/** Task to manage CDC data transmission and reception to and from the host, from and to the physical USART. */
void CDC_Task(void)
{
	/* Device must be connected and configured for the task to run */
	if (USB_DeviceState != DEVICE_STATE_Configured)
	  return;
	  
#if 0
	/* NOTE: Here you can use the notification endpoint to send back line state changes to the host, for the special RS-232
			 handshake signal lines (and some error states), via the CONTROL_LINE_IN_* masks and the following code:
	*/

	USB_Notification_Header_t Notification = (USB_Notification_Header_t)
		{
			.NotificationType = (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE),
			.Notification     = NOTIF_SerialState,
			.wValue           = 0,
			.wIndex           = 0,
			.wLength          = sizeof(uint16_t),
		};
		
	uint16_t LineStateMask;
	
	// Set LineStateMask here to a mask of CONTROL_LINE_IN_* masks to set the input handshake line states to send to the host
	
	Endpoint_SelectEndpoint(CDC_NOTIFICATION_EPNUM);
	Endpoint_Write_Stream_LE(&Notification, sizeof(Notification));
	Endpoint_Write_Stream_LE(&LineStateMask, sizeof(LineStateMask));
	Endpoint_ClearIN();
#endif

	/* Select the Serial Rx Endpoint */
	Endpoint_SelectEndpoint(CDC_RX_EPNUM);
	
	/* Check to see if a packet has been received from the host */
	if (Endpoint_IsOUTReceived())
	{
		/* Read the bytes in from the endpoint into the buffer while space is available */
		while (Endpoint_BytesInEndpoint() && (Rx_Buffer.Elements != BUFF_STATICSIZE))
		{
			/* Store each character from the endpoint */
			Buffer_StoreElement(&Rx_Buffer, Endpoint_Read_Byte());
		}
		
		/* Check to see if all bytes in the current packet have been read */
		if (!(Endpoint_BytesInEndpoint()))
		{
			/* Clear the endpoint buffer */
			Endpoint_ClearOUT();
		}
	}

	/* Check if Rx buffer contains data - if so, send it */
	if (Rx_Buffer.Elements) {
		/* Serial_TxByte(Buffer_GetElement(&Rx_Buffer)); */
		/* Do not want to stream data over UART, want to leave it in
			the buffer for Main_Task to process */
		/* Note this is the only alteration to CDC_Task from the original USBtoSerial */
	}

	/* Select the Serial Tx Endpoint */
	Endpoint_SelectEndpoint(CDC_TX_EPNUM);

	/* Check if the Tx buffer contains anything to be sent to the host */
	if ((Tx_Buffer.Elements) && LineEncoding.BaudRateBPS)
	{
		/* Wait until Serial Tx Endpoint Ready for Read/Write */
		Endpoint_WaitUntilReady();
		
		/* Write the bytes from the buffer to the endpoint while space is available */
		while (Tx_Buffer.Elements && Endpoint_IsReadWriteAllowed())
		{
			/* Write each byte retreived from the buffer to the endpoint */
			Endpoint_Write_Byte(Buffer_GetElement(&Tx_Buffer));
		}
		
		/* Remember if the packet to send completely fills the endpoint */
		bool IsFull = (Endpoint_BytesInEndpoint() == CDC_TXRX_EPSIZE);
		
		/* Send the data */
		Endpoint_ClearIN();

		/* If no more data to send and the last packet filled the endpoint, send an empty packet to release
		 * the buffer on the receiver (otherwise all data will be cached until a non-full packet is received) */
		if (IsFull && !(Tx_Buffer.Elements))
		{
			/* Wait until Serial Tx Endpoint Ready for Read/Write */
			Endpoint_WaitUntilReady();
				
			/* Send an empty packet to terminate the transfer */
			Endpoint_ClearIN();
		}
	}
}

/** Reconfigures firmware based on current serial port settings issued by the host. */
void Reconfigure(void)
{
	uint8_t SPCRmask = (1 << SPE) | (1 << MSTR); // always enable SPI as Master
	uint8_t SPSRmask = 0;

	/* Determine parity - non odd/even parity mode defaults to no parity */
	if (LineEncoding.ParityType == Parity_Odd) {
		//
	} else if (LineEncoding.ParityType == Parity_Even) {
		//
	} else if (LineEncoding.ParityType == Parity_Mark) {
		// 
	} else if (LineEncoding.ParityType == Parity_Space) {
		//
	} else {  //Parity_None
		//
	}

	/* Determine stop bits - 1.5 stop bits is set as 1 stop bit due to hardware limitations */
	if (LineEncoding.CharFormat == TwoStopBits) {
		SPCRmask |= (1 << DORD);
	} else if (LineEncoding.CharFormat == OneAndAHalfStopBits) {
		//
	} else {  // OneStopBit
		SPCRmask |= (0 << DORD);
	}
	
	/* Determine data size - 5, 6, 7, or 8 bits are supported */
	/* Changing line coding changes SPI Mode
		CPOL=0, CPHA=0 Sample (Rising) Setup (Falling) SPI-Mode0 == 8 bits line coding
		CPOL=0, CPHA=1 Setup (Rising) Sample (Falling) SPI-Mode1 == 7 bits line coding
		CPOL=1, CPHA=0 Sample (Falling) Setup (Rising) SPI-Mode2 == 6 bits line coding
		CPOL=1, CPHA=1 Setup (Falling) Sample (Rising) SPI-Mode3 == 5 bits line coding
	*/
	if (LineEncoding.DataBits == 8) {
		SPCRmask |= ((0 << CPOL) | (0 << CPHA));
	} else if (LineEncoding.DataBits == 7) {
		SPCRmask |= ((0 << CPOL) | (1 << CPHA));
	} else if (LineEncoding.DataBits == 6) {
		SPCRmask |= ((1 << CPOL) | (0 << CPHA));
	} else {  // LineEncoding.DataBits == 5
		SPCRmask |= ((1 << CPOL) | (1 << CPHA));
	}

	/* What BaudRate does the host want to use */
	/* SPI data rate is determined by baud rate setting
		Note Fosc is the system clock, assumed to be 8MHz
	*/
	if        (LineEncoding.BaudRateBPS == 9600) { // 125kHz SPI (Fosc / 64)
		SPCRmask |= ((1 << SPR1) | (0 << SPR0));
		SPSRmask |= (0 << SPI2X);
	} else if (LineEncoding.BaudRateBPS == 14400) { // 4MHz SPI (Fosc / 2)
		SPCRmask |= ((0 << SPR1) | (0 << SPR0));
		SPSRmask |= (1 << SPI2X);
	} else if (LineEncoding.BaudRateBPS == 19200) { // 2MHz SPI (Fosc / 4)
		SPCRmask |= ((0 << SPR1) | (0 << SPR0));
		SPSRmask |= (0 << SPI2X);
	} else if (LineEncoding.BaudRateBPS == 38400) { // 1MHz SPI (Fosc / 8)
		SPCRmask |= ((0 << SPR1) | (1 << SPR0));
		SPSRmask |= (1 << SPI2X);
	} else if (LineEncoding.BaudRateBPS == 57600) { // 500kHz SPI (Fosc / 16)
		SPCRmask |= ((0 << SPR1) | (1 << SPR0));
		SPSRmask |= (0 << SPI2X);
	} else if (LineEncoding.BaudRateBPS == 115200) { // 250kHz SPI (Fosc / 32)
		SPCRmask |= ((1 << SPR1) | (0 << SPR0));
		SPSRmask |= (1 << SPI2X);
	} else { // 62.5kHz SPI (Fosc / 128)
		SPCRmask |= ((1 << SPR1) | (1 << SPR0));
		SPSRmask |= (0 << SPI2X);
	}

	SPCR = SPCRmask;
	SPSR = SPSRmask;

	// only read if first run
	if (firstRun) {
		tempIOreg = SPSR; //need to read to initiliaze
		tempIOreg = SPDR; //need to read to initiliaze
		firstRun = 0;
	}
}

/* Use this function to make sure data exists.
	Need this function as RingBuff will crash if getData is called on
	an empty buffer
*/
uint8_t haveData(void) {
	return (uint8_t)(Rx_Buffer.Elements);
}

/* In order to use printf functions, need a function that will send data over
	the USB Virtual Serial Port link
	return 0 on success, else failure and ensure binary compatibility
*/
static int sendData(char c, FILE *stream) {
	// most terminals require \r\n
	// however, do not include this conversion as it will break binary transfers 
	//if (c == '\n') {
	//	sendData('\r', stream);
	//}
	Buffer_StoreElement(&Tx_Buffer, (uint8_t)c);
	return 0;
}

/* Also require a function to receive data from the USB Virtual Serial Port link */
int getData(FILE *__stream) {
	// TODO - implement the following errors - but are they really necessary?
	//if (something) return _FDEV_ERR;
	//if (something) return _FDEV_EOF;
	return (int)Buffer_GetElement(&Rx_Buffer);
}



/* Main_Task will run once initialization is complete */
void Main_Task(void)
{
	uint8_t tempByte = 0;	// temporary storage - integer (16-bit)

	// select SS pin of slave SPI device, but only if there is data to send
	if (haveData()) {
		PORTB = (PORTB & ~(1 << PB0));
	}

	while (haveData()) {
		// send/receive data - SPI is synchronous
		tempByte = SPI_TransferByte(getData(stdin));
		// send received data to host
		sendData(tempByte, stdout);
	}

	PORTB |= (1 << PB0); // de-select SS pin of slave SPI device

}