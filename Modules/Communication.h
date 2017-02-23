// Communication.h
////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////
// Constants

// Communication message types, in priority order
//
enum COMM_POSTMSG_TYPES
{
	COMM_POSTMSG_ERR = 0,	// error messages, highest priority
	COMM_POSTMSG_DBG,		// debug messages
	COMM_POSTMSG_ECHO,		// "echo" for incoming command
	COMM_POSTMSG_INFO,		// information messages, e.g. sensor values
	COMM_POSTMSG_DEF		// messages without type, least priority
};

const char* const Comm_PostMsgTypeNames[] = { "ERR:", "DBG:", "ECHO:", "INFO:", "" };	// text IDs for the message types, in same order as in COMM_MSG_TYPES

const char* const Comm_PostMsgStartMarker = "##";
const char* const Comm_IncomeCommandStartMarker = "##";
const char* const Comm_IncomeCommandEndMarker = "$$";				// should not be the same as start marker
const char* const Comm_IncomeCommandValueSeparateMarker = ":";		// separator for command name and value



/////////////////////////////////////////////////////////////////////////////////
// Functions

void Comm_Init();

void Comm_SendString( const char *strMsg, COMM_POSTMSG_TYPES enMsgType = COMM_POSTMSG_DEF );

// Debug logging functions
void Comm_SendDebugString( const char *strMsg, ... );		// supports string formatting
void Comm_SendMemInfo( const char *strPrefix = NULL );
void Comm_SendDebugArray( void *pArr, int nItemsCount, int nItemSizeInBytes, int nItemsPitchInBytes, const char *strPrefix = NULL );
