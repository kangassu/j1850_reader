/*
 * Project: OBD II reader with Arduino
 *
 * Author: Tommi Kangassuo (kangassu@gmail.com)
 *
 * Work for LUT CT10A4000
 */
 
/*
 * Messaging entity for OBD2 reader protocol messaging and prioritization
 *
 * Contents:
 * 1. Inclusions
 * 2. Constants and global variables
 * 3. Functions
 * 3.1 obd2_messaging_init
 * 3.2 j1850_vpw_rx_input_msg
 * 3.3 j1850_vpw_tx_input_msg
 * 3.4 j1850_dl_input_msg
 * 3.5 j1979_app_input_msg
 * 3.6 j1850_check_queue
 */
 
/*
 * 1. Inclusions
 */
 
/*
 * 2. Constants and global variables
 */
  
/*
 * Message inbox for entities, supports queuing of two messages for
 * same receiving entity
 */
volatile PHYS_SERVICE_ID msg_inbox_j1850_vpw_rx_service[ 2 ];
volatile uint8_t msg_inbox_j1850_vpw_rx_data[ 2 ];
volatile uint8_t msg_inbox_j1850_vpw_rx_msg_count;
volatile PHYS_SERVICE_ID msg_inbox_j1850_vpw_tx_service[ 2 ];
volatile uint8_t msg_inbox_j1850_vpw_tx_data[ 2 ];
volatile uint8_t msg_inbox_j1850_vpw_tx_msg_count;
volatile DATA_LINK_SERVICE_ID msg_inbox_j1850_dl_service[ 2 ];
volatile uint8_t msg_inbox_j1850_dl_data[ 2 ];
volatile uint8_t msg_inbox_j1850_dl_msg_count;
volatile APP_SERVICE_ID msg_inbox_j1979_app_service[ 2 ];
volatile uint8_t msg_inbox_j1979_app_data[ 2 ];
volatile uint8_t msg_inbox_j1979_app_msg_count;

/*
 * 3. Functions
 */

/*
 * 3.1 obd2_messaging_init
 *
 * Description: OBD2 reader messaging service init
 *
 * input: void
 * output: void
 * Function resource consumption: 2 us
 */
void obd2_messaging_init( void )
{
  msg_inbox_j1850_vpw_tx_service[ 0 ] = PHYS_INVALID_SERVICE;
  msg_inbox_j1850_vpw_tx_service[ 1 ] = PHYS_INVALID_SERVICE;
  msg_inbox_j1850_vpw_tx_msg_count = 0;
  msg_inbox_j1850_vpw_rx_service[ 0 ] = PHYS_INVALID_SERVICE;
  msg_inbox_j1850_vpw_rx_service[ 1 ] = PHYS_INVALID_SERVICE;
  msg_inbox_j1850_vpw_rx_msg_count = 0;
  msg_inbox_j1850_dl_service[ 0 ] = DATA_LINK_INVALID_SERVICE;
  msg_inbox_j1850_dl_service[ 1 ] = DATA_LINK_INVALID_SERVICE;
  msg_inbox_j1850_dl_msg_count = 0;
  msg_inbox_j1979_app_service[ 0 ] = APP_INVALID_SERVICE;
  msg_inbox_j1979_app_service[ 1 ] = APP_INVALID_SERVICE;
  msg_inbox_j1979_app_msg_count = 0;
}

/*
 * 3.2 j1850_vpw_rx_input_msg
 *
 * Description: Message input for VPW RX messages
 *
 * input: service - requested service
 *        data - service specific data
 * output: void
 * Function resource consumption: 2.5 us
 */
void j1850_vpw_rx_input_msg( PHYS_SERVICE_ID service, int data )
{
  msg_inbox_j1850_vpw_rx_service[ msg_inbox_j1850_vpw_rx_msg_count ] = service;
  msg_inbox_j1850_vpw_rx_data[ msg_inbox_j1850_vpw_rx_msg_count ] = data;
  msg_inbox_j1850_vpw_rx_msg_count++;
}

/*
 * 3.3 j1850_vpw_tx_input_msg
 *
 * Description: Message input for VPW TX messages
 *
 * input: service - requested service
 *        data - service specific data
 * output: void
 * Function resource consumption: 2.5 us
 */
void j1850_vpw_tx_input_msg( PHYS_SERVICE_ID service, int data )
{
  msg_inbox_j1850_vpw_tx_service[ msg_inbox_j1850_vpw_tx_msg_count ] = service;
  msg_inbox_j1850_vpw_tx_data[ msg_inbox_j1850_vpw_tx_msg_count ] = data;
  msg_inbox_j1850_vpw_tx_msg_count++;
}

/*
 * 3.4 j1850_dl_input_msg
 *
 * Description: Message input for Data Link messages
 *
 * input: service - requested service
 *        data - service specific data
 * Function resource consumption: 2.5 us
 */
void j1850_dl_input_msg( DATA_LINK_SERVICE_ID service, int data )
{
  msg_inbox_j1850_dl_service[ msg_inbox_j1850_dl_msg_count ] = service;
  msg_inbox_j1850_dl_data[ msg_inbox_j1850_dl_msg_count ] = data;
  msg_inbox_j1850_dl_msg_count++;
}

/*
 * 3.5 j1979_app_input_msg
 *
 * Description: Message input for Application layer messages
 *
 * input: service - requested service
 *        data - service specific data
 * Function resource consumption: 2.5 us
 */
void j1979_app_input_msg( APP_SERVICE_ID service, int data )
{
  msg_inbox_j1979_app_service[ msg_inbox_j1979_app_msg_count ] = service;
  msg_inbox_j1979_app_data[ msg_inbox_j1979_app_msg_count ] = data;
  msg_inbox_j1979_app_msg_count++;
}

/*
 * 3.6 j1850_check_queue
 *
 * Description: Checks the message queue in task priority order.
 *   This function implements the priorization between entities.
 *
 * input: void
 * output: SUCCESS (msg was sent), FAILURE (queue empty)
 * Function resource consumption: 2 (no msg in queue) - 440 us 
 */
int j1850_check_queue( void )
{
  int return_value = FAILURE;

  /*
   * Check if there was a message in queue
   */
  if( msg_inbox_j1850_vpw_rx_msg_count != 0 )
  {
    if( msg_inbox_j1850_vpw_rx_service[ 0 ] != PHYS_INVALID_SERVICE )
    {
      j1850_vpw_stm_func( J1850_VPW_RX, msg_inbox_j1850_vpw_rx_service[ 0 ], msg_inbox_j1850_vpw_rx_data[ 0 ] );
      msg_inbox_j1850_vpw_rx_service[ 0 ] = PHYS_INVALID_SERVICE;
      msg_inbox_j1850_vpw_rx_msg_count--;
      return_value = SUCCESS;
    }
    else
    {
      j1850_vpw_stm_func( J1850_VPW_RX, msg_inbox_j1850_vpw_rx_service[ 1 ], msg_inbox_j1850_vpw_rx_data[ 1 ] );
      msg_inbox_j1850_vpw_rx_service[ 1 ] = PHYS_INVALID_SERVICE;
      msg_inbox_j1850_vpw_rx_msg_count--;
      return_value = SUCCESS;
    }
  }
  else if( msg_inbox_j1850_vpw_tx_msg_count != 0 )
  {
    if( msg_inbox_j1850_vpw_tx_service[ 0 ] != PHYS_INVALID_SERVICE )
    {
      j1850_vpw_stm_func( J1850_VPW_TX, msg_inbox_j1850_vpw_tx_service[ 0 ], msg_inbox_j1850_vpw_tx_data[ 0 ] );
      msg_inbox_j1850_vpw_tx_service[ 0 ] = PHYS_INVALID_SERVICE;
      msg_inbox_j1850_vpw_tx_msg_count--;
      return_value = SUCCESS;
    }
    else
    {
      j1850_vpw_stm_func( J1850_VPW_TX, msg_inbox_j1850_vpw_tx_service[ 1 ], msg_inbox_j1850_vpw_tx_data[ 1 ] );
      msg_inbox_j1850_vpw_tx_service[ 1 ] = PHYS_INVALID_SERVICE;
      msg_inbox_j1850_vpw_tx_msg_count--;
      return_value = SUCCESS;
    }
  }
  else if( msg_inbox_j1850_dl_msg_count != 0 )
  {
    if ( msg_inbox_j1850_dl_service[ 0 ] != DATA_LINK_INVALID_SERVICE )
    {
      j1850_dl_stm_func( msg_inbox_j1850_dl_service[ 0 ], msg_inbox_j1850_dl_data[ 0 ] );
      msg_inbox_j1850_dl_service[ 0 ] = DATA_LINK_INVALID_SERVICE;
      msg_inbox_j1850_dl_msg_count--;
      return_value = SUCCESS;
    }
    else
    {
      j1850_dl_stm_func( msg_inbox_j1850_dl_service[ 1 ], msg_inbox_j1850_dl_data[ 1 ] );
      msg_inbox_j1850_dl_service[ 1 ] = DATA_LINK_INVALID_SERVICE;
      msg_inbox_j1850_dl_msg_count--;
      return_value = SUCCESS;
    }
  }
  else if( msg_inbox_j1979_app_msg_count != 0 )
  {
    if ( msg_inbox_j1979_app_service[ 0 ] != APP_INVALID_SERVICE )
    {
      j1979_app_stm_func( msg_inbox_j1979_app_service[ 0 ], msg_inbox_j1979_app_data[ 0 ] );
      msg_inbox_j1979_app_service[ 0 ] = APP_INVALID_SERVICE;
      msg_inbox_j1979_app_msg_count--;
      return_value = SUCCESS;
    }
    else
    {
      j1979_app_stm_func( msg_inbox_j1979_app_service[ 1 ], msg_inbox_j1979_app_data[ 1 ] );
      msg_inbox_j1979_app_service[ 1 ] = APP_INVALID_SERVICE;
      msg_inbox_j1979_app_msg_count--;
      return_value = SUCCESS;
    }
  }
  return return_value;
}

