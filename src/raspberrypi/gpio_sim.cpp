#include "gpio_sim.h"

const char* GetPhaseStr(data_capture *sample){
    return BUS::GetPhaseStrRaw(GetPhase(sample));
}

BUS::phase_t GetPhase(data_capture *sample)
{
	// Selection Phase
	if (GetSel(sample)) {
		return BUS::selection;
	}

	// Bus busy phase
	if (!GetBsy(sample)) {
		return BUS::busfree;
	}

	// Get target phase from bus signal line
	DWORD mci = GetMsg(sample) ? 0x04 : 0x00;
	mci |= GetCd(sample) ? 0x02 : 0x00;
	mci |= GetIo(sample) ? 0x01 : 0x00;
	return BUS::GetPhase(mci);
}

// bool GPIOSim::GetBSY()
// {
// 	return GetSignal(PIN_BSY);
// }


// BOOL GPIOSim::GetSEL()
// {
// 	return GetSignal(PIN_SEL);
// }


// BOOL GPIOSim::GetATN()
// {
// 	return GetSignal(PIN_ATN);
// }


// BOOL GPIOSim::GetACK()
// {
// 	return GetSignal(PIN_ACK);
// }


// BOOL GPIOSim::GetRST()
// {
// 	return GetSignal(PIN_RST);
// }

// BOOL GPIOSim::GetMSG()
// {
// 	return GetSignal(PIN_MSG);
// }


// BOOL GPIOSim::GetCD()
// {
// 	return GetSignal(PIN_CD);
// }


// BOOL GPIOSim::GetIO()
// {
// 	return GetSignal(PIN_IO);
// }


// BOOL GPIOSim::GetREQ()
// {
// 	return GetSignal(PIN_REQ);
// }

// BYTE GPIOSim::GetDAT()
// {
// 	DWORD data = Aquire();
// 	data =
// 		((data >> (PIN_DT0 - 0)) & (1 << 0)) |
// 		((data >> (PIN_DT1 - 1)) & (1 << 1)) |
// 		((data >> (PIN_DT2 - 2)) & (1 << 2)) |
// 		((data >> (PIN_DT3 - 3)) & (1 << 3)) |
// 		((data >> (PIN_DT4 - 4)) & (1 << 4)) |
// 		((data >> (PIN_DT5 - 5)) & (1 << 5)) |
// 		((data >> (PIN_DT6 - 6)) & (1 << 6)) |
// 		((data >> (PIN_DT7 - 7)) & (1 << 7));

// 	return (BYTE)data;
// }

// BOOL GPIOSim::GetDP()
// {
// 	return GetSignal(PIN_DP);
// }

// BOOL GPIOSim::GetSignal(int pin)
// {
// 	return (current_gpio >> pin) & 1;
// }
// //---------------------------------------------------------------------------
// //
// //	Set output signal value
// //
// //---------------------------------------------------------------------------
// void GPIOSim::SetSignal(int pin, BOOL ast)
// {

// }

// DWORD GPIOSim::Aquire(){
//     return current_gpio;
// }

