/*
 * Project: OBD II reader with Arduino
 *
 * Author: Tommi Kangassuo (kangassu@gmail.com)
 *
 * Work for LUT CT10A4000
 */
 
/*
 * SAE J979 and LCD screen header file for the project,
 * including text and message definitions for protocol usage.
 *
 * Contents:
 * 1.   LCD text definitions
 * 2.   J1979 message definitions
 * 2.1  REQ: Number of emission-related DTCs and MIL status
 * 2.2  REQ: Emission-related DTC
 * 2.3  REQ: Clear/reset emission-related diagnostic information
 * 2.4  REQ: PID 24 "O2S1_WR_lambda - bank 2, sensor 1"
 * 2.5  REQ: PID 28 "O2S1_WR_lambda - bank 2, sensor 1"
 * 2.6  REQ: PID 05 "Engine coolant temperature"
 * 2.7  REQ: PID 11 "Throttle position"
 * 2.8  REQ: PID 0C "Engine RPM"
 * 2.9  RESP: Number of emission-related  DTCs and MIL status
 * 2.10 RESP: Emission-related DTC response
 * 2.11 RESP: Clear/reset emission-related diagnostic information
 * 2.12 RESP: Current powertrain diagnostic data response
 * 3. Statemachine definitions
 * 3.1 LCD stm definitions
 * 3.2 J1979 stm definitions
 */

/*
 * 1. LCD text definitions, set to Flash memory with PROGMEM
 */
prog_char lcd_menuitem_wait[] PROGMEM = "Please wait...                  ";
prog_char lcd_menuitem_dtc[] PROGMEM = "Main menu    1/3Read DTCs       ";
prog_char lcd_menuitem_dtc1[] PROGMEM = "DTC 1/  - code: ";
prog_char lcd_menuitem_dtc2[] PROGMEM = "DTC 2/  - code: ";
prog_char lcd_menuitem_dtc3[] PROGMEM = "DTC 3/  - code: ";
prog_char lcd_menuitem_cleardtc[] PROGMEM = "Main menu    2/3Clear DTCs      ";
prog_char lcd_menuitem_pid[] PROGMEM = "Main menu    3/3Read PIDs       ";
prog_char lcd_menuitem_pid1[] PROGMEM = "PID1 - Lambda 1:";
prog_char lcd_menuitem_pid2[] PROGMEM = "PID2 - Lambda 2:";
prog_char lcd_menuitem_pid3[] PROGMEM = "PID3 - Coolant: ";
prog_char lcd_menuitem_pid4[] PROGMEM = "PID4 - Throttle:";
prog_char lcd_menuitem_pid5[] PROGMEM = "PID5 - RPM:     ";
prog_char lcd_menuitem_dtcs_cleared[] PROGMEM = "DTCs cleared!                   ";
prog_char lcd_menuitem_error[] PROGMEM = "Error!                          ";
prog_char lcd_menuitem_empty_string[] PROGMEM = "                                ";
prog_char lcd_menuitem_no_dtcs[] PROGMEM = "No DTCs found.                  ";

/*
 * NOTE: Order in the list matches the order of LCD_STATES,
 * do not change the order nor numbering
 */
PROGMEM const char *menuitems[] =
{
  lcd_menuitem_wait,
  lcd_menuitem_dtc,
  lcd_menuitem_dtc1,
  lcd_menuitem_dtc2,
  lcd_menuitem_dtc3,
  lcd_menuitem_cleardtc,
  lcd_menuitem_pid,
  lcd_menuitem_pid1,
  lcd_menuitem_pid2,
  lcd_menuitem_pid3,
  lcd_menuitem_pid4,
  lcd_menuitem_pid5,
  lcd_menuitem_dtcs_cleared,
  lcd_menuitem_error,
  lcd_menuitem_empty_string,
  lcd_menuitem_no_dtcs
};

/*
 * 2. J1979 message definitions, set to Flash memory with PROGMEM
 */

/*
 * 2.1 Request for 03 Current Powertrain Diagnostic Data Request Message
 *     Definition (PID $01) for 10.4kbit/s SAE J1850 communication
 *
 * Header byte 1 (Priority/Type): 68
 * Header byte 2 (Target address): 6A
 * Header byte 3 (Source address): F1
 * Data byte 1 (Request current powertrain diagnostic data request SID): 01
 * Data byte 2 (PID: Number of emission-related DTCs and MIL status): 01
 */
prog_char j1979_message_nbr_dtc[] PROGMEM = {0x68, 0x6A, 0xF1, 0x01, 0x01};

/*
 * 2.2 Request for 03 Emission-Related DTC Request Message Definition
 *     for 10.4kbit/s SAE J1850 communication
 *
 * Header byte 1 (Priority/Type): 68
 * Header byte 2 (Target address): 6A
 * Header byte 3 (Source address): F1
 * Data byte 1 (Request emission-related DTC request SID): 03
 */
prog_char j1979_message_read_dtc[] PROGMEM = {0x68, 0x6A, 0xF1, 0x03};

/*
 * 2.3  Request for 04 Clear/Reset Emission-Related Diagnostic Information Request Message Definition
 *      for 10.4kbit/s SAE J1850 communication
 *
 * Header byte 1 (Priority/Type): 68
 * Header byte 2 (Target address): 6A
 * Header byte 3 (Source address): F1
 * Data byte 1 (Clear/reset emission-related diagnostic information request SID): 04
 */
prog_char j1979_message_clear_dtc[] PROGMEM = {0x68, 0x6A, 0xF1, 0x04};

/*
 * 2.4  Request for 01 Current powertrain Diagnostic Data Request Message Definition
 *      for 10.4kbit/s SAE J1850 communication, PID 24 "O2S1_WR_lambda - bank 1, sensor 1"
 *
 * Header byte 1 (Priority/Type): 68
 * Header byte 2 (Target address): 6A
 * Header byte 3 (Source address): F1
 * Data byte 1 (Current powertrain Diagnostic Data Request SID): 01
 * Data byte 2 (PID 24 - Lambda bank 1, sensor 1): 24
 */
prog_char j1979_message_read_pid1[] PROGMEM = {0x68, 0x6A, 0xF1, 0x01, 0x24};

/*
 * 2.5  Request for 01 Current powertrain Diagnostic Data Request Message Definition
 *      for 10.4kbit/s SAE J1850 communication, PID 28 "O2S1_WR_lambda - bank 2, sensor 1"
 *
 * Header byte 1 (Priority/Type): 68
 * Header byte 2 (Target address): 6A
 * Header byte 3 (Source address): F1
 * Data byte 1 (Current powertrain Diagnostic Data Request SID): 01
 * Data byte 2 (PID 28 - Lambda bank 2, sensor 1): 28
 */
prog_char j1979_message_read_pid2[] PROGMEM = {0x68, 0x6A, 0xF1, 0x01, 0x28};

/*
 * 2.6  Request for 01 Current powertrain Diagnostic Data Request Message Definition
 *      for 10.4kbit/s SAE J1850 communication, PID 05 "Engine coolant temperature"
 *
 * Header byte 1 (Priority/Type): 68
 * Header byte 2 (Target address): 6A
 * Header byte 3 (Source address): F1
 * Data byte 1 (Current powertrain Diagnostic Data Request SID): 01
 * Data byte 2 (PID 05 - Engine coolant temperature): 05
 */
prog_char j1979_message_read_pid3[] PROGMEM = {0x68, 0x6A, 0xF1, 0x01, 0x05};

/*
 * 2.7  Request for 01 Current powertrain Diagnostic Data Request Message Definition
 *      for 10.4kbit/s SAE J1850 communication, PID 11 "Throttle position"
 *
 * Header byte 1 (Priority/Type): 68
 * Header byte 2 (Target address): 6A
 * Header byte 3 (Source address): F1
 * Data byte 1 (Current powertrain Diagnostic Data Request SID): 01
 * Data byte 2 (PID 11 - Throttle position): 11
 */
prog_char j1979_message_read_pid4[] PROGMEM = {0x68, 0x6A, 0xF1, 0x01, 0x11};

/*
 * 2.8  Request for 01 Current powertrain Diagnostic Data Request Message Definition
 *      for 10.4kbit/s SAE J1850 communication, PID 0C "Engine RPM"
 *
 * Header byte 1 (Priority/Type): 68
 * Header byte 2 (Target address): 6A
 * Header byte 3 (Source address): F1
 * Data byte 1 (Current powertrain Diagnostic Data Request SID): 01
 * Data byte 2 (PID 0C "Engine RPM"): 0C
 */
prog_char j1979_message_read_pid5[] PROGMEM = {0x68, 0x6A, 0xF1, 0x01, 0x0C};

/*
 * 2.9 Response to 03 Current Powertrain Diagnostic Data Request Message
 *     Definition (PID $01) for 10.4kbit/s SAE J1850 communication
 *
 * Header byte 1 (Priority/Type): 48
 * Header byte 2 (Target address): 6B
 * Header byte 3 (Source address): <ECU address>
 * Data byte 1 (Request current powertrain diagnostic data response SID): 41
 * Data byte 2 (PID: Number of emission-related  DTCs and MIL status): 01
 * Data byte 3.. see SAE J2178:
 *   PKT-32-1 Number of Emission-Related Trouble Codes and MIL Status (PRN 0001)
 *   MSB PRN 1000 MIL Status                           1 bit
 *   PRN 1001 Number of Emission-Related Trouble Codes 7 bits
 *   PRN 1002 Continuous Evaluation Supported          8 bits
 *   PRN 1003 Trip Evaluation Supported                8 bits
 *   LSB PRN 1004 Trip Evaluation Complete             8 bits
 */
prog_char j1979_message_nbr_dtc_resp[] PROGMEM = {0x48, 0x6B, 0x00, 0x41, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/*
 * 2.10 Response to 03 Emission-Related DTC Request Message Definition
 *      for 10.4kbit/s SAE J1850 communication
 *
 * Header byte 1 (Priority/Type): 48
 * Header byte 2 (Target address): 6B
 * Header byte 3 (Source address): <ECU address>
 * Data byte 1 (Request emission-related DTC response SID): 43
 * Data byte 2: DTC#1 (High Byte)
 * Data byte 3: DTC#1 (Low Byte)
 * Data byte 4: DTC#2 (High Byte)
 * Data byte 5: DTC#2 (Low Byte)
 * Data byte 6: DTC#3 (High Byte)
 * Data byte 7: DTC#3 (Low Byte)
 */
prog_char j1979_message_read_dtc_resp[] PROGMEM = {0x48, 0x6B, 0x00, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/*
 * 2.11 Response to 04 Clear/Reset Emission-Related Diagnostic Information Response Message Definition
 *      for 10.4kbit/s SAE J1850 communication
 *
 * Header byte 1 (Priority/Type): 48
 * Header byte 2 (Target address): 6B
 * Header byte 3 (Source address): <ECU address>
 * Data byte 1 (Clear/reset emission-related diagnostic information response SID): 44
 */
prog_char j1979_message_clear_dtc_resp[] PROGMEM = {0x48, 0x6B, 0x00, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/*
 * 2.12 Response to 01 Current powertrain Diagnostic Data Response Message Definition
 *      for 10.4kbit/s SAE J1850 communication, all PIDs
 *
 * Header byte 1 (Priority/Type): 48
 * Header byte 2 (Target address): 6B
 * Header byte 3 (Source address): <ECU address>
 * Data byte 1 ( Request current powertrain diagnostic data response SID): 41
 */
prog_char j1979_message_read_pid_resp[] PROGMEM = {0x48, 0x6B, 0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

PROGMEM const char *j1979_messages[] =
{
  j1979_message_nbr_dtc,
  j1979_message_read_dtc,
  j1979_message_clear_dtc,
  j1979_message_read_pid1,
  j1979_message_read_pid2,
  j1979_message_read_pid3,
  j1979_message_read_pid4,
  j1979_message_read_pid5,
  j1979_message_nbr_dtc_resp,
  j1979_message_read_dtc_resp,
  j1979_message_clear_dtc_resp,
  j1979_message_read_pid_resp
};

/*
 * 3. Statemachine definitions
 */

/*
 * 3.1 LCD stm definitions
 */
/*
 * LCD state definitions
 */
typedef enum
{
  LCD_INVALID_STATE = 0,
  LCD_DTC_STATE,
  LCD_DTC1_STATE,
  LCD_DTC2_STATE,
  LCD_DTC3_STATE,
  LCD_DTC4_STATE,
  LCD_DTC5_STATE,
  LCD_CLEAR_DTC_STATE,
  LCD_PID_STATE,
  LCD_PID1_STATE,
  LCD_PID2_STATE,
  LCD_PID3_STATE,
  LCD_PID4_STATE,
  LCD_PID5_STATE
} LCD_STATE;

/*
 * LCD input definitions
 */
typedef enum
{
  LCD_INVALID_INPUT = 0,
  LCD_BUTTON_SELECT,
  LCD_BUTTON_NEXT,
  LCD_BUTTON_EXIT,
  LCD_TIMER_ELAPSED,
  LCD_RESULT,
  LCD_NO_RESULT
} LCD_INPUT;

/*
 * LCD output definitions
 */
typedef enum
{
  LCD_INVALID_OUTPUT = 0,
  LCD_WAIT_RESP,
  LCD_TIMER_START,
  LCD_REFR_TIMER_START,
  LCD_MSG_SUCCESS
} LCD_OUTPUT;

/*
 * 3.2 J1979 stm definitions
 */
/*
 * J1979 message decoding types
 */
typedef enum
{
  J1979_APP_DEC_INVALID_TYPE = 0,
  J1979_APP_DEC_NBR_OF_DTCS,
  J1979_APP_DEC_DTCS,
  J1979_APP_DEC_CLEAR_DTCS,
  J1979_APP_DEC_PID1,
  J1979_APP_DEC_PID2,
  J1979_APP_DEC_PID3,
  J1979_APP_DEC_PID4,
  J1979_APP_DEC_PID5
} J1979_APP_DECODER_TYPE;
