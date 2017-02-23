// Logic.h
////////////////////////////////////////////////////////////////////////////////////////////

void Logic_Init();

void Logic_MainLoopStep();

void Logic_ProcessControlCommand( const char* strCommand, const char* strValue = NULL );

int Logic_GetTmpADCValue();



class PIDRegulator
{
//private:
public:
// Attributes
	// Regulator parameters
	float m_fCoeffProport, m_fCoeffIntegr, m_fCoeffDiffer;
	float m_fInputOffsetMin, m_fInputOffsetMax;
	float m_fResultMin, m_fResultMax;

	float m_fAdditionalCoeffProport;

	// History params
	bool m_bFirstCalculation;
	float m_fLastResultValue;
	float m_fLastIntegralPartValue;
	uint m_nLastResultTicks;
	float m_fLastInputValue;
	float m_fLastDiffValue;

public:
// Construction
	PIDRegulator( float fCoeffProport, float fCoeffIntegr, float fCoeffDiffer, float fInputMin, float fInputMax, float fResultMin, float fResultMax );

// Operations
	float Calculate( float fInputValue, float fInputOffsetValue );

	void SetIntegralPartValue( int nResultValue )		{ m_fLastIntegralPartValue = nResultValue; };
	void SetAdditionalProportionCoeff( float fCoeff )	{ m_fAdditionalCoeffProport = fCoeff; }
};



// Control commands
const char* const Logic_ControlCommand_SetFlowSpeed = "SetFlowSpeed";
const char* const Logic_ControlCommand_SetFlowTemp = "SetFlowTemp";
const char* const Logic_ControlCommand_Reset = "Reset";

const char* const Logic_ControlCommand_ManualMode_SetVentilatorSpeedPercent = "SetVentilatorSpeedPercent";
const char* const Logic_ControlCommand_ManualMode_SetHeaterPowerPercent = "SetHeaterPowerPercent";

