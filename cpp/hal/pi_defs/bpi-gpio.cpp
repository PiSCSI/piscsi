//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Copyright (C) 2022 akuker
//
//---------------------------------------------------------------------------

#include "hal/pi_defs/bpi-gpio.h"
#include "hal/sbc_version.h"
#include "log.h"

shared_ptr<Banana_Pi_Gpio_Mapping> BPI_GPIO::GetBpiGpioMapping(SBC_Version::sbc_version_type sbc_version)
{
    extern const Banana_Pi_Gpio_Mapping banana_pi_m1_m1p_r1_map;
    extern const Banana_Pi_Gpio_Mapping banana_pi_m2_map;
    extern const Banana_Pi_Gpio_Mapping banana_pi_m2m_1p1_map;
    extern const Banana_Pi_Gpio_Mapping banana_pi_m2m_map;
    extern const Banana_Pi_Gpio_Mapping banana_pi_m2p_map;
    extern const Banana_Pi_Gpio_Mapping banana_pi_m2u_map;
    extern const Banana_Pi_Gpio_Mapping banana_pi_m3_map;
    extern const Banana_Pi_Gpio_Mapping banana_pi_m64_map;

    switch (sbc_version) {
    case SBC_Version::sbc_version_type::sbc_raspberry_pi_1:
    case SBC_Version::sbc_version_type::sbc_raspberry_pi_2_3:
    case SBC_Version::sbc_version_type::sbc_raspberry_pi_4:
        throw std::invalid_argument("Should never get here. Can't have a Raspberry style Banana Pi");
    case SBC_Version::sbc_version_type::sbc_bananapi_m1_plus:
        LOGWARN("Banana Pi M1P is not tested or officially supported. Use at your own risk");
        return make_shared<Banana_Pi_Gpio_Mapping>(banana_pi_m1_m1p_r1_map);
    case SBC_Version::sbc_version_type::sbc_bananapi_m2_ultra:
        LOGWARN("Banana Pi M2 Ultra is not tested or officially supported. Use at your own risk");
        return make_shared<Banana_Pi_Gpio_Mapping>(banana_pi_m2u_map);

    case SBC_Version::sbc_version_type::sbc_bananapi_m2_berry:
        LOGWARN("Banana Pi M2 Berry is not tested or officially supported. Use at your own risk");
        // Note: ** I THINK ** M2 berry uses same mapping as M2 Ultra
        return make_shared<Banana_Pi_Gpio_Mapping>(banana_pi_m2u_map);

    case SBC_Version::sbc_version_type::sbc_bananapi_m2_plus:
        return make_shared<Banana_Pi_Gpio_Mapping>(banana_pi_m2p_map);

    case SBC_Version::sbc_version_type::sbc_bananapi_m2_zero:
        throw std::invalid_argument("Banana Pi M2 Zero is not implemented yet");
        break;
    case SBC_Version::sbc_version_type::sbc_bananapi_m3:
        LOGWARN("Banana Pi M3 is not tested or officially supported. Use at your own risk");
        return make_shared<Banana_Pi_Gpio_Mapping>(banana_pi_m3_map);
        break;
    case SBC_Version::sbc_version_type::sbc_bananapi_m4:
        throw std::invalid_argument("Banana Pi M4 is not implemented yet");
        break;
    case SBC_Version::sbc_version_type::sbc_bananapi_m5:
        throw std::invalid_argument("Banana Pi M5 is not implemented yet");
        break;
    case SBC_Version::sbc_version_type::sbc_bananapi_m64:
        throw std::invalid_argument("Banana Pi M64 is not implemented yet");
        break;
    case SBC_Version::sbc_version_type::sbc_unknown:
        throw std::invalid_argument("Unknown banana pi type");
        break;
    }
    LOGERROR("Unhandled single board computer type");
    return nullptr;
}