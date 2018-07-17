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
xmcApp::powerStatus xmcApp::m_PowerStatus  = off;
bool xmcApp::m_LocSelection                = false;
uint8_t xmcApp::m_XpNetAddress             = 0;
uint8_t xmcApp::m_ConnectCount             = 0;
uint8_t xmcApp::m_SkipRequestCnt           = 0;
uint16_t xmcApp::m_TurnOutAddress          = 1;
uint8_t xmcApp::m_TurnOutDirection         = 0;
uint32_t xmcApp::m_TurnoutOffDelay         = 0;
bool xmcApp::m_CvPomProgrammingFromPowerOn = false;
bool xmcApp::m_CvPomProgramming            = false;
uint16_t xmcApp::m_locAddressAdd           = 1;
uint16_t xmcApp::m_locAddressChange        = 1;
uint16_t xmcApp::m_locAddressDelete        = 1;
uint8_t xmcApp::m_locFunctionAdd           = 0;
uint8_t xmcApp::m_locFunctionChange        = 0;

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
class stateTurnoutControl;
class stateTurnoutControlPowerOff;
class stateMainMenu;
class stateMenuLocAdd;
class stateMenuLocFunctionsAdd;
class stateMenuLocFunctionsChange;
class stateMenuLocDelete;
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
        m_LocStorage.Init();
        m_LocLib.Init(m_LocStorage);
        m_WmcCommandLine.Init(m_LocLib, m_LocStorage);
        m_ConnectCount = 0;
    }

    /**
     * 3 Seconds timeout
     */
    void react(updateEvent3sec const&) override
    {
        /* Some delay so app name is displayed after power on. */
        m_ConnectCount++;
        if (m_ConnectCount > 1)
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
            if (e.Delta > 0)
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
            else if (e.Delta < 0)
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
            nvic_sys_reset();
            break;
        case pushedShort: break;
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
    void entry() override { m_XpNet.start(m_XpNetAddress, PB0); }

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
     * Wait for "connection". If 500msec timer expires we did not get a (fast) response and we assume the
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
        case programmingMode:
        case locdata: break;
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
        case powerStop:
        case programmingMode: break;
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
            }
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
        m_PowerStatus = powerStatus::off;
        m_xmcTft.UpdateStatus("POWER OFF", false, WmcTft::color_red);
        m_xmcTft.UpdateSelectedAndNumberOfLocs(m_LocLib.GetActualSelectedLocIndex(), m_LocLib.GetNumberOfLocs());

        /* Stop loc and update info on screen. */
        m_LocLib.SpeedSet(0);
        m_LocDataReceived.Speed = 0;
        updateLocInfoOnScreen(false);
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
            m_XpNet.getLocoInfo(
                (uint8_t)(m_LocLib.GetActualLocAddress() >> 8), (uint8_t)(m_LocLib.GetActualLocAddress()));
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
        case powerOn: transit<statePowerOn>(); break;
        case powerOff:

        case powerStop: break;
        case locdata:
            LocDataPtr = (locData*)(e.Data);

            // Roco Multimaus keeps transmitting set speed... Force zero speed.
            LocDataPtr->Speed = 0;
            memcpy(&m_LocDataReceived, LocDataPtr, sizeof(locData));
            updateLocInfoOnScreen(false);
            break;
        case programmingMode: break;
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
            if (e.Delta != 0)
            {
                m_LocLib.GetNextLoc(e.Delta);
                m_XpNet.getLocoInfo(
                    (uint8_t)(m_LocLib.GetActualLocAddress() >> 8), (uint8_t)(m_LocLib.GetActualLocAddress()));
                m_xmcTft.UpdateSelectedAndNumberOfLocs(
                    m_LocLib.GetActualSelectedLocIndex(), m_LocLib.GetNumberOfLocs());
                m_LocSelection   = true;
                m_SkipRequestCnt = 2;
            }
            break;
        case pushedShort:
            /* Power on request. */
            m_XpNet.setPower(csNormal);
            break;
        case pushedlong: transit<stateMainMenu>(); break;
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
        m_PowerStatus = powerStatus::on;
        m_xmcTft.UpdateStatus("POWER ON ", false, WmcTft::color_green);
        m_xmcTft.UpdateSelectedAndNumberOfLocs(m_LocLib.GetActualSelectedLocIndex(), m_LocLib.GetNumberOfLocs());
        m_SkipRequestCnt = 0;
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
            m_XpNet.getLocoInfo(
                (uint8_t)(m_LocLib.GetActualLocAddress() >> 8), (uint8_t)(m_LocLib.GetActualLocAddress()));
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
            LocDataPtr = (locData*)(e.Data);
            memcpy(&m_LocDataReceived, LocDataPtr, sizeof(locData));
            updateLocInfoOnScreen(false);
            break;
        case programmingMode: break;
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

            if (m_LocLib.SpeedSet(e.Delta) == true)
            {
                // Transmit loc speed etc. data.
                preparAndTransmitLocoDriveCommand();
                m_SkipRequestCnt = 2;
            }
            break;
        case pushturn:
            /* Select next or previous loc. */
            if (e.Delta != 0)
            {
                m_LocLib.GetNextLoc(e.Delta);
                m_XpNet.getLocoInfo(
                    (uint8_t)(m_LocLib.GetActualLocAddress() >> 8), (uint8_t)(m_LocLib.GetActualLocAddress()));
                m_xmcTft.UpdateSelectedAndNumberOfLocs(
                    m_LocLib.GetActualSelectedLocIndex(), m_LocLib.GetNumberOfLocs());
                m_LocSelection   = true;
                m_SkipRequestCnt = 2;
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
            preparAndTransmitLocoDriveCommand();
            m_SkipRequestCnt = 2;
            break;
        case pushedNormal:
            /* Change direction nad transmit loc data.. */
            m_LocLib.DirectionToggle();
            preparAndTransmitLocoDriveCommand();
            m_SkipRequestCnt = 2;
            break;
        case pushedlong:
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
        case button_power: m_XpNet.setPower(csTrackVoltageOff); break;
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
     * Show turnout screen.
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
        case programmingMode: break;
        }
    }

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
 * Turnout control.
 */
class stateTurnoutControl : public xmcApp
{
    /**
     * Show turnout screen.
     */
    void entry() override
    {
        m_TurnOutDirection = 0;

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
        if (m_TurnOutDirection != 0)
        {
            m_TurnOutDirection = 0;
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
        case programmingMode: break;
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
            if (e.Delta > 0)
            {
                /* Increase address and check for overrrun. */
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
            else if (e.Delta < 0)
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
            /* Back to loc control. */
            transit<stateGetLocData>();
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
            m_TurnOutDirection = 1;
            m_TurnoutOffDelay  = millis();
            updateScreen       = false;
            sentTurnOutCommand = true;
            break;
        case button_5:
            m_TurnOutDirection = 2;
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
            if (m_TurnOutDirection == 1)
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
        if (m_TurnOutDirection != 0)
        {
            m_TurnOutDirection = 0;
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
        case programmingMode: break;
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
 * Show main menu and handle the request.
 */
class stateMainMenu : public xmcApp
{
    /**
     * Show menu on screen.
     */
    void entry() override
    {
        m_xmcTft.ShowMenu();
        m_XpNet.setPower(csTrackVoltageOff);
    };

    /**
     * Handle pulse switch events.
     */
    void react(pulseSwitchEvent const& e) override
    {
        switch (e.Status)
        {
        case turn:
        case pushturn: break;
        case pushedShort:
        case pushedNormal:
        case pushedlong:
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
            // transit<stateCvProgramming>();
            break;
        case button_5:
            m_CvPomProgramming = true;
            // transit<stateCvProgramming>();
            break;
        case button_power:
            m_LocSelection = true;
            transit<stateGetPowerStatus>();
            break;
        case button_0:
            // Set invalid XpNet device address and go to Xp address menu.
            m_LocStorage.XpNetAddressSet(255);
            transit<stateCheckXpNetAddress>();
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
            if (e.Delta > 0)
            {
                m_locAddressAdd++;
                m_locAddressAdd = m_LocLib.limitLocAddress(m_locAddressAdd);
                m_xmcTft.ShowlocAddress(m_locAddressAdd, WmcTft::color_green);
            }
            else if (e.Delta < 0)
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
            transit<stateMainMenu>();
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
            if (e.Delta > 0)
            {
                m_locFunctionAdd++;
                if (m_locFunctionAdd > FUNCTION_MAX)
                {
                    m_locFunctionAdd = FUNCTION_MIN;
                }
                m_xmcTft.FunctionAddUpdate(m_locFunctionAdd);
            }
            else if (e.Delta < 0)
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
            m_LocLib.StoreLoc(m_locAddressAdd, m_locFunctionAssignment, LocLib::storeAdd);
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
        case button_power: transit<stateMainMenu>(); break;
        case button_5:
            /* Store loc functions */
            m_LocLib.StoreLoc(m_locAddressAdd, m_locFunctionAssignment, LocLib::storeAdd);
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
        m_locFunctionChange = 0;
        m_locAddressChange  = m_LocLib.GetActualLocAddress();
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
            if (e.Delta > 0)
            {
                m_locFunctionChange++;
                if (m_locFunctionChange > FUNCTION_MAX)
                {
                    m_locFunctionChange = FUNCTION_MIN;
                }
                m_xmcTft.FunctionAddUpdate(m_locFunctionChange);
            }
            else if (e.Delta < 0)
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
            m_locAddressChange = m_LocLib.GetNextLoc(e.Delta);
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
            m_LocLib.StoreLoc(m_locAddressChange, m_locFunctionAssignment, LocLib::storeChange);
            m_xmcTft.ShowlocAddress(m_locAddressChange, WmcTft::color_yellow);
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
            /* Rest of buttons for oher functions except light. */
            if (m_locFunctionChange != 0)
            {
                m_locFunctionAssignment[static_cast<uint8_t>(e.Button)] = m_locFunctionChange;
                m_xmcTft.UpdateFunction(
                    static_cast<uint8_t>(e.Button), m_locFunctionAssignment[static_cast<uint8_t>(e.Button)]);
            }
            break;
        case button_power: transit<stateMainMenu>(); break;
        case button_5:
            /* Store changed data and yellow text indicating data is stored. */
            m_LocLib.StoreLoc(m_locAddressChange, m_locFunctionAssignment, LocLib::storeChange);
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
        switch (e.Status)
        {
        case turn:
            /* Select loc to be deleted. */
            m_locAddressDelete = m_LocLib.GetNextLoc(e.Delta);
            m_xmcTft.UpdateSelectedAndNumberOfLocs(m_LocLib.GetActualSelectedLocIndex(), m_LocLib.GetNumberOfLocs());
            m_xmcTft.ShowlocAddress(m_locAddressDelete, WmcTft::color_green);
            break;
        case pushedNormal:
        case pushedlong:
            /* Remove loc. */
            m_LocLib.RemoveLoc(m_locAddressDelete);
            m_xmcTft.UpdateSelectedAndNumberOfLocs(m_LocLib.GetActualSelectedLocIndex(), m_LocLib.GetNumberOfLocs());
            m_locAddressDelete = m_LocLib.GetActualLocAddress();
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
        case button_power: transit<stateMainMenu>(); break;
        case button_none: break;
        }
    };
};

/***********************************************************************************************************************
 * Command lie interface active state.
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
            m_xmcTft.UpdateStatus("CV programming", true, WmcTft::color_green);
        }
        else
        {
            EventCv.EventData = startPom;
            m_xmcTft.UpdateStatus("POM programming", true, WmcTft::color_green);
            // m_z21Slave.LanSetTrackPowerOn();
        }

        send_event(EventCv);
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
        case programmingMode: break;
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
            Event.EventData.Delta  = e.Delta;
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
        cvEvent EventCv;

        /* Handle menu request. */
        switch (e.Button)
        {
        case button_0:
        case button_1:
        case button_2:
        case button_3:
        case button_4:
        case button_5:
            /* Forward event.*/
            Event.EventData.Button = e.Button;
            send_event(Event);
            break;
        case button_power:
            EventCv.EventData = stop;
            send_event(EventCv);
            if (m_CvPomProgrammingFromPowerOn == false)
            {
                transit<stateMainMenu>();
            }
            else
            {
                transit<stateGetLocData>();
            }
            break;
        case button_none: break;
        }
    };

    /**
     * Handle events from the cv state machine.
     */
    void react(cvProgEvent const& e) override
    {
        cvEvent EventCv;

        switch (e.Request)
        {
        case cvRead: m_XpNet.readCVMode(e.CvNumber); break;
        case cvWrite: m_XpNet.writeCVMode(e.CvNumber, e.CvValue); break;
        case pomWrite: break;
        case cvExit:
            EventCv.EventData = stop;
            send_event(EventCv);
            if (m_CvPomProgrammingFromPowerOn == false)
            {
                transit<stateMainMenu>();
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

        // Convert speed.
        switch (m_LocDataReceived.Direction)
        {
        case 0: m_LocLib.DirectionSet(directionBackWard); break;
        case 1: m_LocLib.DirectionSet(directionForward); break;
        default: m_LocLib.DirectionSet(directionForward); break;
        }

        // Set function data.
        m_LocLib.FunctionUpdate(m_LocDataReceived.Functions);

        // Convert decoder steps.
        switch (m_LocDataReceived.Steps)
        {
        case 0: m_LocLib.DecoderStepsUpdate(decoderStep14); break;
        case 2: m_LocLib.DecoderStepsUpdate(decoderStep28); break;
        case 4: m_LocLib.DecoderStepsUpdate(decoderStep128); break;
        default: m_LocLib.DecoderStepsUpdate(decoderStep28); break;
        }

        // Get function assignment of loc.
        for (Index = 0; Index < 5; Index++)
        {
            m_locFunctionAssignment[Index] = m_LocLib.FunctionAssignedGet(Index);
        }

        /* Invert functions so function symbols are updated if new loc is selected and set new
         * direction. */
        if (m_LocSelection == true)
        {
            m_LocDataRecievedPrevious.Functions = ~m_LocDataReceived.Functions;
            m_LocSelection                      = false;
        }

        // Convert data for display.
        convertLocDataToDisplayData(&m_LocDataReceived, &locInfoActual);
        convertLocDataToDisplayData(&m_LocDataRecievedPrevious, &locInfoPrevious);
        m_xmcTft.UpdateLocInfo(&locInfoActual, &locInfoPrevious, m_locFunctionAssignment, updateAll);

        memcpy(&m_LocDataRecievedPrevious, &m_LocDataReceived, sizeof(locData));
    }
}

/***********************************************************************************************************************
 * Prepare and transmit loco drive command
 */
void xmcApp::preparAndTransmitLocoDriveCommand(void)
{
    uint8_t Steps = 0;
    uint8_t Speed = 0;
    // Determine decoder speed step.
    switch (m_LocLib.DecoderStepsGet())
    {
    case decoderStep14:
        Steps = 0;
        Speed = m_LocLib.SpeedGet();
        break;
    case decoderStep28:
        Steps = 2;
        Speed = SpeedStep28TableToDcc[m_LocLib.SpeedGet()];
        break;
    case decoderStep128:
        Steps = 4;
        Speed = m_LocLib.SpeedGet();
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
 * Callback function for loc data.
 */
void notifyLokAll(uint8_t Adr_High, uint8_t Adr_Low, boolean Busy, uint8_t Steps, uint8_t Speed, uint8_t Direction,
    uint8_t F0, uint8_t F1, uint8_t F2, uint8_t F3, boolean Req)
{
    XpNetEvent Event;

    /* Conversion table for 28 steps DCC speed to normal speed. */
    const uint8_t SpeedStep28TableFromDcc[32] = { 0, 0, 1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 0, 0, 2, 4,
        6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28 };

    Busy = Busy;
    Req  = Req;

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
        LocDataPtr->Occupied = false;

        send_event(Event);
    }
}
