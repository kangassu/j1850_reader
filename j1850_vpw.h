/*
 * Project: OBD II reader with Arduino
 *
 * Author: Tommi Kangassuo (kangassu@gmail.com)
 *
 * Work for LUT CT10A4000
 */
 
/*
 * SAE J1850 VPW and J1979 protocol header file for the project,
 * including common definitions for protocol usage.
 *
 * Contents:
 * 1. Common definitions
 * 2. Interface definitions
 * 2.1 Phys Service IDs
 * 2.2 Data Link Service IDs
 * 2.3 Application layer service IDs
 * 2.4 Common Interface definitions
 * 2.5 Interface function definitions
 */

/*
 * 1. Common definitions
 */

/*
 * Function return values
 */
#define SUCCESS 1
#define FAILURE 0

/*
 * J8150 message constant for maximum number of symbols:
 * BRK + IFS + SOF + DATA (12 * 8 bits) + CRC + IFS
 */
#define J1850_MAX_MSG_SYMBOL_COUNT 108

/*
 * Port (pin) number for J1850 reception
 */
#define J1850_RECEIVE_PORT 3

/* 
 * Port (pin) number for J1850 transmission
 */
#define J1850_TRANSMIT_PORT 4

/*
 * timer type definitions
 */
typedef enum
{
  J1850_INVALID_TIMER_TYPE = 0,
  J1850_REG_EVENT_TIMER,
  J1850_RX_TIMER
} J1850_TIMER_TYPE;

/*
 * J1850 VPW timer constants
 */
#define J1850_VPW_TIMER_PRESCALER 64
#define J1850_VPW_TIMER_CTC_IN_US 64
#define J1850_VPW_TIMER_CTC_SHIFT_VAL 6
#define J1850_VPW_TIMER_RX_SHIFT_VAL 2
 
/*
 * Declare J1850 specific entities
 */
typedef enum
{
  J1850_INVALID_ENTITY = 0,
  J1850_VPW_RX,
  J1850_VPW_TX,
  J1850_DL,
  J1979_APP
} J1850_ENTITY_ID;

/*
 * Global OBD2 message array
 */
static char obd2_message[ 12 ];

/*
 * 2. Interface definitions
 */

/* 
 * 2.1 Phys Service IDs
 */
typedef enum
{
  PHYS_INVALID_SERVICE = 0,
  PHYS_SEND_SYMBOL,
  PHYS_CANCEL_SYMBOL,
  PHYS_START_LISTEN,
  PHYS_STOP_LISTEN,
  PHYS_TIMER_ELAPSED
} PHYS_SERVICE_ID;

/*
 * 2.2 Data Link Service IDs
 */
typedef enum
{
  DATA_LINK_INVALID_SERVICE = 0,
  DATA_LINK_RECV_SYMBOL,
  DATA_LINK_SEND_MESSAGE,
  DATA_LINK_CANCEL_MESSAGE
} DATA_LINK_SERVICE_ID;

/*
 * 2.3 Application layer service IDs
 */
typedef enum
{
  APP_INVALID_SERVICE = 0,
  APP_MESSAGE_RECEIVED,
  APP_MESSAGE_SENT,
  APP_BUTTON_PRESSED,
  APP_TIMER_ELAPSED
} APP_SERVICE_ID;
 
/*
 * 2.4 Common Interface definitions
 */

/*
 * Bit definitions and lengths:
 * IFS (Inter Frame Separation)
 * BRK (Break symbol)
 * BIT_ONE (1)
 * BIT_ZERO (0)
 * SOF (Start Of Frame)
 * EOD (End Of Data)
 * EOF (End Of Frame)
 */
typedef enum
{
  J1850_BIT_ZERO_SYMBOL = 0,
  J1850_BIT_ONE_SYMBOL,
  J1850_SOF_SYMBOL,
  J1850_EOD_SYMBOL,
  J1850_EOF_SYMBOL,
  J1850_IFS_SYMBOL,
  J1850_BRK_SYMBOL,
  J1850_INVALID_SYMBOL
} J1850_SYMBOL_ID;
  
#define J1850_BIT_ZERO_LOW_LEN    64
#define J1850_BIT_ZERO_HIGH_LEN  128
#define J1850_BIT_ONE_LOW_LEN    128
#define J1850_BIT_ONE_HIGH_LEN    64
#define J1850_IFS_LEN            300
#define J1850_SOF_LEN            200
#define J1850_EOF_LEN            280
#define J1850_EOD_LEN            200
#define J1850_BRK_LEN            300

/*
 * 2.5 Interface function definitions
 */
void j1850_vpw_rx_input_msg( PHYS_SERVICE_ID service, int data );
void j1850_vpw_tx_input_msg( PHYS_SERVICE_ID service, int data );
void j1850_dl_input_msg( DATA_LINK_SERVICE_ID service, int data );
void j1979_app_input_msg( APP_SERVICE_ID service, int data );
void ( *j1850_vpw_stm_func ) ( J1850_ENTITY_ID, PHYS_SERVICE_ID, int );
void ( *j1850_dl_stm_func ) ( DATA_LINK_SERVICE_ID, int );
void ( *j1979_app_stm_func ) ( APP_SERVICE_ID, int );
void obd2_messaging_init( void );
void j1979_app_init( void );
void j1850_dl_init( void );
void j1850_vpw_init( void );
void j1850_timer_init( void );
void j1850_tx_timer_request( unsigned int timeout_value );
void j1850_tx_timer_next_pending( unsigned int is_high );
void j1850_rx_timer_request( unsigned int timeout_value );
void j1850_timer_remove_entity( J1850_ENTITY_ID entity );
