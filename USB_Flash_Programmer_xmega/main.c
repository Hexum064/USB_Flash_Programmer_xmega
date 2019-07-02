/*
 * DIYReflowOvenV1.c
 *
 * Created: 2019-07-02
 * Author : Branden
 */ 



#define  F_CPU 32000000UL

#include <asf.h>
#include <avr/io.h>
#include <stdio.h>
#include <avr/delay.h>
#include <string.h>

#define CS_ENABLE() (PORTC.OUTCLR = PIN4_bm)
#define CS_DISABLE() (PORTC.OUTSET = PIN4_bm)

#define MEM_WRITE_STATUS 0x01
#define MEM_WRITE 0x02
#define MEM_READ 0x03
#define MEM_READ_STATUS 0x05
#define MEM_WRITE_EN 0x06
#define MEM_FAST_READ 0x0B
#define MEM_SECT_ERASE 0x20
#define MEM_CHIP_ERASE 0xC7
#define MEM_128K_ERASE 0xD2
#define MEM_READ_ID 0x9F

#define MEM_STAT_WEN 0x02
#define MEM_STAT_BUSY 0x01

#define RX_WRITE_TEXT 0x10
#define RX_WRITE_DATA 0x20
#define RX_ERASE_ALL 0x30
#define RX_READ_ID 0x01
#define RX_READ_TEXT 0x11
#define RX_READ_DATA 0x21

#define MEM_BLOCK_SIZE 256
#define MEM_SECTOR_SIZE 4096

void uart_putchar(uint8_t c, FILE * stream);
FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);
volatile uint8_t _runTest = 0x00;
volatile uint8_t _currentCommand = 0x00;
volatile uint8_t _rxData[MEM_BLOCK_SIZE];
volatile uint8_t _rxIndex = 0;
volatile uint32_t _rxDataLen = 0;
volatile uint32_t _rxDataPos = 0;
volatile uint8_t _txEmpty;

void initSPI()
{	
	sysclk_enable_module(SYSCLK_PORT_C, SYSCLK_SPI);
	PORTC.DIRSET = PIN4_bm | PIN5_bm | PIN7_bm;
	PORTC.DIRCLR = PIN6_bm;
	SPIC.CTRL =  SPI_CLK2X_bm | SPI_ENABLE_bm | SPI_MASTER_bm | SPI_PRESCALER_DIV16_gc;
}

void initDebugUART()
{
	PORTE.DIRSET = PIN3_bm;
	USARTE0.CTRLB = USART_TXEN_bm;
	USARTE0.CTRLC = USART_CHSIZE_8BIT_gc;
	
}

uint8_t sendSPI(uint8_t data)
{
	SPIC.DATA = data;
	
	while(!(SPIC.STATUS & SPI_IF_bm)) {}
	
	return SPIC.DATA;
	
}

uint8_t sendDummy()
{
	return sendSPI(0x00);
}

void sendString(uint8_t *chr)
{
	while(*chr)
	{
		sendSPI(*(chr++));
	}
	
}

void memSendAddress(uint32_t address)
{
	sendSPI((uint8_t)((address >> 16) & 0xFF));
	sendSPI((uint8_t)((address >> 8) & 0xFF));
	sendSPI((uint8_t)((address >> 0) & 0xFF));
}

uint8_t getMemStatus()
{
	uint8_t out;
	CS_DISABLE();
	CS_ENABLE();
	
	sendSPI(MEM_READ_STATUS);
	out = sendDummy();	
	CS_DISABLE();
	return out;
}

void waitForNotBusy()
{
	while((getMemStatus() & MEM_STAT_BUSY)) {}	
}

void memEnableWrite()
{
	//printf("Enabling Write\r\n");
	CS_DISABLE();
	CS_ENABLE();
	
	sendSPI(MEM_WRITE_EN);
	
	while(!(getMemStatus() & MEM_STAT_WEN)) {}
		
	CS_DISABLE();
	return;
}

void memErase128kBlock(uint32_t address)
{
	waitForNotBusy();
			
	memEnableWrite();
	
	
	
	CS_DISABLE();
	CS_ENABLE();
	
	sendSPI(MEM_128K_ERASE);
	memSendAddress(address);
	
	CS_DISABLE();
	return;	
}

void memEraseSector(uint32_t address)
{
	waitForNotBusy();
	
	memEnableWrite();
	
	
	
	CS_DISABLE();
	CS_ENABLE();
	
	sendSPI(MEM_SECT_ERASE);
	memSendAddress(address);
	
	CS_DISABLE();
	return;
}

void memRead256ToStdOut(uint32_t address)
{
	uint8_t data[16];
	waitForNotBusy();
		
	CS_DISABLE();
	CS_ENABLE();
	sendSPI(MEM_READ);
	memSendAddress(address);;
		
	for(uint8_t i = 0; i < 32; i++)
	{
		for (uint8_t j = 0; j < 16; j++)
		{
			data[j] = sendDummy();
			printf("%02x ", data[j]);
		}
		
		printf("\t");
			
		for (uint8_t j = 0; j < 16; j++)
		{
			printf("%c", data[j]);
		}			
		printf("\r\n");
	}
	
	CS_DISABLE();
	return;	
}

void memWrite256(uint32_t address, uint8_t data)
{
	waitForNotBusy();
	
	memEnableWrite();

	CS_DISABLE();
	CS_ENABLE();
	
	sendSPI(MEM_WRITE);
	memSendAddress(address);
	
	for(uint16_t i = 0; i < 256; i++)
	{
		sendSPI(data);
	}
	
	CS_DISABLE();
	return;	
	
}

void memWriteString(uint32_t address, uint8_t *str)
{
	waitForNotBusy();
	
	memEnableWrite();

	CS_DISABLE();
	CS_ENABLE();
	
	sendSPI(MEM_WRITE);
	memSendAddress(address);
	
	while(*str)
	{
		sendSPI(*(str++));
	}
	
	CS_DISABLE();
	return;
	
}

void memReadToBuffer(uint32_t address, void *buff, uint8_t len)
{
	uint8_t *ptr = buff;
	waitForNotBusy();

	CS_DISABLE();
	CS_ENABLE();
	
	sendSPI(MEM_READ);
	memSendAddress(address);
	
	do
	{
		*(ptr++) = sendDummy();
	}while(len--);
	
	CS_DISABLE();
	return;
	
}

void memWriteBuff(uint32_t address, uint8_t *buff, uint8_t len)
{
	waitForNotBusy();
	
	memEnableWrite();

	CS_DISABLE();
	CS_ENABLE();
	
	sendSPI(MEM_WRITE);
	memSendAddress(address);
	
	do
	{
		sendSPI(*(buff++));
	}while(len--);
	
	CS_DISABLE();
	return;
	
}

void TESTWriteMem()
{
	CS_DISABLE();
	CS_ENABLE();
	sendSPI(MEM_WRITE_EN);
	CS_DISABLE();
	CS_ENABLE();
	sendSPI(MEM_WRITE_STATUS);
	sendSPI(0x00);
	CS_DISABLE();
	
	uint32_t address = 0x00001000;
	uint8_t data[17] = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x00};
	printf("Testing Write\r\n");
	printf("Status: 0x%02x\r\n", getMemStatus());
	//printf("First Erase\r\n");
	//memEraseSector(address);

	printf("Read 256 bytes to confirm erase\r\n");
	memRead256ToStdOut(address);
	
	//printf("Write string\r\n");
	//memWriteString(address, "Eat, Sleep,     Rave, Repeat.");
	//
	//printf("Read 256 bytes to confirm write\r\n");
	//memRead256ToStdOut(address);
	//
	//printf("Done. Status: 0x%02x\r\n", getMemStatus());
}

void writeUARTByte(uint8_t data)
{
	while (!_txEmpty) {}
	udi_cdc_putc(data);		
	_txEmpty = 0x00;
}

void writeUARTBuff(void* buff, uint16_t len)
{
	uint8_t *ptr = buff;
	uint16_t remaining = 0;
	
	
	
	do 
	{
		while (!_txEmpty) {}
		udi_cdc_putc (*(ptr++));
		remaining++;
		
		if (remaining == 63)
		{
			
			remaining  = 0;
			_txEmpty = 0x00;
		}
		//remaining = udi_cdc_multi_write_buf(0, buff, len);
		//printf("Remaining: %u\r\n", remaining);
		//writeUARTByte(*(ptr++));		
	} while (len--);
}


void returnChipId()
{
	//printf("Reading Chip Id\r\n");
	waitForNotBusy();
	memEnableWrite();
	CS_DISABLE();
	CS_ENABLE();
	
	sendSPI(MEM_READ_ID);
	udi_cdc_putc(sendDummy());
	udi_cdc_putc(sendDummy());
	udi_cdc_putc(sendDummy());
	
	CS_DISABLE();
	return;
}

void eraseChip()
{
	//printf("Erasing Chip\r\n");
	waitForNotBusy();
	
	CS_DISABLE();
	CS_ENABLE();
	
	sendSPI(MEM_CHIP_ERASE);
	
	waitForNotBusy();
	
	uart_putchar(0xFF, 0x00);
	
	CS_DISABLE();
	return;
	
}

void readText()
{
	uint16_t len;
	
	memReadToBuffer(0x00000000, _rxData, 2);
	_rxDataLen = ((uint32_t)(_rxData[0] << 8)) + (uint32_t)_rxData[1];
	_rxDataPos = 2;
	
	udi_cdc_putc(_rxData[0]);
	udi_cdc_putc(_rxData[1]);
	
	//printf("Reading 0x%02x 0x%02x %lu chars\r\n", _rxData[0], _rxData[1], _rxDataLen);
	
	while(1)
	{
		if (_rxDataLen < MEM_BLOCK_SIZE) //Less than MEM_BLOCK_SIZE because that's the size of our buffer
		{
			if (_rxDataLen > 0)
			{
				
				//printf("Reading ending %u bytes from 0x%08x\r\n",len, _rxDataPos);
				memReadToBuffer(_rxDataPos, _rxData, _rxDataLen);
				udi_cdc_multi_write_buf(0, _rxData, _rxDataLen);
			}
			return;	
		}
		else
		{		
			//printf("Reading 256 bytes from 0x%08x\r\n", _rxDataPos);	
			memReadToBuffer(_rxDataPos, _rxData, 255);
			_rxDataLen -= 256;
			_rxDataPos+=256;
			udi_cdc_multi_write_buf(0, _rxData, 256);			
		}
	}
	
}

void readData()
{
	uint32_t len;
	
	
	//The first 4 bytes are the total length of the Data including the lookup table 
	memReadToBuffer(MEM_SECTOR_SIZE, _rxData, 4);
	_rxDataLen = ((uint32_t)_rxData[0] << 24) + ((uint32_t)_rxData[1] << 16) + ((uint32_t)_rxData[2] << 8) + (uint32_t)_rxData[3];
	_rxDataPos = 4;
	
	udi_cdc_putc(_rxData[0]);
	udi_cdc_putc(_rxData[1]);
	udi_cdc_putc(_rxData[2]);
	udi_cdc_putc(_rxData[3]);
	
	//printf("Reading 0x%02x 0x%02x 0x%02x 0x%02x %lu chars\r\n", _rxData[0], _rxData[1], _rxData[2], _rxData[3], _rxDataLen);
	
	while(1)
	{
		if (_rxDataLen < MEM_BLOCK_SIZE) //Less than MEM_BLOCK_SIZE because that's the size of our buffer
		{
			if (_rxDataLen > 0)
			{
				
				//printf("Reading ending %u bytes from 0x%08x\r\n",len, _rxDataPos);
				memReadToBuffer(_rxDataPos + MEM_SECTOR_SIZE, _rxData, _rxDataLen);
				udi_cdc_multi_write_buf(0, _rxData, _rxDataLen);
			}
			return;	
		}
		else
		{		
			//printf("Reading 256 bytes from 0x%08x\r\n", _rxDataPos);	
			memReadToBuffer(_rxDataPos + MEM_SECTOR_SIZE, _rxData, 255);
			_rxDataLen -= 256;
			_rxDataPos+=256;
			udi_cdc_multi_write_buf(0, _rxData, 256);			
		}
	}
	
}

void uart_putchar(uint8_t c, FILE * stream)
{
	//udi_cdc_putc(c);
	//return;
	
	while(!(USARTE0.STATUS & USART_DREIF_bm)) {}
	
	USARTE0.DATA = c;
}

bool my_callback_cdc_enable(void)
{
	return true;
}

void my_callback_cdc_disable(void)
{

}

void writeText(uint8_t bytesRead, uint8_t *bytes)
{
	uint8_t bytesLeft = 0;
	//printf("write data\r\n");
			
	//This is the way we detect the first read through. Even if the length being sent is 0, we will be stopping here anyways.
	//NOTE: If this is unreliable, we should include a state flag that indicates this is the first time through.
	if (bytesRead > 1 && _rxDataLen == 0)
	{
		_currentCommand = RX_WRITE_TEXT;	//Set the current command
		memEraseSector(0x00);	//The string is written to the very first sector.
		waitForNotBusy();
		_rxDataLen = ((uint32_t)bytes[0] << 8) + (uint32_t)bytes[1]; //Get the length of the text, first time through,  byte 0 and 1 should be the 16bit length
				
				
		//printf("Len: %lu  \r\n", _rxDataLen);


				
	}

	//Here we have to split the array of data read between the current memory block and the next one.
	if ((bytesRead + _rxIndex) >= MEM_BLOCK_SIZE)
	{
		//_rxData + _rxIndex is pointer math
		//This should fill our _rxData buffer (which is the same size as a block) so we should write the block now and update the _rxDataPos
		bytesLeft = MEM_BLOCK_SIZE - _rxIndex;
		memcpy(_rxData + _rxIndex, bytes, bytesLeft);
		_rxIndex += bytesRead; //_rxIndex is 8bit. if _rxIndex = 192, then _rxIndex + 64 would roll over to 0
				
		//printf("Writing 256 bytes to 0x%08lx\r\n", (_rxDataPos & 0xFFFFFF00));
		memWriteBuff((_rxDataPos & 0xFFFFFF00), _rxData, MEM_BLOCK_SIZE - 1);
				
				
		//Because _rxIndex would have rolled over to 0 if we were on a factor of 256, and we are already here because the current amount of bytes read + the current index is greater or equal to our Block size
		if (_rxIndex != 0)
		{
			//We are copying what is left in bytes and bytes + bytesLeft is pointer math
			memcpy(_rxData, bytes + bytesLeft, bytesRead - bytesLeft);
		}
				
	}
	else
	{
		memcpy(_rxData + _rxIndex, bytes, bytesRead);
		_rxIndex += bytesRead;
	}
			
	_rxDataPos += bytesRead;
	//printf("Pos %lu\r\n", _rxDataPos);
			
	//We have the whole message so just write it and be done with the command. Subtract 2 because the len are at the beginning
	if ((_rxDataPos - 2) == _rxDataLen)
	{
		//Write the last bit of data
		memWriteBuff((_rxDataPos & 0xFFFFFF00), _rxData, _rxIndex);
		//printf("Writing %u bytes from index %u to 0x%08lx\r\n",_rxIndex, (_rxDataPos & 0xFFFFFF00));
		//Send ack byte;
		udi_cdc_putc(0xFF);
		//printf("Done writing %lu bytes\r\n", _rxDataPos);
		_currentCommand = 0;
		_rxDataPos = 0;

		//return;
	}
	
	
}

void writeData(uint8_t bytesRead, uint8_t *bytes)
{
	uint8_t bytesLeft = 0;
		
	if (bytesRead > 3 && _rxDataLen == 0)
	{
		_currentCommand = RX_WRITE_DATA;	//Set the current command
				
		waitForNotBusy();
		_rxDataLen = ((uint32_t)bytes[0] << 24) + ((uint32_t)bytes[1] << 16) + ((uint32_t)bytes[2] << 8) + (uint32_t)bytes[3]; //Get the length of the text, first time through,  bytes 0 to 3 should be the 32bit length
		_rxDataPos = 0;
				
		//printf("Len: %lu  0x%02x 0x%02x 0x%02x 0x%02x\r\n", _rxDataLen, bytes[0], bytes[1], bytes[2], bytes[3]);
				
		//Erase the number of sectors we will need to store the data
		do
		{
			//_rxDataPos is being used for our address here to erase sectors
			//We initialize our _rxDataPos this way to start our erase at the second sector.
			_rxDataPos += MEM_SECTOR_SIZE;
			//printf("Erasing 128K 0x%08lx\r\n", _rxDataPos);
			memEraseSector(_rxDataPos);	//The string is written to the very first sector.
			waitForNotBusy();
					
		} while (_rxDataPos < (_rxDataLen + 4)); // + 4 to include room for the length bytes
				
		//Reset _rxDataPos
		_rxDataPos = 0;

				
	}

	//Here we have to split the array of data read between the current memory block and the next one.
	if ((bytesRead + _rxIndex) >= MEM_BLOCK_SIZE)
	{
		//_rxData + _rxIndex is pointer math
		//This should fill our _rxData buffer (which is the same size as a block) so we should write the block now and update the _rxDataPos
		bytesLeft = MEM_BLOCK_SIZE - _rxIndex;
		memcpy(_rxData + _rxIndex, bytes, bytesLeft);
		_rxIndex += bytesRead; //_rxIndex is 8bit. if _rxIndex = 192, then _rxIndex + 64 would roll over to 0
				
		//printf("Writing 256 bytes to 0x%08lx\r\n", (_rxDataPos & 0xFFFFFF00) + MEM_SECTOR_SIZE);
		//We add  MEM_SECTOR_SIZE to our address to offset everything to the second sector
		memWriteBuff((_rxDataPos & 0xFFFFFF00) + MEM_SECTOR_SIZE, _rxData, MEM_BLOCK_SIZE - 1);
				
				
		//Because _rxIndex would have rolled over to 0 if we were on a factor of 256, and we are already here because the current amount of bytes read + the current index is greater or equal to our Block size
		if (_rxIndex != 0)
		{
			//We are copying what is left in bytes and bytes + bytesLeft is pointer math
			memcpy(_rxData, bytes + bytesLeft, bytesRead - bytesLeft);
		}
				
	}
	else
	{
		memcpy(_rxData + _rxIndex, bytes, bytesRead);
		_rxIndex += bytesRead;
	}
			
	_rxDataPos += bytesRead;
	//printf("Pos %lu\r\n", _rxDataPos);
			
	//We have the whole message so just write it and be done with the command. Subtract 4 because the len are at the beginning
	if ((_rxDataPos - 4) == _rxDataLen)
	{
		//Write the last bit of data
		//We add  MEM_SECTOR_SIZE to our address to offset everything to the second sector
		memWriteBuff((_rxDataPos & 0xFFFFFF00) + MEM_SECTOR_SIZE, _rxData, _rxIndex);
		//printf("Writing %u bytes from index %u to 0x%08x\r\n",_rxIndex, (_rxDataPos & 0xFFFFFF00));
		//Send ack byte;
		udi_cdc_putc(0xFF);
		//printf("Done writing %lu bytes\r\n", _rxDataPos);
		_currentCommand = 0;
		_rxDataPos = 0;

		//return;
	}
				
}

void my_callback_rx_notify(uint8_t port)
{
	
	//printf("RX\r\n");
	
	//return;
	uint8_t bytesRead = 0;
	uint8_t bytes[UDI_CDC_COMM_EP_SIZE]; //UDI_CDC_COMM_EP_SIZE is the max size of the number of bytes that will be received at once.
	uint8_t bytesLeft = 0;
	
	if (!_currentCommand)
	{
		udi_cdc_read_no_polling(bytes, 1);
		_currentCommand =  bytes[0];
		_rxDataLen = 0;
		_rxDataPos = 0;	
		_rxIndex = 0;
		//printf("Cmd: 0x%02x\r\n", _currentCommand);
	}
	
	bytesRead = udi_cdc_read_no_polling(bytes, UDI_CDC_COMM_EP_SIZE);
	//printf("RX Received %u bytes\r\n", bytesRead);

	switch(_currentCommand)
	{
		//These are long running and implemented in the main loop instead of here in the IRQ handler
		//case 'a':
		//case RX_READ_ID:
		//case 'b':
		//case RX_READ_TEXT:
		//case RX_READ_DATA:
		//case RX_ERASE_ALL:

		case RX_WRITE_TEXT:
			writeText(bytesRead, bytes);
			break;
		case RX_WRITE_DATA:
			writeData(bytesRead, bytes);
			break;
	}

}

void my_callback_tx_empty_notify(uint8_t port)
{
	_txEmpty = 0xFF;
}



int main(void)
{

	CS_DISABLE();
	
	cli();

	sysclk_init();
	udc_start();
	initSPI();
	initDebugUART();
	
	stdout = &mystdout;
	
	irq_initialize_vectors();


	sei();


	//printf("Started\r\n");


	while(1)
	{
		if (_runTest)
		{
			TESTWriteMem();
			_runTest = 0x00;
		}
		
		switch(_currentCommand)
		{
			case 'a':
				memRead256ToStdOut(0x00001000);
				_currentCommand = 0;
				break;
			case RX_READ_ID:
			case 'b':
				returnChipId();
				_currentCommand = 0;
				//printf("chip id returned. _currentCommand reset\r\n");
				break;
			case RX_READ_TEXT:
			case 'c':			
				readText();
				//printf("\r\nDone Reading Text\r\n");
				_currentCommand = 0;
				break;
			case RX_READ_DATA:
			case 'd':			
				readData();
				//printf("\r\nDone Reading Data\r\n");
				_currentCommand = 0;
				break;		
			case RX_ERASE_ALL:
			case 'e':			
				eraseChip();
				_currentCommand = 0;
				break;

		}
		

		
	}
	

}
