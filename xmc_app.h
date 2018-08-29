/**
 **********************************************************************************************************************
 * @file  xmc_app.h
 * @brief Main application of the XpressNet manual control.
 ***********************************************************************************************************************
 */
#ifndef XMC_APP_H
#define XMC_APP_H

/***********************************************************************************************************************
 * I N C L U D E S
 **********************************************************************************************************************/
#include "LocStorage.h"
#include "Loclib.h"
#include "WmcCli.h"
#include "WmcTft.h"
#include "XpressNet.h"
#include "tinyfsm.hpp"
#include "xmc_event.h"

/***********************************************************************************************************************
 * T Y P E D  E F S  /  E N U M
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * C L A S S E S
 **********************************************************************************************************************/
class xmcApp : public tinyfsm::Fsm<xmcApp>
{
public:
    /**
     * Enum with application power status.
     */
    enum powerStatus
    {
        off = 0,
        on,
        emergency,
        progMode,
    };

    /* default reaction for unhandled events */
    void react(tinyfsm::Event const&){};

    virtual void react(XpNetEvent const&);
    virtual void react(cliEnterEvent const&);
    virtual void react(xpNetEventUpdate const&);
    virtual void react(updateEvent3sec const&);
    virtual void react(pushButtonsEvent const&);
    virtual void react(pulseSwitchEvent const&);
    virtual void react(updateEvent100msec const&);
    virtual void react(updateEvent500msec const&);
    virtual void react(cvProgEvent const&);

    virtual void entry(void){}; /* entry actions in some states */
    virtual void exit(void){};  /* no exit actions at all */

    void convertLocDataToDisplayData(locData* XpDataPtr, WmcTft::locoInfo* TftDataPtr);
    void updateLocInfoOnScreen(bool updateAll);
    void preparAndTransmitLocoDriveCommand(void);
    void StoreAndSortLocDatabaseData(void);

protected:
    static WmcTft m_xmcTft;
    static LocLib m_LocLib;
    static XpressNetClass m_XpNet;
    static LocStorage m_LocStorage;
    static WmcCli m_WmcCommandLine;
    static uint8_t m_XpNetAddress;
    static uint8_t m_ConnectCount;
    static powerStatus m_PowerStatus;
    static bool m_LocSelection;
    static locData m_LocDataReceived;
    static locData m_LocDataRecievedPrevious;
    static uint8_t m_locFunctionAssignment[5];
    static uint8_t m_SkipRequestCnt;
    static WmcTft::locoInfo locInfoActual;
    static WmcTft::locoInfo locInfoPrevious;
    static uint16_t m_TurnOutAddress;
    static uint8_t m_TurnOutDirection;
    static uint32_t m_TurnoutOffDelay;
    static bool m_CvPomProgramming;
    static bool m_CvPomProgrammingFromPowerOn;
    static uint16_t m_locAddressAdd;
    static uint16_t m_locAddressChange;
    static uint16_t m_locAddressDelete;
    static uint8_t m_locFunctionAdd;
    static uint8_t m_locFunctionChange;

    static uint16_t m_locDbData[200];
    static uint16_t m_locDbDataCnt;

    static const uint16_t ADDRESS_TURNOUT_MIN = 1;
    static const uint16_t ADDRESS_TURNOUT_MAX = 9999;
    static const uint8_t FUNCTION_MIN         = 0;
    static const uint8_t FUNCTION_MAX         = 28;

    /* Conversion table for normal speed to 28 steps DCC speed. */
    const uint8_t SpeedStep28TableToDcc[29] = { 16, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23, 8, 24, 9, 25, 10, 26, 11,
        27, 12, 28, 13, 29, 14, 30, 15, 31 };
};
#endif
