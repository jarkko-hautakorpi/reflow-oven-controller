#ifndef REFLOW_COMMANDS_H
#define REFLOW_COMMANDS_H

// Commands to send to MCU board
enum reflow_commands {
    COMMAND_NO_COMMAND,
    COMMAND_STOP_ALL,
    COMMAND_START_DEFAULT_REFLOW,
    COMMAND_GET_TEMPERATURE,
    COMMAND_RELAY_ON,
    COMMAND_RELAY_OFF
    };

// Messages that MCU board sends back (answers, replies etc.)
enum reflow_responsemessages {
    RESPONSE_TEMPERATURE,
    RESPONSE_RELAYSTATES
    };





#endif
