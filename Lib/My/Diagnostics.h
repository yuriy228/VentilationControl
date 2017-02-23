// Diagnostics.h
/////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once


// *** Debugging macros

#ifdef DEBUG
	#define ASSERT( val )	{ if (!(val)) printf( "ASSERTION: Line: %d File: %s", __LINE__, __FILE__ ); }
#else
	#define ASSERT( val );	// do nothing
#endif


// Calc (implement) a bool value, call "return" if a value is not TRUE
//	In debug version written app log and shows a message to user
//
#define VERIFY_RETURN( val )				{ if (!(val)) { ASSERT(0); return; } }

#define VERIFY_RETURN_VALUE( val, retVal )	{ if (!(val)) { ASSERT(0); return retVal; } }

#define VERIFY_RETURN_FALSE( val )			{ if (!(val)) { ASSERT(0); return false; } }

#define VERIFY_RETURN_NULL( val )			{ if (!(val)) { ASSERT(0); return false; } }

#define VERIFY_RETURN_RESULT( res )			{ if (IsFailed(res)) { ASSERT(0); return res; } }


// Evaluate a bool value
//	In debug version runs MFC ASSERT
// 
#define VERIFY_CHECK( val )		{ if (!(val)) ASSERT(0); }


