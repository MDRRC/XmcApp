/**
 **********************************************************************************************************************
 * @file  xmc_event.h
 * @brief Collection of event data and events fo the WMC application.
 ***********************************************************************************************************************
 */
#ifndef XMC_EVENT_H
#define XMC_EVENT_H

/***********************************************************************************************************************
 * I N C L U D E S
 **********************************************************************************************************************/
#include <tinyfsm.hpp>

/***********************************************************************************************************************
 * T Y P E D  E F S  /  E N U M
 **********************************************************************************************************************/

/**
 * Status of the pulse switch.
 */
enum pulseSwitchStatus
{
    turn = 0,
    pushturn,
    pushedShort,
    pushedNormal,
    pushedlong,
    released,
};

/**
 * Definitions for the push buttons.
 */
enum pushButtons
{
    button_0 = 0,
    button_1,
    button_2,
    button_3,
    button_4,
    button_5,
    button_power,
    button_none
};

/**
 * CV programming module events.
 */
enum cvProgRequest
{
    cvRead = 0,
    cvWrite,
    cvStatusRequest,
    pomWrite,
    cvExit,
};

enum xpNetDataType
{
    none = 0,
    powerOn,
    powerOff,
    powerStop,
    programmingMode,
    locdata,
    cvResponse,
    locDataBase,
    locDatabaseTransmit,
};

enum xpCvInfo
{
    dataNotFound = 0,
    centralBusy,
    centralReady,
    centralShotCircuit,
    transmitError,
    dataReady,
};

/**
 * Struct for CV reponse data.
 */
struct cvResponseData
{
    uint16_t cvNumber;
    uint8_t cvValue;
    xpCvInfo cvInfo;
};

/**
 * Strucv with loc response data.
 */
struct locData
{
    uint16_t Address;
    uint8_t Steps;
    uint8_t Speed;
    uint8_t Direction;
    uint32_t Functions;
    bool Occupied;
};

/**
 * Struct with loc data base data
 */
struct locDatabaseData
{
    uint16_t Address;
    uint16_t Number;
    uint16_t Total;
    char NameStr[10];
};

/**
 * Pulse switch event.
 */
struct pulseSwitchEvent : tinyfsm::Event
{
    int8_t Delta;             /* Delta of pulsw switch. */
    pulseSwitchStatus Status; /* Status */
};

/**
 * Event for buttons.
 */
struct pushButtonsEvent : tinyfsm::Event
{
    pushButtons Button; /* Button which was pressed. */
};

/**
 * XpNet Update event
 */
struct xpNetEventUpdate : tinyfsm::Event
{
};

/**
 * XpNet event
 */
struct XpNetEvent : tinyfsm::Event
{
    xpNetDataType dataType;
    uint8_t Data[20];
};

/**
 * 100msec Update event
 */
struct updateEvent100msec : tinyfsm::Event
{
};

/**
 * 500msec Update event
 */
struct updateEvent500msec : tinyfsm::Event
{
};

/**
 * 3 Seconds ipdate event
 */
struct updateEvent3sec : tinyfsm::Event
{
};

/**
 * Data changed by command line interface event.
 */
struct cliEnterEvent : tinyfsm::Event
{
};

/**
 * CV programming events from cv module.
 */
struct cvProgEvent : tinyfsm::Event
{
    cvProgRequest Request;
    uint16_t Address;
    uint16_t CvNumber;
    uint8_t CvValue;
};

/***********************************************************************************************************************
 * C L A S S E S
 **********************************************************************************************************************/

#endif
