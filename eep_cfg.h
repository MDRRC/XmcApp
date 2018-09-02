/***********************************************************************************************************************
   @file   wmc_app.cpp
   @brief  Main application of WifiManualControl (WMC).
 **********************************************************************************************************************/

#ifndef EEP_CFG_H
#define EEP_CFG_H

/***********************************************************************************************************************
   I N C L U D E S
 **********************************************************************************************************************/
#include <Arduino.h>

/***********************************************************************************************************************
   C L A S S E S
 **********************************************************************************************************************/

class EepCfg
{
public:
    static const uint8_t EepromVersion   = 2;  /* Version of data in EEPROM. */
    static const uint32_t EepromPageSize = 64; /* 24LC256 page size. */

    static const int EepromVersionAddress         = 0;  /* EEPROM address version info. */
    static const int AcTypeControlAddress         = 2;  /* EEPROM address for "AC" type control */
    static const int XpNetAddress                 = 4;  /* EEPROM address of Xpnet address */
    static const int EmergencyStopEnabledAddress  = 6;  /* EEPROM address for emergency option. */
    static const int locLibEepromAddressNumOfLocs = 8;  /* EEPROM address number of locs. */
    static const int locLibEepromAddressLocData   = 64; /* EEPROM address number of locs. */
};
#endif
