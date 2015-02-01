/*
 * Project: OBD II reader with Arduino
 *
 * Author: Tommi Kangassuo (kangassu@gmail.com)
 *
 * Work for LUT CT10A4000
 */
 
/*
 * SAE J1850 VPW protocol data link layer part implementation for
 * the project, including link control with arbitration,
 * crc calculation and message integrity check
 *
 * Contents:
 * 1. Inclusions
 * 2. Constants and global variables
 * 3. Function list:
 * 3.1 j1850_dl_init
 * 3.2 j1850_dl_stm_idle
 * 3.3 j1850_dl_stm_send
 * 3.4 j1850_dl_stm_receive
 * 3.5 j1850_dl_stm_wait
 * 3.6 j1850_dl_send_msg
 * 3.7 j1850_dl_build_crc
 * 3.8 j1850_dl_data_integ_check
 */
 
/*
 * 1. Inclusions
 */

/*
 * 2. Constants and global variables
 */
uint8_t j1850_msg_bit_table[ J1850_MAX_MSG_SYMBOL_COUNT ];
uint8_t j1850_msg_bit_table_len;
uint8_t msg_send_pos;
uint8_t msg_recv_pos;

/*
 * 3.1 j1850_dl_init
 *
 * Description: J1850 Data Link initialization
 *
 * input: void
 * output: void
 * Function resource consumption: 1 us
 */
void j1850_dl_init( void )
{
  /*
   * Set "TX" green and "RX" red leds in port D6 and D5 as outputs
   */
  DDRD = DDRD | B01100000;
  j1850_dl_stm_func = &j1850_dl_stm_idle;
}

/*
 * 3.2 j1850_dl_stm_idle
 *
 * Description: State machine functionality for J8150 DL IDLE state
 *
 * input: service - requested service
 *        data - service specific data
 * output: void
 * Function resource consumption: 432 us with 12-byte input
 */
void j1850_dl_stm_idle( DATA_LINK_SERVICE_ID service, int data )
{
  switch( service )
  {
    case DATA_LINK_SEND_MESSAGE:

      /* Illuminate "TX" green led in port D6 */
      PORTD = PORTD | B01000000;

      j1850_dl_send_msg( data );
      j1850_dl_stm_func = &j1850_dl_stm_send;
      break;
  }
}

/*
 * 3.3 j1850_dl_stm_send
 *
 * Description: State machine functionality for J8150 DL SEND state
 *
 * input: service - requested service
 *        data - service specific data
 * output: void
 * Function resource consumption: 4-10.5 us
 */
void j1850_dl_stm_send( DATA_LINK_SERVICE_ID service, int data )
{
  switch( service )
  {
    case DATA_LINK_RECV_SYMBOL:
      /*
       * In case of error in reception, or lost arbitration, wait
       * for retry (after IFS).
       */
      if( j1850_msg_bit_table[ msg_recv_pos++ ] == ( J1850_SYMBOL_ID )data )
      {
        /*
         * Check that there is no more than one symbol queued at one time in phys layer,
         * and there are still symbols left to transmit.
         */
        if( ( msg_send_pos <= ( msg_recv_pos + 1 ) ) && ( msg_send_pos < j1850_msg_bit_table_len ) )
        {
          j1850_vpw_tx_input_msg( PHYS_SEND_SYMBOL, j1850_msg_bit_table[ msg_send_pos++ ] );
        }
        /*
         * If last sent symbol was received, go to message receive state
         */
        else if( msg_recv_pos == j1850_msg_bit_table_len )
        {
          /*
           * After last symbol transmission, there will be at least IFS period
           * before response, maximum of 100ms. This is handled by J1979.
           */
          msg_recv_pos = 0;
          
          j1850_dl_stm_func = &j1850_dl_stm_receive;

          /*
           * Turn off "TX" green led in port D6
           */
          PORTD = PORTD & B10111111;
          
          /*
           * Send indication of successful request transmission
           */
          j1979_app_input_msg( APP_MESSAGE_SENT, 0 );
        }
      }
      /*
       * In case of error in reception, or lost arbitration, wait
       * for retry (after IFS period of LOW line level).
       */
      else
      {
        j1850_vpw_tx_input_msg( PHYS_CANCEL_SYMBOL, 0 );
        j1850_dl_stm_func = &j1850_dl_stm_wait;
        /*
         * Turn off "TX" green led in port D6
         */
        PORTD = PORTD & B10111111;
      }
      break;
      
    case DATA_LINK_CANCEL_MESSAGE:
      /*
       * Cancel ongoing transmission and reception and settle to IDLE
       */
      j1850_vpw_tx_input_msg( PHYS_CANCEL_SYMBOL, 0 );
      j1850_vpw_rx_input_msg( PHYS_STOP_LISTEN, 0 );
      j1850_dl_stm_func = &j1850_dl_stm_idle;
      /*
       * Turn off "TX" green led in port D6 and "RX" red led in port D5
       */
      PORTD = PORTD & B10011111;
      break;
  }
}

/*
 * 3.4 j1850_dl_stm_receive
 *
 * Description: State machine functionality for J8150 DL RECEIVE state
 *
 * input: service - requested service
 *        data - service specific data
 * output: void
 * Function resource consumption: xxx us
 */
void j1850_dl_stm_receive( DATA_LINK_SERVICE_ID service, int data )
{
  int msg_length;
  
  switch( service )
  {
    case DATA_LINK_RECV_SYMBOL:
      /*
       * Receive symbols to j1850_msg_bit_table
       * After reception starts SOF received, configure
       * Physical layer to detect IFS. This will latest end the
       * message reception.
       */
      if( msg_recv_pos == 0 &&
          ( ( J1850_SYMBOL_ID )data == J1850_SOF_SYMBOL ) )
      {
        j1850_msg_bit_table[ msg_recv_pos++ ] = ( J1850_SYMBOL_ID )data;
        /*
         * Illuminate "RX" red led in port D5
         */
        PORTD = PORTD | B00100000;
      }
      /*
       * Store values when reception is started
       */
      else if( ( msg_recv_pos > 0 ) && ( msg_recv_pos < J1850_MAX_MSG_SYMBOL_COUNT ) )
      {
        j1850_msg_bit_table[ msg_recv_pos++ ] = ( J1850_SYMBOL_ID )data;
        /*
         * Check if complete message is received
         */
        if( ( J1850_SYMBOL_ID )data == J1850_IFS_SYMBOL )
        {
          if( ( msg_length = j1850_dl_data_integ_check( ) ) != 0 )
          {
            /*
             * There is some time to handle message in J1979 application layer,
             * as new message contents will be stored to the same message buffer
             * after next IFS is received (min SOF + 1 byte + IFS: ~1ms).
             */
            j1979_app_input_msg( APP_MESSAGE_RECEIVED, msg_length );
          }
 
          msg_recv_pos = 0;
          /*
           * Turn off "RX" red led in port D5
           */
          PORTD = PORTD & B11011111;
        }
      }
      break;

    case DATA_LINK_CANCEL_MESSAGE:
      /*
       * Cancel ongoing reception and settle to IDLE
       */
      j1850_vpw_rx_input_msg( PHYS_STOP_LISTEN, 0 );
      j1850_dl_stm_func = &j1850_dl_stm_idle;
      /*
       * Turn off "RX" red led in port D5
       */
      PORTD = PORTD & B11011111;
      break;
  }
}

/*
 * 3.5 j1850_dl_stm_wait
 *
 * Description: State machine functionality for J8150 DL WAIT state
 *
 * input: service - requested service
 *        data - service specific data
 * output: void
 * Function resource consumption: xxx us
 */
void j1850_dl_stm_wait( DATA_LINK_SERVICE_ID service, int data )
{ 
  switch( service )
  {
    case DATA_LINK_RECV_SYMBOL:
      if( ( J1850_SYMBOL_ID )data == J1850_IFS_SYMBOL )
      {
        /*
         * Restart the message transmission after IFS detected
         */
        msg_send_pos = 0;
        msg_recv_pos = 0;
      
        j1850_vpw_tx_input_msg( PHYS_SEND_SYMBOL, j1850_msg_bit_table[ msg_send_pos++ ] );
        j1850_vpw_tx_input_msg( PHYS_SEND_SYMBOL, j1850_msg_bit_table[ msg_send_pos++ ] );
        j1850_dl_stm_func = &j1850_dl_stm_send;
        /* Illuminate "TX" green led in port D6 */
        PORTD = PORTD | B01000000;
      }
      break;
      
    case DATA_LINK_CANCEL_MESSAGE:
      /*
       * Cancel ongoing reception and settle to IDLE
       */
      j1850_vpw_rx_input_msg( PHYS_STOP_LISTEN, 0 );
      j1850_dl_stm_func = &j1850_dl_stm_idle;
      break;
  }
}

/*
 * 3.6 j1850_dl_send_msg
 *
 * Description: Coversion function from J1979 message to
 *              J1850 tranmission symbols, including CRC calculation.
 *
 * input: input_msg_length - J1979 message length in bytes
 * output: void
 * Function resource consumption: 432 us (12 bytes of data)
 */
void j1850_dl_send_msg( int input_msg_length )
{
  uint8_t symbol_index = 0;
  uint8_t bit_index;
  uint8_t nbr_of_data_bytes = 0;
  uint8_t crc_result;
  byte* input_msg;

  input_msg = ( byte* ) &obd2_message;
  
  /*
   * Construct the symbol sequence:
   * start with BREAK symbol to maximize
   * success of arbitration and having other
   * nodes to listen state.
   * According to SAE J1850 reference chapter 5.3,
   * BREAK symbol can occur (be sent) on a network at any time.
   */
  j1850_msg_bit_table[ symbol_index++ ] = J1850_BRK_SYMBOL;
   
  /*
   * There needs to be an IFS period after BRK
   * in order for nodes to resynchronize themselves.
   */
  j1850_msg_bit_table[ symbol_index++ ] = J1850_IFS_SYMBOL;
   
  /*
   * Next, message frame is started with SOF.
   */
  j1850_msg_bit_table[ symbol_index++ ] = J1850_SOF_SYMBOL;
   
  /*
   * Actual data field of the message, derive it from J1979 input msg.
   */
  while( nbr_of_data_bytes < input_msg_length )
  {
    bit_index = 0;
    while( bit_index < 8 )
    {
      /*
       * bit 1 = J1850_BIT_ONE_SYMBOL
       * bit 0 = J1850_BIT_ZERO_SYMBOL
       */
      if( ( *( input_msg + nbr_of_data_bytes ) & ( 128 >> bit_index++ ) ) == 0 )
      {
        j1850_msg_bit_table[ symbol_index++ ] = J1850_BIT_ZERO_SYMBOL;
      }
      else
      {
        j1850_msg_bit_table[ symbol_index++ ] = J1850_BIT_ONE_SYMBOL;
      }
    }
    nbr_of_data_bytes++;
  }
   
  /*
   * After data, calculate CRC and add it to the symbol table
   */
  crc_result = j1850_dl_build_crc( input_msg, nbr_of_data_bytes );

  bit_index = 0;
  while( bit_index < 8 )
  {
    /*
     * bit 1 = J1850_BIT_ONE_SYMBOL
     * bit 0 = J1850_BIT_ZERO_SYMBOL
     */
    if( ( crc_result & ( 128 >> bit_index++ ) ) == 0 )
    {
      j1850_msg_bit_table[ symbol_index++ ] = J1850_BIT_ZERO_SYMBOL;
    }
    else
    {
      j1850_msg_bit_table[ symbol_index++ ] = J1850_BIT_ONE_SYMBOL;
    }
  }
   
  /*
   * After the frame, EOD, EOF and IFS are transmitted as LOW.
   * This can be implemented without any actual symbol as after
   * the IFS the response message is waited.
   */
   
  /*
   * Store the total message length of symbols
   */
  j1850_msg_bit_table_len = symbol_index;

  /*
   * Start monitoring the line before sending anything:
   * Send RX start request to Physical layer.
   */
  j1850_vpw_rx_input_msg( PHYS_START_LISTEN, 0 );
    
  /*
   * Start message sending, send two first symbols.
   * Following symbols are sent after first symbol done ISR.
   */
  msg_send_pos = 0;
  msg_recv_pos = 0;

  j1850_vpw_tx_input_msg( PHYS_SEND_SYMBOL, j1850_msg_bit_table[ msg_send_pos++ ] );
  j1850_vpw_tx_input_msg( PHYS_SEND_SYMBOL, j1850_msg_bit_table[ msg_send_pos++ ] );
}


/*
 * 3.7 j1850_dl_build_crc
 *
 * Description: CRC-8-SAE calculation with polynomial for CRC:
 * x8 + x4 + x3 + x2 + 1. NOTE: CRC build code is done by B. Roadman
 *
 * input: input_msg - pointer to J1979 message content
 *        nbytes - number of bytes in the message
 * output: crc_req - calculated CRC byte
 * Function resource consumption: 125.5 us (12 bytes of data)
 * 
 */
uint8_t j1850_dl_build_crc( byte* input_msg, uint8_t nbytes )
{
  uint8_t crc_reg = 0xff, poly, i, j;
  uint8_t *byte_point;
  uint8_t bit_point;

  for( i = 0, byte_point = input_msg; i < nbytes; ++i, ++byte_point )
  {
    for ( j = 0, bit_point = 0x80; j < 8; ++j, bit_point >>= 1 )
    {
      if( bit_point & *byte_point ) // case for new bit = 1
      {
        if( crc_reg & 0x80 )
        {
          poly = 1; // define the polynomial
	}
        else
	{
          poly = 0x1c;
        }
        crc_reg = ( ( crc_reg << 1 ) | 1 ) ^ poly;
      }
      else // case for new bit = 0
      {
        poly = 0;
	if( crc_reg & 0x80 )
        {
          poly = 0x1d;
        }
        crc_reg = ( crc_reg << 1 ) ^ poly;
      }
    }
  }
  return ~crc_reg; // Return CRC
}

/*
 * 3.8 j1850_dl_data_integ_check
 *
 * Description: Checks input message integrity and CRC-8-SAE calculation.
 *
 * input: input_msg - pointer to J1979 message content
 * output: int - number of data bytes
 * Function resource consumption: xx us (12 bytes of data)
 */
int j1850_dl_data_integ_check( )
{
  byte* input_msg;
  uint8_t index = 1; // Skip SOF
  uint8_t nbr_of_data_bytes = 0;
  uint8_t bit_index = 0;
  uint8_t crc_result;

  input_msg = ( byte* ) &obd2_message;

  /*
   * Decode the bits into obd2 message store and check that there are
   * only ONE and ZERO symbols in the received buffer.
   */
  while( j1850_msg_bit_table[ index ] != J1850_IFS_SYMBOL )
  {
    if( j1850_msg_bit_table[ index ] == J1850_BIT_ONE_SYMBOL )
    {
      *( input_msg + nbr_of_data_bytes ) |= ( 128 >> bit_index++ );
    }
    else if( j1850_msg_bit_table[ index ] == J1850_BIT_ZERO_SYMBOL )
    {
      *( input_msg + nbr_of_data_bytes ) &= ( ~( 128 >> bit_index++ ) );
    }
    else
    {
      /*
       * Non valid symbol found, integrity check failed.
       */
      return 0;
    }
    /*
     * Check the obd2 message bit counters
     */
    if( bit_index == 8 )
    {
      bit_index = 0;
      nbr_of_data_bytes++;
    }
    index++;
  }

  /*
   * Take the number of data bytes, i.e reduce the CRC from decoded message
   */
  nbr_of_data_bytes--;
  crc_result = *( input_msg + nbr_of_data_bytes );
  *( input_msg + nbr_of_data_bytes ) = 0;

  /*
   * After data, calculate CRC and compare it to received CRC bits
   */
  if( j1850_dl_build_crc( input_msg, nbr_of_data_bytes ) != crc_result )
  {
    /*
     * CRC error found, integrity check failed.
     */
    return 0;
  }

  return nbr_of_data_bytes;
}
