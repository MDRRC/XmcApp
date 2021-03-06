/***********************************************************************************************************************
   @file   xwc_app.cpp
   @brief  Main application of XPressNetManualControl (XMC).
 **********************************************************************************************************************/

/***********************************************************************************************************************
   I N C L U D E S
 **********************************************************************************************************************/
#include "xmc_app.h"
#include "fsmlist.hpp"
#include "tinyfsm.hpp"
#include "version.h"
#include "wmc_cv.h"
#include "xmc_event.h"
#include <EEPROM.h>

/***********************************************************************************************************************
   D E F I N E S
 **********************************************************************************************************************/

/***********************************************************************************************************************
   F O R W A R D  D E C L A R A T I O N S
 **********************************************************************************************************************/

/***********************************************************************************************************************
   D A T A   D E C L A R A T I O N S (exported, local)
 **********************************************************************************************************************/

/* Init variables. */
WmcTft xmcApp::m_xmcTft;
LocLib xmcApp::m_LocLib;
LocStorage xmcApp::m_LocStorage;
XpressNetClass xmcApp::m_XpNet;
locData xmcApp::m_LocDataReceived;
WmcCli xmcApp::m_WmcCommandLine;
WmcTft::locoInfo xmcApp::locInfoActual;
WmcTft::locoInfo xmcApp::locInfoPrevious;
locData xmcApp::m_LocDataRecievedPrevious;
uint8_t xmcApp::m_locFunctionAssignment[5];
uint16_t xmcApp::m_locDbData[200];
char xmcApp::m_locDbDataName[200][11];
uint16_t xmcApp::m_locDbDataCnt;
uint16_t xmcApp::m_locDbDataTransmitCnt;
uint32_t xmcApp::m_locDbDataTransmitDelay;
xmcApp::powerStatus xmcApp::m_PowerStatus           = off;
bool xmcApp::m_LocSelection                         = false;
bool xmcApp::m_PushButtonReleased                   = false;
uint8_t xmcApp::m_XpNetAddress                      = 0;
uint8_t xmcApp::m_ConnectCount                      = 0;
uint8_t xmcApp::m_SkipRequestCnt                    = 0;
uint16_t xmcApp::m_TurnOutAddress                   = 1;
xmcApp::turnoutDirection xmcApp::m_TurnOutDirection = ForwardOff;
uint32_t xmcApp::m_TurnoutOffDelay                  = 0;
bool xmcApp::m_CvPomProgrammingFromPowerOn          = false;
bool xmcApp::m_CvPomProgramming                     = false;
bool xmcApp::m_EmergencyStopEnabled                 = false;
uint16_t xmcApp::m_locAddressAdd                    = 1;
uint16_t xmcApp::m_locAddressChange                 = 1;
uint16_t xmcApp::m_LocAddressChangeActive           = 1;
uint16_t xmcApp::m_locAddressDelete                 = 1;
uint8_t xmcApp::m_locFunctionAdd                    = 0;
uint8_t xmcApp::m_locFunctionChange                 = 0;
bool xmcApp::m_PulseSwitchInvert                    = false;

/* Conversion table for 28 steps DCC speed to normal speed. */
const uint8_t SpeedStep28TableFromDcc[32] = { 0, 0, 1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 0, 0, 2, 4, 6, 8,
    10, 12, 14, 16, 18, 20, 22, 24, 26, 28 };

/***********************************************************************************************************************
  F U N C T I O N S
 **********************************************************************************************************************/

class stateInit;
class stateCheckXpNetAddress;
class stateInitXpNet;
class stateGetPowerStatus;
class stateGetLocData;
class statePowerOff;
class statePowerOn;
class statePowerEmergencyStop;
class stateProgrammingMode;
class stateTurnoutControl;
class stateTurnoutControlPowerOff;
class stateMainMenu1;
class stateMainMenu2;
class stateMenuLocAdd;
class stateMenuLocFunctionsAdd;
class stateMenuLocFunctionsChange;
class stateMenuLocDelete;
class stateMenuTransmitLocDatabase;
class stateCommandLineInterfaceActive;
class stateCvProgramming;

/***********************************************************************************************************************
 * Init the application and show start screen 3 seconds.
 */
class stateInit : public xmcApp
{
    /**
     * Init modules
     */
    void entry() override
    {
        m_xmcTft.Init();
        m_xmcTft.ShowVersion(SW_MAJOR, SW_MINOR, SW_PATCH);

        m_LocStorage.Init();
        m_LocLib.Init(m_LocStorage);
        m_WmcCommandLine.Init(m_LocLib, m_LocStorage);
        m_ConnectCount = 0;
    }

    /**
     * 3 Seconds timeout
     */
    void react(updateEvent500msec const&) override
    {
        /* Some delay so app name is displayed after power on. */
        m_ConnectCount++;
        if (m_ConnectCount >= 2)
        {
            m_XpNetAddress = m_LocStorage.XpNetAddressGet();

            if (m_XpNetAddress > 31)
            {
                /* If invalid XpNet address go to setting valid address. */
                transit<stateCheckXpNetAddress>();
            }
            else
            {
                /* Valid XpNet address, start.... */
                m_ConnectCount = 0;
                transit<stateInitXpNet>();
            }
        }
    }
};

/***********************************************************************************************************************
 * If XpNet address of device not valid set it.
 */
class stateCheckXpNetAddress : public xmcApp
{
    /**
     */
    void entry() override
    {
        m_XpNetAddress = 1;

        m_xmcTft.Clear();
        m_xmcTft.UpdateStatus("XPRESSNET ADDRESS", true, WmcTft::color_yellow);
        m_xmcTft.ShowXpNetAddress(m_XpNetAddress);
    }

    /**
     * Check pulse switch event data.
     */
    void react(pulseSwitchEvent const& e) override
    {
        switch (e.Status)
        {
        case turn:
        case pushturn:
            if (CheckPulseSwitchRevert(e.Delta) > 0)
            {
                /* Increase XpNet device address. Roll over when required. */
                if (m_XpNetAddress < 31)
                {
                    m_XpNetAddress++;
                }
                else
                {
                    m_XpNetAddress = 1;
                }

                m_xmcTft.ShowXpNetAddress(m_XpNetAddress);
            }
            else if (CheckPulseSwitchRevert(e.Delta) < 0)
            {
                /* Decrease XpNet device address. Roll over when required. */
                if (m_XpNetAddress > 1)
                {
                    m_XpNetAddress--;
                }
                else
                {
                    m_XpNetAddress = 31;
                }

                m_xmcTft.ShowXpNetAddress(m_XpNetAddress);
            }
            break;
        case pushedNormal:
        case pushedlong:
            /* Store selected address and reset to activate new address. */
            m_LocStorage.XpNetAddressSet(m_XpNetAddress);
            m_xmcTft.Clear();
            nvic_sys_reset();
            break;
        case pushedShort:
        case released: break;
        }
    }

    /**
     * Exit event.
     */
    void exit() override
    {
        m_xmcTft.Clear();
        m_xmcTft.ShowName();
    }
};

/***********************************************************************************************************************
 * Init the Xpressnet module.
 */
class stateInitXpNet : public xmcApp
{
    /**
     * Init xpressnet module.
     */
    void entry() override
    {
        m_XpNet.start(m_XpNetAddress, PB0);
        m_PulseSwitchInvert    = m_LocStorage.PulseSwitchInvertGet();
        m_EmergencyStopEnabled = m_LocStorage.EmergencyOptionGet();
    }

    /**
     * Next state.
     */
    void react(updateEvent500msec const&) override { transit<stateGetPowerStatus>(); }
};

/***********************************************************************************************************************
 * Get the power status of the central unit.
 */
class stateGetPowerStatus : public xmcApp
{
    /**
     */
    void entry() override {}

    /**
     * If power request after initial start was performed request status. AFter initial start of the
     * XPressnet module the XPressnet module requests status itself.
     */
    void react(updateEvent100msec const&) override
    {
        if (m_XpNet.getPower() != 255)
        {
            m_XpNet.commandStationStatusRequest();
        }
        m_WmcCommandLine.Update();
    }

    /**
     * Wait for "connection". If 500msec timer expires a (fast) response was not present and it is assumed the
     * module is not connected or detected....
     */
    void react(updateEvent500msec const&) override
    {
        if (m_ConnectCount == 0)
        {
            m_xmcTft.UpdateStatus("CONNECTING", true, WmcTft::color_green);
        }

        m_ConnectCount++;
        m_xmcTft.UpdateRunningWheel(m_ConnectCount);
    }

    void react(XpNetEvent const& e) override
    {
        switch (e.dataType)
        {
        case none:
        case powerOn:
            m_PowerStatus = powerStatus::on;
            transit<stateGetLocData>();
            break;
        case powerOff:
            m_PowerStatus = powerStatus::off;
            transit<stateGetLocData>();
            break;
        case powerStop:
            m_PowerStatus = powerStatus::emergency;
            transit<stateGetLocData>();
            break;
        case locdata: break;
        case programmingMode:
            m_PowerStatus = powerStatus::progMode;
            transit<stateProgrammingMode>();
            break;
        case cvResponse:
        case locDataBase:
        case locDatabaseTransmit: break;
        }
    }
};

/***********************************************************************************************************************
 * Get the actual loc data.
 */
class stateGetLocData : public xmcApp
{
    /**
     */
    void entry() override
    {
        m_XpNet.getLocoInfo((uint8_t)(m_LocLib.GetActualLocAddress() >> 8), (uint8_t)(m_LocLib.GetActualLocAddress()));
    }

    /**
     * Get loc info.
     */
    void react(updateEvent500msec const&) override
    {
        m_XpNet.getLocoInfo((uint8_t)(m_LocLib.GetActualLocAddress() >> 8), (uint8_t)(m_LocLib.GetActualLocAddress()));
    }

    /**
     * Handle the response.
     */
    void react(XpNetEvent const& e) override
    {
        locData* LocDataPtr = NULL;

        switch (e.dataType)
        {
        case none:
        case powerOn:
        case powerOff:
        case powerStop: break;
        case programmingMode:
            m_PowerStatus = powerStatus::progMode;
            transit<stateProgrammingMode>();
            break;
        case locdata:
            LocDataPtr = (locData*)(e.Data);
            memcpy(&m_LocDataReceived, LocDataPtr, sizeof(locData));

            m_xmcTft.Clear();
            updateLocInfoOnScreen(true);

            switch (m_PowerStatus)
            {
            case powerStatus::off: transit<statePowerOff>(); break;
            case powerStatus::on: transit<statePowerOn>(); break;
            case powerStatus::emergency: transit<statePowerEmergencyStop>(); break;
            case powerStatus::progMode: transit<stateProgrammingMode>(); break;
            }
            break;
        case cvResponse:
        case locDataBase:
        case locDatabaseTransmit: break;
        }
    }
};

/***********************************************************************************************************************
 * Power off state.
 */
class statePowerOff : public xmcApp
{
    /**
     */
    void entry() override
    {
        m_PowerStatus        = powerStatus::off;
        m_locDbDataCnt       = 0;
        m_LocSelection       = false;
        m_PushButtonReleased = false;
        m_xmcTft.UpdateStatus("POWER OFF", false, WmcTft::color_red);
        m_xmcTft.UpdateSelectedAndNumberOfLocs(m_LocLib.GetActualSelectedLocIndex(), m_LocLib.GetNumberOfLocs());

        /* Stop loc and update info on screen. */
        m_LocLib.SpeedSet(0);
        m_LocDataReceived.Speed = 0;
        updateLocInfoOnScreen(false);
        preparAndTransmitLocoDriveCommand(m_LocLib.SpeedGet());
    }

    /**
     * Get loc info.
     */
    void react(updateEvent500msec const&) override
    {
        // If command transmitted wait a little...
        if (m_SkipRequestCnt > 0)
        {
            m_SkipRequestCnt--;
        }
        else
        {
            m_SkipRequestCnt = 2;
            if (m_LocSelection == false)
            {
                m_XpNet.getLocoInfo(
                    (uint8_t)(m_LocLib.GetActualLocAddress() >> 8), (uint8_t)(m_LocLib.GetActualLocAddress()));
            }
        }
    }

    /**
     * Handle the response.
     */
    void react(XpNetEvent const& e) override
    {
        locData* LocDataPtr             = NULL;
        locDatabaseData* locDatabasePtr = NULL;

        switch (e.dataType)
        {
        case none:
        case powerOn: transit<statePowerOn>(); break;
        case powerOff:

        case powerStop: break;
        case locdata:
            // Only update when not selecting a loc or when button released after selecting.
            if ((m_LocSelection == false) || (m_PushButtonReleased = true))
            {
                LocDataPtr = (locData*)(e.Data);

                /* Roco Multimaus keeps transmitting set speed... Force zero speed. */
                LocDataPtr->Speed = 0;
                memcpy(&m_LocDataReceived, LocDataPtr, sizeof(locData));
                updateLocInfoOnScreen(m_PushButtonReleased);
                m_PushButtonReleased = false;
            }
            break;
        case programmingMode:
            m_PowerStatus = powerStatus::progMode;
            transit<stateProgrammingMode>();
            break;
        case locDataBase:
        {
            locDatabasePtr = (locDatabaseData*)(e.Data);

            /* First database entry received? Reset counter. */
            if (locDatabasePtr->Number == 0)
            {
                m_locDbDataCnt = 0;
                m_xmcTft.UpdateStatus("RECEIVING", false, WmcTft::color_white);

                // Copy initial loc data.
                m_locDbData[m_locDbDataCnt] = locDatabasePtr->Address;
                memset(&m_locDbDataName[m_locDbDataCnt][0], '\0', 11);
                memcpy(&m_locDbDataName[m_locDbDataCnt][0], locDatabasePtr->NameStr, 11 - 1);

                /* Update status row indicating something is happening. */
                m_xmcTft.UpdateSelectedAndNumberOfLocs(1, m_locDbDataCnt + 1);
            }
            else
            {
                /* XpressNet sends loc database data twice, only store one of both identical messages.*/
                if (m_locDbData[m_locDbDataCnt] != locDatabasePtr->Address)
                {
                    /* Add received loc address */
                    m_locDbDataCnt++;
                    m_locDbData[m_locDbDataCnt] = locDatabasePtr->Address;
                    memset(&m_locDbDataName[m_locDbDataCnt][0], '\0', 11);
                    memcpy(&m_locDbDataName[m_locDbDataCnt][0], locDatabasePtr->NameStr, 11 - 1);

                    /* Update status row indicating something is happening. */
                    m_xmcTft.UpdateSelectedAndNumberOfLocs(1, m_locDbDataCnt + 1);
                }
            }

            /* All received? Store and sort data. */
            if ((locDatabasePtr->Number + 1) == locDatabasePtr->Total)
            {
                StoreAndSortLocDatabaseData();

                /* Reset so new loc data can be used. */
                nvic_sys_reset();
            }
        }
        break;
        case cvResponse:
        case locDatabaseTransmit: break;
        }
    }

    /**
     * Check pulse switch event data.
     */
    void react(pulseSwitchEvent const& e) override
    {
        switch (e.Status)
        {
        case pushturn:
            /* Select next or previous loc. */
            if (CheckPulseSwitchRevert(e.Delta) != 0)
            {
                m_LocLib.GetNextLoc(CheckPulseSwitchRevert(CheckPulseSwitchRevert(e.Delta)));
                m_xmcTft.UpdateSelectedAndNumberOfLocs(
                    m_LocLib.GetActualSelectedLocIndex(), m_LocLib.GetNumberOfLocs());
                m_xmcTft.UpdateLocInfoSelect(m_LocLib.GetActualLocAddress(), m_LocLib.GetLocName());
                m_LocSelection = true;
            }
            break;
        case pushedShort:
            /* Power on request. */
            m_XpNet.setPower(csNormal);
            break;
        case pushedlong: transit<stateMainMenu1>(); break;
        case released:
            m_SkipRequestCnt     = 2;
            m_PushButtonReleased = true;
            m_XpNet.getLocoInfo(
                (uint8_t)(m_LocLib.GetActualLocAddress() >> 8), (uint8_t)(m_LocLib.GetActualLocAddress()));
            break;

        default: break;
        }
    }

    /**
     * Handle button events.
     */
    void react(pushButtonsEvent const& e) override
    {
        switch (e.Button)
        {
        case button_power: m_XpNet.setPower(csNormal); break;
        case button_0:
        case button_1:
        case button_2:
        case button_3:
        case button_4:
        case button_5:
        default: break;
        }
    };
};

/***********************************************************************************************************************
 * Power off state.
 */
class statePowerOn : public xmcApp
{
    /**
     */
    void entry() override
    {
        m_LocSelection       = false;
        m_PowerStatus        = powerStatus::on;
        m_SkipRequestCnt     = 0;
        m_PushButtonReleased = false;
        m_xmcTft.UpdateStatus("POWER ON ", false, WmcTft::color_green);
        m_xmcTft.UpdateSelectedAndNumberOfLocs(m_LocLib.GetActualSelectedLocIndex(), m_LocLib.GetNumberOfLocs());
    }

    /**
     * Get loc info.
     */
    void react(updateEvent500msec const&) override
    {
        if (m_SkipRequestCnt > 0)
        {
            m_SkipRequestCnt--;
        }
        else
        {
            m_SkipRequestCnt = 2;
            if (m_LocSelection == false)
            {
                m_XpNet.getLocoInfo(
                    (uint8_t)(m_LocLib.GetActualLocAddress() >> 8), (uint8_t)(m_LocLib.GetActualLocAddress()));
            }
        }
    }

    /**
     * Handle the response.
     */
    void react(XpNetEvent const& e) override
    {
        locData* LocDataPtr = NULL;
        switch (e.dataType)
        {
        case none:
        case powerOn: break;
        case powerOff: transit<statePowerOff>(); break;
        case powerStop: transit<statePowerEmergencyStop>(); break;
        case locdata:
            // Only update when not selecting a loc.
            if ((m_LocSelection == false) || (m_PushButtonReleased == true))
            {
                LocDataPtr = (locData*)(e.Data);
                memcpy(&m_LocDataReceived, LocDataPtr, sizeof(locData));
                updateLocInfoOnScreen(m_PushButtonReleased);
                m_PushButtonReleased = false;
            }
            break;
        case programmingMode:
            m_PowerStatus = powerStatus::progMode;
            transit<stateProgrammingMode>();
            break;
        case locDataBase:
        case locDatabaseTransmit:
        case cvResponse: break;
        }
    }

    /**
     * Check pulse switch event data.
     */
    void react(pulseSwitchEvent const& e) override
    {
        uint16_t Speed = 0;

        switch (e.Status)
        {
        case turn:
            Speed = m_LocLib.SpeedSet(CheckPulseSwitchRevert(e.Delta));
            if (Speed != 0xFFFF)
            {
                // Transmit loc speed etc. data.
                preparAndTransmitLocoDriveCommand(Speed);
            }
            break;
        case pushturn:
            /* Select next or previous loc. */
            if (CheckPulseSwitchRevert(e.Delta) != 0)
            {
                m_LocLib.GetNextLoc(CheckPulseSwitchRevert(CheckPulseSwitchRevert(e.Delta)));
                m_xmcTft.UpdateSelectedAndNumberOfLocs(
                    m_LocLib.GetActualSelectedLocIndex(), m_LocLib.GetNumberOfLocs());
                m_xmcTft.UpdateLocInfoSelect(m_LocLib.GetActualLocAddress(), m_LocLib.GetLocName());
                m_LocSelection = true;
            }
            break;
        case pushedShort:
            if (m_LocLib.SpeedGet() != 0)
            {
                /* Stop loc. */
                m_LocLib.SpeedSet(0);
            }
            else
            {
                /* Change direction. */
                m_LocLib.DirectionToggle();
            }
            /* Transmit loc speed etc. data. */
            preparAndTransmitLocoDriveCommand(m_LocLib.SpeedGet());
            m_SkipRequestCnt = 2;
            break;
        case pushedNormal:
            /* Change direction and transmit loc data.. */
            m_LocLib.DirectionToggle();
            preparAndTransmitLocoDriveCommand(m_LocLib.SpeedGet());
            m_SkipRequestCnt = 2;
            break;
        case pushedlong:
            m_CvPomProgramming            = true;
            m_CvPomProgrammingFromPowerOn = true;
            transit<stateCvProgramming>();
            break;
        case released:
            m_PushButtonReleased = true;
            m_SkipRequestCnt     = 2;
            m_XpNet.getLocoInfo(
                (uint8_t)(m_LocLib.GetActualLocAddress() >> 8), (uint8_t)(m_LocLib.GetActualLocAddress()));
            break;
        default: break;
        }
    };

    /**
     * Handle button events.
     */
    void react(pushButtonsEvent const& e) override
    {
        uint8_t Function = 0;
        switch (e.Button)
        {
        case button_power:
            if (m_EmergencyStopEnabled == false)
            {
                m_XpNet.setPower(csTrackVoltageOff);
            }
            else
            {
                m_XpNet.setPower(csEmergencyStop);
            }
            break;
        case button_0:
        case button_1:
        case button_2:
        case button_3:
        case button_4:
            /* Get the assigned function to the button, toggle function and transmit data.*/
            Function = m_LocLib.FunctionAssignedGet(static_cast<uint8_t>(e.Button));
            m_LocLib.FunctionToggle(Function);

            if (m_LocLib.FunctionStatusGet(Function) == LocLib::functionOn)
            {
                m_XpNet.setLocoFunc((uint8_t)(m_LocLib.GetActualLocAddress() >> 8),
                    (uint8_t)(m_LocLib.GetActualLocAddress()), 1, Function);
            }
            else
            {
                m_XpNet.setLocoFunc((uint8_t)(m_LocLib.GetActualLocAddress() >> 8),
                    (uint8_t)(m_LocLib.GetActualLocAddress()), 0, Function);
            }

            break;
        case button_5:
            /* To turnout control. */
            m_xmcTft.Clear();
            transit<stateTurnoutControl>();
            break;
        default: break;
        }
    };
};

/***********************************************************************************************************************
 * Emergency stop state, no speed control allowed, functions allowed.
 */
class statePowerEmergencyStop : public xmcApp
{
    /**
     * Show Emergency stop screen.
     */
    void entry() override
    {
        m_PowerStatus = powerStatus::emergency;
        m_xmcTft.UpdateStatus("POWER ON ", true, WmcTft::color_yellow);
        m_xmcTft.UpdateSelectedAndNumberOfLocs(m_LocLib.GetActualSelectedLocIndex(), m_LocLib.GetNumberOfLocs());

        /* Stop loc and update info on screen. */
        m_LocLib.SpeedSet(0);
        m_LocDataReceived.Speed = 0;
        updateLocInfoOnScreen(false);

        /* Transmit zero speed of selected loc... */
        preparAndTransmitLocoDriveCommand(m_LocLib.SpeedGet());
    };

    /**
     * Handle the response.
     */
    void react(XpNetEvent const& e) override
    {
        locData* LocDataPtr = NULL;
        switch (e.dataType)
        {
        case none:
        case powerOn: transit<statePowerOn>(); break;
        case powerOff: transit<statePowerOff>(); break;
        case powerStop: break;
        case locdata:
            LocDataPtr = (locData*)(e.Data);
            memcpy(&m_LocDataReceived, LocDataPtr, sizeof(locData));
            updateLocInfoOnScreen(false);
            break;
        case programmingMode:
            m_PowerStatus = powerStatus::progMode;
            transit<stateProgrammingMode>();
            break;
        case cvResponse:
        case locDatabaseTransmit:
        case locDataBase: break;
        }
    }

    /**
     * Check pulse switch event data.
     */
    void react(pulseSwitchEvent const& e) override
    {
        switch (e.Status)
        {
        case turn:
        case pushturn:
        case pushedShort: break;
        case pushedNormal:
            m_LocLib.DirectionToggle();
            preparAndTransmitLocoDriveCommand(m_LocLib.SpeedGet());
            m_SkipRequestCnt = 2;
            break;
        case pushedlong:
            m_CvPomProgramming = true;
            transit<stateCvProgramming>();
            break;

        default: break;
        }
    };

    /**
     * Handle button events.
     */
    void react(pushButtonsEvent const& e) override
    {
        uint8_t Function = 0;
        switch (e.Button)
        {
        case button_power: m_XpNet.setPower(csNormal); break;
        case button_0:
        case button_1:
        case button_2:
        case button_3:
        case button_4:
            /* Get the assinged function to the button, toggle function and transmit data.*/
            Function = m_LocLib.FunctionAssignedGet(static_cast<uint8_t>(e.Button));
            m_LocLib.FunctionToggle(Function);

            if (m_LocLib.FunctionStatusGet(Function) == LocLib::functionOn)
            {
                m_XpNet.setLocoFunc((uint8_t)(m_LocLib.GetActualLocAddress() >> 8),
                    (uint8_t)(m_LocLib.GetActualLocAddress()), 1, Function);
            }
            else
            {
                m_XpNet.setLocoFunc((uint8_t)(m_LocLib.GetActualLocAddress() >> 8),
                    (uint8_t)(m_LocLib.GetActualLocAddress()), 0, Function);
            }
            break;
        case button_5: break;
        default: break;
        }
    };
};

/***********************************************************************************************************************
 * External cv programmng active by another device.
 */
class stateProgrammingMode : public xmcApp
{
    void entry() override
    {
        m_PowerStatus = powerStatus::progMode;
        m_xmcTft.UpdateStatus("PROG MODE", true, WmcTft::color_yellow);
        m_xmcTft.UpdateSelectedAndNumberOfLocs(m_LocLib.GetActualSelectedLocIndex(), m_LocLib.GetNumberOfLocs());

        /* Stop loc and update info on screen. */
        m_LocLib.SpeedSet(0);
        m_LocDataReceived.Speed = 0;
        updateLocInfoOnScreen(false);
    };

    /**
     * Handle the response.
     */
    void react(XpNetEvent const& e) override
    {
        switch (e.dataType)
        {
        case none:
        case powerOn: transit<statePowerOn>(); break;
        case powerOff: transit<statePowerOff>(); break;
        case powerStop:
        case locdata:
        case programmingMode:
        case cvResponse:
        case locDataBase:
        case locDatabaseTransmit: break;
        }
    }

    /**
     * Handle button events.
     */
    void react(pushButtonsEvent const& e) override
    {
        switch (e.Button)
        {
        case button_power: m_XpNet.setPower(csTrackVoltageOff); break;
        case button_0:
        case button_1:
        case button_2:
        case button_3:
        case button_4:
        case button_5: break;
        default: break;
        }
    };
};

/***********************************************************************************************************************
 * Turnout control.
 */
class stateTurnoutControl : public xmcApp
{
    /**
     * Show turnout screen.
     */
    void entry() override
    {
        m_TurnOutDirection = ForwardOff;

        m_xmcTft.UpdateStatus("TURNOUT", true, WmcTft::color_green);
        m_xmcTft.ShowTurnoutScreen();
        m_xmcTft.ShowTurnoutAddress(m_TurnOutAddress);
        m_xmcTft.ShowTurnoutDirection(static_cast<uint8_t>(m_TurnOutDirection));
    };

    /**
     * Handle received data.
     */
    void react(updateEvent500msec const&) override
    {
        /* When turnout active sent after 500msec off command. */
        if ((m_TurnOutDirection == Forward) || (m_TurnOutDirection == Turn))
        {
            m_TurnOutDirection = ForwardOff;
            m_XpNet.setTrntPos((m_TurnOutAddress - 1) >> 8, (uint8_t)(m_TurnOutAddress - 1), 0x00);
            m_xmcTft.ShowTurnoutDirection(static_cast<uint8_t>(m_TurnOutDirection));
        }
    };

    /**
     * Handle the response.
     */
    void react(XpNetEvent const& e) override
    {
        switch (e.dataType)
        {
        case none:
        case powerOn: break;
        case powerOff: transit<stateTurnoutControlPowerOff>(); break;
        case powerStop:
        case locdata: break;
        case programmingMode:
            m_PowerStatus = powerStatus::progMode;
            transit<stateProgrammingMode>();
            break;
        case cvResponse:
        case locDataBase:
        case locDatabaseTransmit: break;
        }
    }

    /**
     * Handle pulse switch events.
     */
    void react(pulseSwitchEvent const& e) override
    {
        bool updateScreen = false;
        switch (e.Status)
        {
        case pushturn: break;
        case turn:
            if (CheckPulseSwitchRevert(e.Delta) > 0)
            {
                /* Increase address and check for overrun. */
                if (m_TurnOutAddress < ADDRESS_TURNOUT_MAX)
                {
                    m_TurnOutAddress++;
                }
                else
                {
                    m_TurnOutAddress = ADDRESS_TURNOUT_MIN;
                }

                updateScreen = true;
            }
            else if (CheckPulseSwitchRevert(e.Delta) < 0)
            {
                /* Decrease address and handle address 0. */
                if (m_TurnOutAddress > ADDRESS_TURNOUT_MIN)
                {
                    m_TurnOutAddress--;
                }
                else
                {
                    m_TurnOutAddress = ADDRESS_TURNOUT_MAX;
                }
                updateScreen = true;
            }
            break;
        case pushedShort:
            /* Reset turnout address. */
            m_TurnOutAddress = ADDRESS_TURNOUT_MIN;
            updateScreen     = true;
            break;
        case pushedNormal:
        case pushedlong:
        case released:
            /* Back to loc control. */
            transit<stateGetPowerStatus>();
            break;
        default: break;
        }

        /* Update address on display if required. */
        if (updateScreen == true)
        {
            m_xmcTft.ShowTurnoutAddress(m_TurnOutAddress);
        }
    };

    /**
     * Handle button events.
     */
    void react(pushButtonsEvent const& e) override
    {
        bool updateScreen       = true;
        bool sentTurnOutCommand = false;
        uint8_t turnoutData     = 0;

        /* Handle button requests. */
        switch (e.Button)
        {
        case button_power: m_XpNet.setPower(csTrackVoltageOff); break;
        case button_0: m_TurnOutAddress++; break;
        case button_1: m_TurnOutAddress += 10; break;
        case button_2: m_TurnOutAddress += 100; break;
        case button_3: m_TurnOutAddress += 1000; break;
        case button_4:
            m_TurnOutDirection = Forward;
            m_TurnoutOffDelay  = millis();
            updateScreen       = false;
            sentTurnOutCommand = true;
            break;
        case button_5:
            m_TurnOutDirection = Turn;
            m_TurnoutOffDelay  = millis();
            updateScreen       = false;
            sentTurnOutCommand = true;
            break;
        default: break;
        }

        if (updateScreen == true)
        {
            if (m_TurnOutAddress > ADDRESS_TURNOUT_MAX)
            {
                m_TurnOutAddress = 1;
            }
            m_xmcTft.ShowTurnoutAddress(m_TurnOutAddress);
        }

        if (sentTurnOutCommand == true)
        {
            /* Sent command and show turnout direction. */
            turnoutData = 0x08;
            if (m_TurnOutDirection == Forward)
            {
                turnoutData |= 1;
            }
            m_XpNet.setTrntPos((m_TurnOutAddress - 1) >> 8, (uint8_t)(m_TurnOutAddress - 1), turnoutData);
            m_xmcTft.ShowTurnoutDirection(static_cast<uint8_t>(m_TurnOutDirection));
        }
    };

    /**
     * When exit and turnout active transmit off command.
     */
    void exit() override
    {
        if ((m_TurnOutDirection == Forward) || (m_TurnOutDirection == Turn))
        {
            m_TurnOutDirection = ForwardOff;
            m_XpNet.setTrntPos((m_TurnOutAddress - 1) >> 8, (uint8_t)(m_TurnOutAddress - 1), m_TurnOutDirection);
        }
    }
};

/***********************************************************************************************************************
 * Power off screen and handling for turnout control.
 */
class stateTurnoutControlPowerOff : public xmcApp
{
    /**
     * Show turnout screen.
     */
    void entry() override
    {
        m_PowerStatus = powerStatus::off;
        m_xmcTft.UpdateStatus("TURNOUT", true, WmcTft::color_red);
    };

    /**
     * Handle the response.
     */
    void react(XpNetEvent const& e) override
    {
        switch (e.dataType)
        {
        case none:
        case powerOn: transit<stateTurnoutControl>(); break;
        case powerOff: break;
        case powerStop:
        case locdata: break;
        case programmingMode:
            m_PowerStatus = powerStatus::progMode;
            transit<stateProgrammingMode>();
            break;
        case cvResponse:
        case locDataBase:
        case locDatabaseTransmit: break;
        }
    }

    /**
     * Handle pulse switch events.
     */
    void react(pulseSwitchEvent const& e) override
    {
        switch (e.Status)
        {
        case pushturn:
        case turn:
        case pushedShort: break;
        case pushedNormal:
        case pushedlong:
        case released:
            /* Back to loc control. */
            transit<stateGetLocData>();
            break;
        }
    };

    /**
     * Handle button events.
     */
    void react(pushButtonsEvent const& e) override
    {
        /* Handle button requests. */
        switch (e.Button)
        {
        case button_power: m_XpNet.setPower(csNormal); break;
        case button_0:
        case button_1:
        case button_2:
        case button_3:
        case button_4:
        case button_5: break;
        default: break;
        }
    };
};

/***********************************************************************************************************************
 * Show first main menu  and handle the request.
 */
class stateMainMenu1 : public xmcApp
{
    /**
     * Show menu on screen.
     */
    void entry() override { m_xmcTft.ShowMenu1(); };

    /**
     * Handle pulse switch events.
     */
    void react(pulseSwitchEvent const& e) override
    {
        switch (e.Status)
        {
        case turn: transit<stateMainMenu2>(); break;
        case pushturn: break;
        case pushedShort:
        case pushedNormal:
        case pushedlong:
        case released:
            m_LocSelection = true;
            transit<stateGetPowerStatus>();
            break;
        }
    }

    /**
     * Handle switch events.
     */
    void react(pushButtonsEvent const& e) override
    {
        /* Handle menu request. */
        switch (e.Button)
        {
        case button_1:
            m_locAddressAdd = m_LocLib.GetActualLocAddress();
            transit<stateMenuLocAdd>();
            break;
        case button_2: transit<stateMenuLocFunctionsChange>(); break;
        case button_3: transit<stateMenuLocDelete>(); break;
        case button_4:
            m_CvPomProgramming = false;
            transit<stateCvProgramming>();
            break;
        case button_5:
            m_CvPomProgramming = true;
            transit<stateCvProgramming>();
            break;
        case button_power:
            m_LocSelection = true;
            transit<stateGetPowerStatus>();
            break;
        case button_0:
        case button_none: break;
        }
    };
};

/***********************************************************************************************************************
 * Show first main menu  and handle the request.
 */
class stateMainMenu2 : public xmcApp
{
    /**
     * Show menu on screen.
     */
    void entry() override { m_xmcTft.ShowMenu2(m_LocStorage.EmergencyOptionGet(), true); };

    /**
     * Handle pulse switch events.
     */
    void react(pulseSwitchEvent const& e) override
    {
        switch (e.Status)
        {
        case turn: transit<stateMainMenu1>(); break;
        case pushturn: break;
        case pushedShort:
        case pushedNormal:
        case pushedlong:
        case released:
            m_LocSelection = true;
            transit<stateGetPowerStatus>();
            break;
        }
    }

    /**
     * Handle switch events.
     */
    void react(pushButtonsEvent const& e) override
    {
        /* Handle menu request. */
        switch (e.Button)
        {
        case button_0: break;
        case button_1:
            // Set invalid XpNet device address and go to Xp address menu.
            m_LocStorage.XpNetAddressSet(255);
            transit<stateCheckXpNetAddress>();
            break;
        case button_2:
            /* Toggle emergency stop or power off for power button. */
            if (m_LocStorage.EmergencyOptionGet() == false)
            {
                m_LocStorage.EmergencyOptionSet(1);
                m_EmergencyStopEnabled = true;
                m_xmcTft.ShowMenu2(true, false);
            }
            else
            {
                m_LocStorage.EmergencyOptionSet(0);
                m_EmergencyStopEnabled = false;
                m_xmcTft.ShowMenu2(false, false);
            }
            break;
        case button_3:
            /* Transmit loc data to control unit. This option is NOT compatible
             * with the Roco MM !! Only for MDRCC-II
             */
            transit<stateMenuTransmitLocDatabase>();
            break;
        case button_4:
            /* Erase loc info and perform reset. */
            m_xmcTft.ShowErase();
            m_LocLib.InitialLocStore();
            m_LocStorage.NumberOfLocsSet(1);
            m_xmcTft.Clear();
            nvic_sys_reset();
            break;
        case button_5:
            /* Erase loc info and set invalid XpNet address. */
            m_xmcTft.ShowErase();
            m_LocLib.InitialLocStore();
            m_LocStorage.AcOptionSet(0);
            m_LocStorage.NumberOfLocsSet(1);
            m_LocStorage.XpNetAddressSet(255);
            m_LocStorage.EmergencyOptionSet(0);
            transit<stateCheckXpNetAddress>();
            break;
        case button_power:
            m_LocSelection = true;
            transit<stateGetPowerStatus>();
            break;
        case button_none: break;
        }
    };
};

/***********************************************************************************************************************
 * Add a loc.
 */
class stateMenuLocAdd : public xmcApp
{
    /**
     * Show loc menu add screen.
     */
    void entry() override
    {
        // Show loc add screen.
        m_xmcTft.Clear();
        m_xmcTft.UpdateStatus("ADD LOC", true, WmcTft::color_green);
        m_xmcTft.ShowLocSymbolFw(WmcTft::color_white);
        m_xmcTft.ShowlocAddress(m_locAddressAdd, WmcTft::color_green);
        m_xmcTft.UpdateSelectedAndNumberOfLocs(m_LocLib.GetActualSelectedLocIndex(), m_LocLib.GetNumberOfLocs());
    };

    /**
     * Handle pulse switch events.
     */
    void react(pulseSwitchEvent const& e) override
    {
        switch (e.Status)
        {
        case turn:
            /* Increase or decrease loc address to be added. */
            if (CheckPulseSwitchRevert(e.Delta) > 0)
            {
                m_locAddressAdd++;
                m_locAddressAdd = m_LocLib.limitLocAddress(m_locAddressAdd);
                m_xmcTft.ShowlocAddress(m_locAddressAdd, WmcTft::color_green);
            }
            else if (CheckPulseSwitchRevert(e.Delta) < 0)
            {
                m_locAddressAdd--;
                m_locAddressAdd = m_LocLib.limitLocAddress(m_locAddressAdd);
                m_xmcTft.ShowlocAddress(m_locAddressAdd, WmcTft::color_green);
            }
            break;
        case pushturn: break;
        case pushedNormal:
        case pushedlong:
            /* If loc is not present goto add functions else red address indicating loc already
             * present. */
            if (m_LocLib.CheckLoc(m_locAddressAdd) != 255)
            {
                m_xmcTft.ShowlocAddress(m_locAddressAdd, WmcTft::color_red);
            }
            else
            {
                transit<stateMenuLocFunctionsAdd>();
            }
            break;
        default: break;
        }
    };

    /**
     * Handle button events for easier / faster increase of address and reset of address.
     */
    void react(pushButtonsEvent const& e) override
    {
        bool updateScreen = true;

        switch (e.Button)
        {
        case button_0: m_locAddressAdd++; break;
        case button_1: m_locAddressAdd += 10; break;
        case button_2: m_locAddressAdd += 100; break;
        case button_3: m_locAddressAdd += 1000; break;
        case button_4: m_locAddressAdd = 1; break;
        case button_5:
            /* If loc is not present goto add functions else red address indicating loc already
             * present. */
            if (m_LocLib.CheckLoc(m_locAddressAdd) != 255)
            {
                updateScreen = false;
                m_xmcTft.ShowlocAddress(m_locAddressAdd, WmcTft::color_red);
            }
            else
            {
                transit<stateMenuLocFunctionsAdd>();
            }
            break;
        case button_power:
            updateScreen = false;
            transit<stateMainMenu1>();
            break;
        case button_none: updateScreen = false; break;
        }

        if (updateScreen == true)
        {
            m_locAddressAdd = m_LocLib.limitLocAddress(m_locAddressAdd);
            m_xmcTft.ShowlocAddress(m_locAddressAdd, WmcTft::color_green);
        }
    };
};

/***********************************************************************************************************************
 * Set the functions of the loc to be added.
 */
class stateMenuLocFunctionsAdd : public xmcApp
{
    /**
     * Show function add screen.
     */
    void entry() override
    {
        uint8_t Index;

        m_xmcTft.UpdateStatus("FUNCTIONS", true, WmcTft::color_green);
        m_locFunctionAdd = 0;
        for (Index = 0; Index < 5; Index++)
        {
            m_locFunctionAssignment[Index] = Index;
        }

        m_xmcTft.FunctionAddSet();
        m_xmcTft.UpdateSelectedAndNumberOfLocs(m_LocLib.GetActualSelectedLocIndex(), m_LocLib.GetNumberOfLocs());
    };

    /**
     * Handle pulse switch events.
     */
    void react(pulseSwitchEvent const& e) override
    {
        switch (e.Status)
        {
        case turn:
            /* ncrease of decrease the function. */
            if (CheckPulseSwitchRevert(e.Delta) > 0)
            {
                m_locFunctionAdd++;
                if (m_locFunctionAdd > FUNCTION_MAX)
                {
                    m_locFunctionAdd = FUNCTION_MIN;
                }
                m_xmcTft.FunctionAddUpdate(m_locFunctionAdd);
            }
            else if (CheckPulseSwitchRevert(e.Delta) < 0)
            {
                if (m_locFunctionAdd == FUNCTION_MIN)
                {
                    m_locFunctionAdd = FUNCTION_MAX;
                }
                else
                {
                    m_locFunctionAdd--;
                }
                m_xmcTft.FunctionAddUpdate(m_locFunctionAdd);
            }
            break;
        case pushedNormal:
            /* Store loc functions */
            m_xmcTft.UpdateStatus("SORTING  ", false, WmcTft::color_white);
            m_LocLib.StoreLoc(m_locAddressAdd, m_locFunctionAssignment, NULL, LocLib::storeAdd);
            m_LocLib.LocBubbleSort();
            m_locAddressAdd++;
            transit<stateMenuLocAdd>();
            break;
        default: break;
        }
    };

    /**
     * Handle button events.
     */
    void react(pushButtonsEvent const& e) override
    {
        switch (e.Button)
        {
        case button_0:
            /* Button 0 only for light or other functions. */
            m_locFunctionAssignment[static_cast<uint8_t>(e.Button)] = m_locFunctionAdd;
            m_xmcTft.UpdateFunction(
                static_cast<uint8_t>(e.Button), m_locFunctionAssignment[static_cast<uint8_t>(e.Button)]);
            break;
        case button_1:
        case button_2:
        case button_3:
        case button_4:
            /* Rest of buttons for oher functions except light. */
            if (m_locFunctionAdd != 0)
            {
                m_locFunctionAssignment[static_cast<uint8_t>(e.Button)] = m_locFunctionAdd;
                m_xmcTft.UpdateFunction(
                    static_cast<uint8_t>(e.Button), m_locFunctionAssignment[static_cast<uint8_t>(e.Button)]);
            }
            break;
        case button_power: transit<stateMainMenu1>(); break;
        case button_5:
            /* Store loc functions */
            m_xmcTft.UpdateStatus("SORTING  ", false, WmcTft::color_white);
            m_LocLib.StoreLoc(m_locAddressAdd, m_locFunctionAssignment, NULL, LocLib::storeAdd);
            m_LocLib.LocBubbleSort();
            m_locAddressAdd++;
            transit<stateMenuLocAdd>();
            break;
        case button_none: break;
        }
    };
};

/***********************************************************************************************************************
 * Changer functions of a loc already present.
 */
class stateMenuLocFunctionsChange : public xmcApp
{
    /**
     * Show change function screen.
     */
    void entry() override
    {
        uint8_t Index;

        m_xmcTft.Clear();
        m_locFunctionChange      = 0;
        m_locAddressChange       = m_LocLib.GetActualLocAddress();
        m_LocAddressChangeActive = m_locAddressChange;
        m_xmcTft.UpdateStatus("CHANGE FUNC", true, WmcTft::color_green);
        m_xmcTft.ShowLocSymbolFw(WmcTft::color_white);
        m_xmcTft.ShowlocAddress(m_locAddressChange, WmcTft::color_green);
        m_xmcTft.FunctionAddUpdate(m_locFunctionChange);
        m_xmcTft.UpdateSelectedAndNumberOfLocs(m_LocLib.GetActualSelectedLocIndex(), m_LocLib.GetNumberOfLocs());

        for (Index = 0; Index < 5; Index++)
        {
            m_locFunctionAssignment[Index] = m_LocLib.FunctionAssignedGet(Index);
            m_xmcTft.UpdateFunction(Index, m_locFunctionAssignment[Index]);
        }
    }

    /**
     * Handle pulse switch events.
     */
    void react(pulseSwitchEvent const& e) override
    {
        uint8_t Index = 0;

        switch (e.Status)
        {
        case turn:
            /* Change function. */
            if (CheckPulseSwitchRevert(e.Delta) > 0)
            {
                m_locFunctionChange++;
                if (m_locFunctionChange > FUNCTION_MAX)
                {
                    m_locFunctionChange = FUNCTION_MIN;
                }
                m_xmcTft.FunctionAddUpdate(m_locFunctionChange);
            }
            else if (CheckPulseSwitchRevert(e.Delta) < 0)
            {
                if (m_locFunctionChange == FUNCTION_MIN)
                {
                    m_locFunctionChange = FUNCTION_MAX;
                }
                else
                {
                    m_locFunctionChange--;
                }
                m_xmcTft.FunctionAddUpdate(m_locFunctionChange);
            }
            break;
        case pushturn:
            /* Select another loc and update function data of newly selected loc. */
            m_locAddressChange = m_LocLib.GetNextLoc(CheckPulseSwitchRevert(e.Delta));
            m_xmcTft.UpdateSelectedAndNumberOfLocs(m_LocLib.GetActualSelectedLocIndex(), m_LocLib.GetNumberOfLocs());

            for (Index = 0; Index < 5; Index++)
            {
                m_locFunctionAssignment[Index] = m_LocLib.FunctionAssignedGet(Index);
                m_xmcTft.UpdateFunction(Index, m_locFunctionAssignment[Index]);
            }

            m_xmcTft.ShowlocAddress(m_locAddressChange, WmcTft::color_green);
            break;
        case pushedNormal:
        case pushedlong:
            /* Store changed data and yellow text indicating data is stored. */
            m_LocLib.StoreLoc(m_locAddressChange, m_locFunctionAssignment, NULL, LocLib::storeChange);
            m_xmcTft.ShowlocAddress(m_locAddressChange, WmcTft::color_yellow);

            /* Update data. Misuse locselection variable to force update when loc screen is redrawn. */
            m_LocSelection = true;
            m_LocLib.UpdateLocData(m_locAddressChange);
            break;
        default: break;
        }
    };

    /**
     * Handle button events.
     */
    void react(pushButtonsEvent const& e) override
    {
        switch (e.Button)
        {
        case button_0:
            /* Button 0 only for light or other functions. */
            m_locFunctionAssignment[static_cast<uint8_t>(e.Button)] = m_locFunctionChange;
            m_xmcTft.UpdateFunction(
                static_cast<uint8_t>(e.Button), m_locFunctionAssignment[static_cast<uint8_t>(e.Button)]);
            break;
        case button_1:
        case button_2:
        case button_3:
        case button_4:
            /* Rest of buttons for other functions except light. */
            if (m_locFunctionChange != 0)
            {
                m_locFunctionAssignment[static_cast<uint8_t>(e.Button)] = m_locFunctionChange;
                m_xmcTft.UpdateFunction(
                    static_cast<uint8_t>(e.Button), m_locFunctionAssignment[static_cast<uint8_t>(e.Button)]);
            }
            break;
        case button_power:
            /* If loc where functions are changed is unequal to active controlled loc set active controlled loc.
               Misuse LocSelection to force screen update of active selected loc. */
            if (m_LocAddressChangeActive != m_locAddressChange)
            {
                m_LocSelection = true;
                m_LocLib.UpdateLocData(m_LocAddressChangeActive);
            }
            transit<stateMainMenu1>();
            break;
        case button_5:
            /* Store changed data and yellow text indicating data is stored. */
            m_LocLib.StoreLoc(m_locAddressChange, m_locFunctionAssignment, NULL, LocLib::storeChange);
            m_xmcTft.ShowlocAddress(m_locAddressChange, WmcTft::color_yellow);
            break;
        case button_none: break;
        }
    };
};

/***********************************************************************************************************************
 * Delete a loc.
 */
class stateMenuLocDelete : public xmcApp
{
    /**
     * Show delete screen.
     */
    void entry() override
    {
        m_xmcTft.Clear();
        m_locAddressDelete = m_LocLib.GetActualLocAddress();
        m_xmcTft.UpdateStatus("DELETE", true, WmcTft::color_green);
        m_xmcTft.ShowLocSymbolFw(WmcTft::color_white);
        m_xmcTft.ShowlocAddress(m_locAddressDelete, WmcTft::color_green);
        m_xmcTft.UpdateSelectedAndNumberOfLocs(m_LocLib.GetActualSelectedLocIndex(), m_LocLib.GetNumberOfLocs());
    }

    /**
     * Handle pulse switch events.
     */
    void react(pulseSwitchEvent const& e) override
    {
        uint16_t LocAddresActual = 0;

        switch (e.Status)
        {
        case turn:
            /* Select loc to be deleted. */
            m_locAddressDelete = m_LocLib.GetNextLoc(CheckPulseSwitchRevert(e.Delta));
            m_xmcTft.UpdateSelectedAndNumberOfLocs(m_LocLib.GetActualSelectedLocIndex(), m_LocLib.GetNumberOfLocs());
            m_xmcTft.ShowlocAddress(m_locAddressDelete, WmcTft::color_green);
            break;
        case pushedNormal:
        case pushedlong:
            /* Remove loc. */
            LocAddresActual = m_LocLib.GetActualLocAddress();

            // Only delete when at least two locs are present.
            if (m_LocLib.GetNumberOfLocs() > 1)
            {
                m_xmcTft.UpdateStatus("DELETING", true, WmcTft::color_red);
                m_LocLib.RemoveLoc(m_locAddressDelete);
                m_xmcTft.UpdateStatus("DELETE", true, WmcTft::color_green);
                m_xmcTft.UpdateSelectedAndNumberOfLocs(
                    m_LocLib.GetActualSelectedLocIndex(), m_LocLib.GetNumberOfLocs());

                // In case the selected loc was the same as the active controlled loc update last selected loc entry.
                if (m_locAddressDelete == LocAddresActual)
                {
                    m_locAddressDelete = m_LocLib.GetActualLocAddress();
                    m_LocStorage.SelectedLocIndexStore(m_LocLib.GetActualSelectedLocIndex() - 1);
                }
                else
                {
                    m_locAddressDelete = m_LocLib.GetActualLocAddress();
                }
            }

            m_xmcTft.ShowlocAddress(m_locAddressDelete, WmcTft::color_green);
            break;
        default: break;
        }
    }

    /**
     * Handle button events, just go back to main menu on each button.
     */
    void react(pushButtonsEvent const& e) override
    {
        switch (e.Button)
        {
        case button_0:
        case button_1:
        case button_2:
        case button_3:
        case button_4:
        case button_5:
        case button_power: transit<stateMainMenu1>(); break;
        case button_none: break;
        }
    };
};

/***********************************************************************************************************************
 * Transmit loc data on XpressNet
 */
class stateMenuTransmitLocDatabase : public xmcApp
{
    void entry() override
    {
        m_locDbDataTransmitCnt   = 0;
        m_locDbDataTransmitDelay = millis();

        m_xmcTft.UpdateStatus("SEND LOC DATA", true, WmcTft::color_white);
        m_XpNet.TransmitLocDatabaseEnable();
    }

    /**
     * Handle the XP net request.
     */
    void react(XpNetEvent const& e) override
    {
        LocLibData* LocDbData;

        switch (e.dataType)
        {
        case none: break;
        case powerOn:
        case powerOff:
            m_XpNet.TransmitLocDatabaseDisable();
            transit<stateMainMenu2>();
            break;
        case powerStop:
        case locdata:
        case programmingMode:
        case cvResponse:
        case locDataBase: break;
        case locDatabaseTransmit:
            /* Transmit with some delay... */
            if (millis() - m_locDbDataTransmitDelay >= LOC_DATABASE_TX_DELAY)
            {
                m_locDbDataTransmitDelay = millis();

                // Send loc data until last loc is transmitted.
                m_xmcTft.UpdateTransmitCount(
                    static_cast<uint8_t>(m_locDbDataTransmitCnt), static_cast<uint8_t>(m_LocLib.GetNumberOfLocs()));

                LocDbData = m_LocLib.LocGetAllDataByIndex(m_locDbDataTransmitCnt);
                m_XpNet.TransmitLocData(LocDbData->Addres >> 8, LocDbData->Addres, m_locDbDataTransmitCnt,
                    static_cast<uint8_t>(m_LocLib.GetNumberOfLocs()));
                m_locDbDataTransmitCnt++;

                if (m_locDbDataTransmitCnt > m_LocLib.GetNumberOfLocs())
                {
                    m_XpNet.TransmitLocDatabaseDisable();
                    transit<stateMainMenu2>();
                }
            }
            break;
        }
    }

    /**
     * Handle pulse switch events.
     */
    void react(pulseSwitchEvent const& e) override
    {
        switch (e.Status)
        {
        case turn:
        case pushedNormal:
        case pushedlong:
            m_XpNet.TransmitLocDatabaseDisable();
            transit<stateMainMenu2>();
            break;
            break;
        default: break;
        }
    }

    /**
     * Handle button events, just go back to main menu on each button.
     */
    void react(pushButtonsEvent const& e) override
    {
        switch (e.Button)
        {
        case button_0:
        case button_1:
        case button_2:
        case button_3:
        case button_4:
        case button_5:
        case button_power:
            m_XpNet.TransmitLocDatabaseDisable();
            transit<stateMainMenu2>();
            break;
        case button_none: break;
        }
    };
};

/***********************************************************************************************************************
 * Command line interface active state.
 */
class stateCommandLineInterfaceActive : public xmcApp
{
    /**
     * Show delete screen.
     */
    void entry() override
    {
        m_xmcTft.Clear();
        m_xmcTft.UpdateStatus("COMMAND LINE", true, WmcTft::color_green);
        m_xmcTft.CommandLine();
    };
};

/***********************************************************************************************************************
 * CV programming main state.
 */
class stateCvProgramming : public xmcApp
{
    /**
     * Show delete screen.
     */
    void entry() override
    {
        cvEvent EventCv;
        m_xmcTft.Clear();
        if (m_CvPomProgramming == false)
        {
            EventCv.EventData = startCv;
        }
        else
        {
            EventCv.EventData = startPom;
            m_XpNet.setPower(csNormal);
        }

        send_event(EventCv);
    };

    /**
     * Handle the response.
     */
    void react(XpNetEvent const& e) override
    {
        cvEvent EventCv;
        cvResponseData* CvResponsePtr = NULL;
        switch (e.dataType)
        {
        case none:
        case powerOn: break;
        case powerOff: transit<stateGetPowerStatus>(); break;
        case powerStop:
        case locdata:
        case locDataBase:
        case locDatabaseTransmit:
        case programmingMode: break;
        case cvResponse:
            CvResponsePtr = (cvResponseData*)(e.Data);

            switch (CvResponsePtr->cvInfo)
            {
            case centralBusy: EventCv.EventData = responseBusy; break;
            case centralReady:
                EventCv.cvValue   = 1;
                EventCv.EventData = responseReady;
                break;
            case dataNotFound:
            case centralShotCircuit:
            case transmitError: EventCv.EventData = responseNok; break;
            case dataReady:
                EventCv.EventData = responseReady;
                EventCv.cvNumber  = CvResponsePtr->cvNumber;
                EventCv.cvValue   = CvResponsePtr->cvValue;
                break;
            }
            send_event(EventCv);
            break;
        }
    }

    /**
     * Handle pulse switch events.
     */
    void react(pulseSwitchEvent const& e) override
    {
        cvpulseSwitchEvent Event;

        switch (e.Status)
        {
        case pushturn:
        case turn:
        case pushedShort:
        case pushedNormal:
            /* Forward event */
            Event.EventData.Delta  = CheckPulseSwitchRevert(e.Delta);
            Event.EventData.Status = e.Status;
            send_event(Event);
            break;
        default: break;
        }
    };

    void react(updateEvent500msec const&) override
    {
        cvEvent EventCv;
        EventCv.EventData = update;
        send_event(EventCv);
    }

    /**
     * Handle button events.
     */
    void react(pushButtonsEvent const& e) override
    {
        cvpushButtonEvent Event;

        /* Handle menu request. */
        switch (e.Button)
        {
        case button_0:
        case button_1:
        case button_2:
        case button_3:
        case button_4:
        case button_5:
        case button_power:
            /* Forward event.*/
            Event.EventData.Button = e.Button;
            send_event(Event);
            break;
        case button_none: break;
        }
    };

    /**
     * Handle events from the cv state machine.
     */
    void react(cvProgEvent const& e) override
    {
        switch (e.Request)
        {
        case cvRead: m_XpNet.readCVMode(e.CvNumber); break;
        case cvWrite: m_XpNet.writeCVMode(e.CvNumber, e.CvValue); break;
        case cvStatusRequest: m_XpNet.getresultCV(); break;
        case pomWrite: m_XpNet.writeCvPom(e.Address >> 8, e.Address, e.CvNumber - 1, e.CvValue); break;
        case cvExit:
            if (m_CvPomProgrammingFromPowerOn == false)
            {
                m_XpNet.setPower(csTrackVoltageOff);
                transit<stateMainMenu1>();
            }
            else
            {
                transit<stateGetLocData>();
            }
            break;
        }
    }

    /**
     * Exit handler.
     */
    void exit() override { m_CvPomProgrammingFromPowerOn = false; };
};

/***********************************************************************************************************************
 * Default event handlers when not declared in states itself.
 */
void xmcApp::react(XpNetEvent const&){};
void xmcApp::react(xpNetEventUpdate const&) { m_XpNet.receive(); };
void xmcApp::react(cliEnterEvent const&) { transit<stateCommandLineInterfaceActive>(); };
void xmcApp::react(updateEvent3sec const&){};
void xmcApp::react(pushButtonsEvent const&){};
void xmcApp::react(pulseSwitchEvent const&){};
void xmcApp::react(updateEvent100msec const&) { m_WmcCommandLine.Update(); };
void xmcApp::react(updateEvent500msec const&){};
void xmcApp::react(cvProgEvent const&){};

/***********************************************************************************************************************
 * Initial state.
 */
FSM_INITIAL_STATE(xmcApp, stateInit)

/***********************************************************************************************************************
 * Convert loc data to tft loc data.
 */
void xmcApp::convertLocDataToDisplayData(locData* XpDataPtr, WmcTft::locoInfo* TftDataPtr)
{
    TftDataPtr->Address = XpDataPtr->Address;
    TftDataPtr->Speed   = XpDataPtr->Speed;

    switch (XpDataPtr->Steps)
    {
    case 0: TftDataPtr->Steps = WmcTft::locoDecoderSpeedSteps14; break;
    case 2: TftDataPtr->Steps = WmcTft::locoDecoderSpeedSteps28; break;
    case 4: TftDataPtr->Steps = WmcTft::locoDecoderSpeedSteps128; break;
    default: TftDataPtr->Steps = WmcTft::locoDecoderSpeedStepsUnknown; break;
    }

    switch (XpDataPtr->Direction)
    {
    case 0: TftDataPtr->Direction = WmcTft::locoDirectionBackward; break;
    case 1: TftDataPtr->Direction = WmcTft::locoDirectionForward; break;
    }

    if ((XpDataPtr->Functions & 0x01) == 0x01)
    {
        TftDataPtr->Light = WmcTft::locoLightOn;
    }
    else
    {
        TftDataPtr->Light = WmcTft::locoLightOff;
    }

    // For display shift out light info...
    TftDataPtr->Functions = XpDataPtr->Functions >> 1;
    TftDataPtr->Occupied  = XpDataPtr->Occupied;
}

/***********************************************************************************************************************
 * Update loc info on screen.
 */
void xmcApp::updateLocInfoOnScreen(bool updateAll)
{
    uint8_t Index = 0;

    if (m_LocLib.GetActualLocAddress() == m_LocDataReceived.Address)
    {
        m_LocLib.SpeedUpdate(m_LocDataReceived.Speed);

        /* Convert direction. */
        switch (m_LocDataReceived.Direction)
        {
        case 0: m_LocLib.DirectionSet(directionBackWard); break;
        case 1: m_LocLib.DirectionSet(directionForward); break;
        default: m_LocLib.DirectionSet(directionForward); break;
        }

        /* Set function data. */
        m_LocLib.FunctionUpdate(m_LocDataReceived.Functions);

        /* Convert decoder steps. */
        switch (m_LocDataReceived.Steps)
        {
        case 0: m_LocLib.DecoderStepsUpdate(decoderStep14); break;
        case 2: m_LocLib.DecoderStepsUpdate(decoderStep28); break;
        case 4: m_LocLib.DecoderStepsUpdate(decoderStep128); break;
        default: m_LocLib.DecoderStepsUpdate(decoderStep28); break;
        }

        /* Get function assignment of loc. */
        for (Index = 0; Index < 5; Index++)
        {
            m_locFunctionAssignment[Index] = m_LocLib.FunctionAssignedGet(Index);
        }

        /* Invert functions so function symbols are updated if new loc is selected and set new
         * direction. */
        if (m_LocSelection == true)
        {
            m_LocDataRecievedPrevious.Functions = ~m_LocDataReceived.Functions;
        }

        /* Convert data for display. */
        convertLocDataToDisplayData(&m_LocDataReceived, &locInfoActual);
        convertLocDataToDisplayData(&m_LocDataRecievedPrevious, &locInfoPrevious);
        m_xmcTft.UpdateLocInfo(
            &locInfoActual, &locInfoPrevious, m_locFunctionAssignment, m_LocLib.GetLocName(), updateAll);

        memcpy(&m_LocDataRecievedPrevious, &m_LocDataReceived, sizeof(locData));
        m_LocSelection = false;
    }
}

/***********************************************************************************************************************
 * Prepare and transmit loco drive command
 */
void xmcApp::preparAndTransmitLocoDriveCommand(uint16_t SpeedSet)
{
    uint8_t Steps = 0;
    uint8_t Speed = 0;
    // Determine decoder speed step.
    switch (m_LocLib.DecoderStepsGet())
    {
    case decoderStep14:
        Steps = 0;
        Speed = static_cast<uint8_t>(SpeedSet);
        break;
    case decoderStep28:
        Steps = 2;
        Speed = SpeedStep28TableToDcc[static_cast<uint8_t>(SpeedSet)];
        break;
    case decoderStep128:
        Steps = 4;
        Speed = SpeedSet;
        break;
    }

    if (m_LocLib.DirectionGet() == directionForward)
    {
        Speed |= 0x80;
    }

    // Send info and get new data.
    m_XpNet.setLocoDrive(
        (uint8_t)(m_LocLib.GetActualLocAddress() >> 8), (uint8_t)(m_LocLib.GetActualLocAddress()), Steps, Speed);
    m_XpNet.getLocoInfo((uint8_t)(m_LocLib.GetActualLocAddress() >> 8), (uint8_t)(m_LocLib.GetActualLocAddress()));
    m_SkipRequestCnt = 5;
}

/***********************************************************************************************************************
 * Store and sort received loc data.
 */
void xmcApp::StoreAndSortLocDatabaseData(void)
{
    uint16_t Index;
    uint8_t locFunctionAssignment[5] = { 0, 1, 2, 3, 4 };

    m_xmcTft.UpdateStatus("STORING  ", false, WmcTft::color_white);

    for (Index = 0; Index < m_locDbDataCnt; Index++)
    {
        /* If loc not in data base add it... */
        if (m_LocLib.CheckLoc(m_locDbData[Index]) == 255)
        {
            m_LocLib.StoreLoc(
                m_locDbData[Index], locFunctionAssignment, &m_locDbDataName[Index][0], LocLib::storeAddNoAutoSelect);

            /* Show increasing counter. */
            m_xmcTft.UpdateSelectedAndNumberOfLocs(m_LocLib.GetActualSelectedLocIndex(), m_LocLib.GetNumberOfLocs());
        }
    }

    /* If all added sort... */
    m_xmcTft.UpdateStatus("SORTING  ", false, WmcTft::color_white);
    m_LocLib.LocBubbleSort();
    m_xmcTft.UpdateStatus("RESET....", false, WmcTft::color_red);
}

/***********************************************************************************************************************
 * Callback function for system status.
 */
void notifyXNetPower(uint8_t State)
{
    XpNetEvent Event;
    Event.dataType = none;

    switch (State)
    {
    case csNormal: Event.dataType = powerOn; break;
    case csTrackVoltageOff: Event.dataType = powerOff; break;
    case csServiceMode: Event.dataType = programmingMode; break;
    case csEmergencyStop: Event.dataType = powerStop; break;
    }

    if (Event.dataType != none)
    {
        send_event(Event);
    }
}

/***********************************************************************************************************************
 * Callback function for generic requests from XpNet.
 */
void NotifyXNet(uint8_t Data)
{
    XpNetEvent Event;
    Event.dataType = none;

    switch (Data)
    {
    case csLocDataBaseSend: Event.dataType = locDatabaseTransmit; break;
    default: break;
    }

    if (Event.dataType != none)
    {
        send_event(Event);
    }
}

/***********************************************************************************************************************
 * Callback function for loc data.
 */
void notifyLokAll(uint8_t Adr_High, uint8_t Adr_Low, boolean Busy, uint8_t Steps, uint8_t Speed, uint8_t Direction,
    uint8_t F0, uint8_t F1, uint8_t F2, uint8_t F3, boolean Req)
{
    XpNetEvent Event;
    Req = Req;

    Event.dataType      = locdata;
    locData* LocDataPtr = (locData*)(Event.Data);

    LocDataPtr->Address = (uint16_t)(Adr_High) << 8;
    LocDataPtr->Address |= Adr_Low;
    LocDataPtr->Steps = Steps;

    /* Convert speed into readable format based on decoder steps. */
    switch (LocDataPtr->Steps)
    {
    case 0:
        /* 14 speed decoder. */
        LocDataPtr->Speed = Speed;
        break;
    case 1:
        /* 27 speed, not supported. */
        LocDataPtr->Speed = 0;
        break;
    case 2:
        /* 28 Speed decoder. */
        LocDataPtr->Speed = SpeedStep28TableFromDcc[Speed];
        break;
    case 4:
        /* 128 speed decoder. */
        LocDataPtr->Speed = Speed;
        break;
    default: LocDataPtr->Speed = 0; break;
    }

    /* The xpnet lib sends initial decoder steps 3, skip this value... */
    if (LocDataPtr->Steps != 3)
    {
        LocDataPtr->Direction = Direction;
        LocDataPtr->Functions = (F0 & 0x0F) << 1;
        LocDataPtr->Functions |= (F0 & 0x10) >> 4;
        LocDataPtr->Functions |= (uint32_t)(F1) << 5;
        LocDataPtr->Functions |= (uint32_t)(F2) << 13;
        LocDataPtr->Functions |= (uint32_t)(F3) << 21;
        LocDataPtr->Occupied = Busy;

        send_event(Event);
    }
}

/***********************************************************************************************************************
 * Callback function for loc data base data.
 */
void notifyLokDataBaseDataReceive(
    uint8_t Adr_High, uint8_t Adr_Low, uint8_t LocCount, uint8_t NumberOfLocs, char* LocName)
{
    XpNetEvent Event;

    locDatabaseData* LocDatabaseDataPtr = (locDatabaseData*)(Event.Data);
    Event.dataType                      = locDataBase;

    LocDatabaseDataPtr->Address = (uint16)(Adr_High) << 8;
    LocDatabaseDataPtr->Address |= (uint16)(Adr_Low);
    LocDatabaseDataPtr->Number = LocCount;
    LocDatabaseDataPtr->Total  = NumberOfLocs;
    memcpy(LocDatabaseDataPtr->NameStr, LocName, 10);

    send_event(Event);
}

/***********************************************************************************************************************
 * Callback function for cv info.
 */
void notifyCVInfo(uint8_t State)
{
    XpNetEvent Event;

    Event.dataType            = cvResponse;
    cvResponseData* CvDataPtr = (cvResponseData*)(Event.Data);

    switch (State)
    {
    case 0x02:
        /*  Data not found. */
        CvDataPtr->cvInfo = dataNotFound;
        break;
    case 0x01:
        /* �Zentrale Busy� */
        CvDataPtr->cvInfo = centralBusy;
        break;
    case 0x00:
        /* Zentrale Ready� */
        CvDataPtr->cvInfo = centralReady;
        break;
    case 0x03:
        /* short-circuit */
        CvDataPtr->cvInfo = centralShotCircuit;
        break;
    case 0xE1:
        /* Transfer Error */
        CvDataPtr->cvInfo = transmitError;
        break;
    default: CvDataPtr->cvInfo = dataNotFound; break;
    }

    send_event(Event);
}

/***********************************************************************************************************************
 * Callback function for cv data.
 */
void notifyCVResult(uint8_t cvAdr, uint8_t cvData)
{
    XpNetEvent Event;

    Event.dataType            = cvResponse;
    cvResponseData* CvDataPtr = (cvResponseData*)(Event.Data);

    CvDataPtr->cvNumber = cvAdr;
    CvDataPtr->cvValue  = cvData;
    CvDataPtr->cvInfo   = dataReady;

    send_event(Event);
}

/***********************************************************************************************************************
 * Invert the pulse switch delate value if required.
 */
int8_t xmcApp::CheckPulseSwitchRevert(int8_t Delta)
{
    int8_t DeltaResult = Delta;

    if (DeltaResult != 0)
    {
        if (m_PulseSwitchInvert == true)
        {
            DeltaResult *= -1;
        }
    }

    return (DeltaResult);
}
