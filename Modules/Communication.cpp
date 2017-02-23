// Communication.cpp
////////////////////////////////////////////////////////////////////////////////


#include "main.h"
#include "Communication.h"
#include "Logic.h"



////////////////////////////////////////////////////////////////////////////////
// Global objects

UART_HandleTypeDef Comm_hUART = {0};

struct Comm_PostMessage
{
	COMM_POSTMSG_TYPES 	enMsgType;
	char* 				strMessage;		// pointer to allocated string to send; = null if no string to send
};

const int Comm_PostMessageArrSize = 50;
Comm_PostMessage Comm_PostMessageArr[Comm_PostMessageArrSize];	// queue of strings to send

char* Comm_CurrentPostMessageBuff = NULL;		// currently sending string; null if no string

const int Comm_ReceivingBufferSize = 100;
char Comm_ReceivingBuffer[Comm_ReceivingBufferSize];

bool Comm_PostMessageOverrun = false;			// is set when post messages queue is full
bool Comm_ReceiveBufferOverrun = false;			// is set when overrun UART receive exception occurs



////////////////////////////////////////////////////////////////////////////////
// Predeclarations

static void TransmitNextMessage();
static char* AllocPostMessageString( const char *strMsg, COMM_POSTMSG_TYPES enMsgType );
static void ProcessReveivingByte();
static void ProcessIncomeCommand( const char *strCommand, int nCommandLength );



////////////////////////////////////////////////////////////////////////////////
// External functions

// Module initialization
//
void Comm_Init()
{
	// Reset globals
	memset( Comm_PostMessageArr, 0, Comm_PostMessageArrSize * sizeof(Comm_PostMessageArr[0]) );

	// Init GPIOs
	Comm_UARTPortRX.Init( GPIO_MODE_AF_PP, GPIO_PULLUP, GPIO_AF8_UART5 );
	Comm_UARTPortTX.Init( GPIO_MODE_AF_PP, GPIO_PULLUP, GPIO_AF8_UART5 );

	// Init UART device
	Comm_UARTEnableFunc();

	Comm_hUART.Instance = Comm_UARTHandle;
	Comm_hUART.Init.BaudRate = 19200;
	Comm_hUART.Init.WordLength = UART_WORDLENGTH_8B;
	Comm_hUART.Init.StopBits = UART_STOPBITS_1;
	Comm_hUART.Init.Parity = UART_PARITY_NONE;
	Comm_hUART.Init.Mode = UART_MODE_TX_RX;
	Comm_hUART.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	Comm_hUART.Init.OverSampling = UART_OVERSAMPLING_16;
	HAL_UART_Init( &Comm_hUART );

	// Init IRQs
	HAL_NVIC_SetPriority( UART5_IRQn, Comm_UARTInterruptPriority, 0 );
	HAL_NVIC_EnableIRQ( Comm_UART5_IRQn );

	// Start receiving process
	HAL_UART_Receive_IT( &Comm_hUART, (uint8 *)Comm_ReceivingBuffer, Comm_ReceivingBufferSize );

	// BT setup values:
	//	"AT+PIN2280"
	//	"AT+BAUD5"
	// BT info request values:
	//	"AT+VERSION"
}



// The main function to send a string
//
void Comm_SendString( const char *strMsg, COMM_POSTMSG_TYPES enMsgType /* = COMM_POSTMSG_DEF */ )
{
	// Prepare string buffer
	char* strBuff = AllocPostMessageString( strMsg, enMsgType );
	if ( !strBuff )
		return;

	// (!) Enter IRQs disable block
	DISABLE_IRQ();

	// Get free item the the queue
	int nFreePos = 0;
	while ( nFreePos < Comm_PostMessageArrSize && Comm_PostMessageArr[nFreePos].strMessage != NULL )
		nFreePos++;

	// Add message
	if ( nFreePos < Comm_PostMessageArrSize )
	// found free item
	{
		Comm_PostMessageArr[nFreePos].enMsgType = enMsgType;
		Comm_PostMessageArr[nFreePos].strMessage = strBuff;

		strBuff = NULL;	// mark as used
	}
	else
	// there is no free items - queue overrun
	{
		Comm_PostMessageOverrun = true;
	}

	// Start transmitting if transmission is not active at the moment
	TransmitNextMessage();

	// (!) End of IRQs disable block
	ENABLE_IRQ();

	// Free buffers (outside the IRQs disable block)
	if ( strBuff )
		free( strBuff );
}



void Comm_SendDebugString( const char *strMsg, ... )
{
	const int nBuffSize = 1000;
	static char buff[nBuffSize];

	buff[0] = 0;
	snprintf( buff, nBuffSize, "[%u] ", (uint)HAL_GetTick() );

    va_list  ap;
    va_start( ap, strMsg );
    vsnprintf( buff + strlen( buff ), nBuffSize, strMsg, ap );
    va_end( ap );

    Comm_SendString( buff, COMM_POSTMSG_DBG);
}



void Comm_SendDebugArray( void *pArr, int nItemsCount, int nItemSizeInBytes, int nItemsPitchInBytes, const char *strPrefix /* = NULL */ )
{
	int nItemCharsCount = ((nItemSizeInBytes == 1)? 3 : ((nItemSizeInBytes == 2)? 5 : 10));

	char *buff = new char[nItemsCount * (nItemCharsCount + 1) + (strPrefix ? strlen(strPrefix) : 0) + 1];
	char str[10];

	int nLen = 0;
	for ( int n = 0; n < nItemsCount; n++ )
	{
		void *p = (int8*)pArr + n * nItemsPitchInBytes;

		if ( nItemSizeInBytes == 1 )
			itoa( *(int8*)p, str, 10 );
		else if ( nItemSizeInBytes == 2 )
			itoa( *(int16*)p, str, 10 );
		else
			itoa( *(int*)p, str, 10 );

		strcpy( &buff[nLen], str );
		nLen += strlen( str );

		buff[nLen++] = ';';
	}
	buff[nLen] = 0;

	//Comm_SendDebugString( "Sensor: %d  ROUGH: %d  RESULT: %d", nSensorInd, (int)arr[nSensorInd], (int)(fValue * 100) );
	Comm_SendString( buff, COMM_POSTMSG_DBG);

	delete[] buff;
}



void Comm_SendMemInfo( const char *strPrefix /* = NULL */ )
{
	struct mallinfo in = mallinfo();

	if ( strPrefix )
		Comm_SendDebugString( "%s Memory allocated: %d", strPrefix, in.uordblks );
	else
		Comm_SendDebugString( "Memory allocated: %d", in.uordblks );
}




////////////////////////////////////////////////////////////////////////////////
// Internal functions


// Sends to UART the next string from the post messages queue, according to priorities
//
// Does nothing if no messages to send or if UART is busy
// (!) Use only within disable interrupt block
//
static void TransmitNextMessage()
{
	// Check if UART is busy
	if ( (Comm_hUART.State & HAL_UART_STATE_BUSY_TX) == HAL_UART_STATE_BUSY_TX )
		return;

	// Release old transmit buffer if any
	if ( Comm_CurrentPostMessageBuff )
		free( Comm_CurrentPostMessageBuff );
	Comm_CurrentPostMessageBuff = NULL;

	// Handle output queue overrun
	static uint nTimeOverrun = HAL_GetTick();
	if ( Comm_PostMessageOverrun && HAL_GetTick() - nTimeOverrun > 1000 )
	{
		Comm_PostMessageOverrun = false;
		nTimeOverrun = HAL_GetTick();

		// Prepare error message
		Comm_CurrentPostMessageBuff =  AllocPostMessageString( "UART output message queue overrun", COMM_POSTMSG_ERR );
		if ( !Comm_CurrentPostMessageBuff )
			return;

		// Transmit error message
		HAL_UART_Transmit_IT( &Comm_hUART, (uint8 *)Comm_CurrentPostMessageBuff, strlen( Comm_CurrentPostMessageBuff ) );

		return;
	}

	// Handle input queue overrun
	if ( Comm_ReceiveBufferOverrun )
	{
		Comm_ReceiveBufferOverrun = false;

		// Prepare error message
		Comm_CurrentPostMessageBuff =  AllocPostMessageString( "UART receive overrun", COMM_POSTMSG_ERR );
		if ( !Comm_CurrentPostMessageBuff )
			return;

		// Transmit error message
		HAL_UART_Transmit_IT( &Comm_hUART, (uint8 *)Comm_CurrentPostMessageBuff, strlen( Comm_CurrentPostMessageBuff ) );

		return;
	}

	// Try to find a message in the post messages queue
	int nFoundMsgInd = -1;
	COMM_POSTMSG_TYPES enFoundMsgTyppe = COMM_POSTMSG_DEF;

	for ( int nInd = 0; nInd < Comm_PostMessageArrSize; nInd++ )
	{
		// Skip empty
		if ( Comm_PostMessageArr[nInd].strMessage == NULL )
			continue;

		// Take only the most import message
		if ( nFoundMsgInd == -1 || Comm_PostMessageArr[nInd].enMsgType < enFoundMsgTyppe )
		{
			nFoundMsgInd = nInd;
			enFoundMsgTyppe = Comm_PostMessageArr[nInd].enMsgType;
		}
	}

	// Start transmission of found message
	if ( nFoundMsgInd != -1 )
	{
		Comm_PostMessage& msg = Comm_PostMessageArr[nFoundMsgInd];

		// Give the message string to HAL
		HAL_UART_Transmit_IT( &Comm_hUART, (uint8 *)msg.strMessage, strlen( msg.strMessage ) );

		// Save current buffer pointer
		Comm_CurrentPostMessageBuff = msg.strMessage;

		// Release the post message list item
		msg.strMessage = NULL;		// the buffer will be freed in transfer complete handler
	}

	// Sift all messages above
	if ( nFoundMsgInd != -1 )
	{
		for ( ; nFoundMsgInd < Comm_PostMessageArrSize - 1; nFoundMsgInd++ )
		{
			Comm_PostMessageArr[nFoundMsgInd] = Comm_PostMessageArr[nFoundMsgInd + 1];
		}
		Comm_PostMessageArr[Comm_PostMessageArrSize - 1].strMessage = NULL;
	}
}




// Allocates and prepares a string to send
//
// Generates a raw string to send through UART, with start-stop markers, msg type and given string
//
static char* AllocPostMessageString( const char *strMsg, COMM_POSTMSG_TYPES enMsgType )
{
	int nStartMarkerLen = strlen( Comm_PostMsgStartMarker );
	int nTypeNameLen = strlen( Comm_PostMsgTypeNames[enMsgType] );
	int nMsgLen = strlen( strMsg );

	// Allocate a new buffer for the string
	char* strBuff = (char *)malloc( nStartMarkerLen + nTypeNameLen + 1 + nMsgLen + 1 + 1 );
	if ( strBuff == NULL )
		return NULL;

	// Fill the buffer with the string and additional values
	char* strBuffTmp = strBuff;
	strcpy( strBuffTmp, Comm_PostMsgStartMarker );			// start marker
	strBuffTmp += nStartMarkerLen;
	strcpy( strBuffTmp, Comm_PostMsgTypeNames[enMsgType] );	// msg type
	strBuffTmp += nTypeNameLen;
	strcpy( strBuffTmp, " " );								// <space>
	strBuffTmp += 1;
	strcpy( strBuffTmp, strMsg );							// msg text
	strBuffTmp += nMsgLen;
	strcpy( strBuffTmp, "\n");								// <end of line>

	return strBuff;
}



static void ProcessReveivingByte()
{

	// Check for command end marker
	const int nEndMarkerSize = strlen( Comm_IncomeCommandEndMarker );

	char *strEndPtr = (char *)Comm_hUART.pRxBuffPtr;
	if ( strEndPtr < Comm_ReceivingBuffer + nEndMarkerSize
		|| memcmp( strEndPtr - nEndMarkerSize, Comm_IncomeCommandEndMarker, nEndMarkerSize ) != 0 )
		return;

	strEndPtr -= nEndMarkerSize;

	// Get command start position
	const int nStartMarkerSize = strlen( Comm_IncomeCommandStartMarker );

	char *strStartPrt = strEndPtr - nStartMarkerSize;
	while ( strStartPrt >= Comm_ReceivingBuffer && memcmp( strStartPrt, Comm_IncomeCommandStartMarker, nStartMarkerSize ) != 0 )
		strStartPrt--;

	if ( strStartPrt < Comm_ReceivingBuffer )
		return;		// command start marker not found

	strStartPrt += nStartMarkerSize;

	// Echo the command
	*strEndPtr = 0;		// will replace the end marker, but no problem

	Comm_SendString( strStartPrt, COMM_POSTMSG_ECHO );

	// Restart receiving process
	if( Comm_hUART.State == HAL_UART_STATE_BUSY_TX_RX )	Comm_hUART.State = HAL_UART_STATE_BUSY_TX;
	if( Comm_hUART.State == HAL_UART_STATE_BUSY_RX )	Comm_hUART.State = HAL_UART_STATE_READY;

	HAL_UART_Receive_IT( &Comm_hUART, (uint8 *)Comm_ReceivingBuffer, Comm_ReceivingBufferSize );

	// Handle the command
	ProcessIncomeCommand( strStartPrt, strEndPtr - strStartPrt );
}



static void ProcessIncomeCommand( const char *strCommand, int nCommandLength )
{
	char *strValue = NULL;

	// Separate command name and command value
	char *strSeparator = strstr( strCommand, Comm_IncomeCommandValueSeparateMarker );
	if ( strSeparator )
	{
		*strSeparator = 0;

		strValue = strSeparator + 1;
	}

	Logic_ProcessControlCommand( strCommand, strValue );
}



// Transmit complete handler
//
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	// (!) Enter IRQs disable block
	DISABLE_IRQ();

	// Start transmitting of next message if any
	TransmitNextMessage();

	// (!) End of IRQs disable block
	ENABLE_IRQ();
}



// Receive complete handler
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	// This call means that we didn't get command's end marker before the buffer is full.

	// Just start receiving process again
	HAL_UART_Receive_IT( &Comm_hUART, (uint8 *)Comm_ReceivingBuffer, Comm_ReceivingBufferSize );
}



// Error handler
//
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
	// Parse error type
	if ( huart->ErrorCode == HAL_UART_ERROR_ORE )
	// overrun error
	{
		Comm_ReceiveBufferOverrun = true;

		// (!) Enter IRQs disable block
		DISABLE_IRQ();

		// Start transmitting of messages starting with overrun error message
		TransmitNextMessage();

		// (!) End of IRQs disable block
		ENABLE_IRQ();
	}
	else
	// other error
	{
		// Add an error message and start transmitting of messages
		Comm_SendString( "UART receive error", COMM_POSTMSG_ERR );
	}

	// Restart receiving process, since it was stopped by error
	HAL_UART_Receive_IT( &Comm_hUART, (uint8 *)Comm_ReceivingBuffer, Comm_ReceivingBufferSize );
}



// System handler
//
extern "C" void UART5_IRQHandler(void)
{
	// Save receive buffer state
	void *pRxBuffPtrOld = Comm_hUART.pRxBuffPtr;

	// Process the interrupt by HAL
	HAL_UART_IRQHandler( &Comm_hUART );

	// Process receiving of new byte
	if ( Comm_hUART.pRxBuffPtr > pRxBuffPtrOld )		// we got one more byte
	{
		ProcessReveivingByte();
	}
}




