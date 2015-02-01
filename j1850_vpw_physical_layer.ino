/*
 * Project: OBD II reader with Arduino
 *
 * Author: Tommi Kangassuo (kangassu@gmail.com)
 *
 * Work for LUT CT10A4000
 */
 
/*
 * SAE J1850 VPW protocol physical layer part implementation for
 * the project, including binary transmission and reception
 *
 * Contents:
 * 1. Inclusions
 * 2. Constants and global variables
 * 3. Functions
 * 3.1 j1850_vpw_init
 * 3.2 j1850_vpw_stm_idle
 * 3.3 j1850_vpw_stm_monitoring
 * 3.4 j1850_vpw_stm_sending
 */
 
/*
 * 1. Inclusions
 */
 
/*
 * 2. Constants and global variables
 */
J1850_SYMBOL_ID next_queued_symbol;

/*
 * 3. Functions
 */
  
/*
 * 3.1 j1850_vpw_init
 *
 * Description: J1850 VPW initialization
 *
 * input: void
 * output: void
 * Function resource consumption: 2 us
 */
void j1850_vpw_init( void )
{
  j1850_vpw_stm_func = &j1850_vpw_stm_idle;
  next_queued_symbol = J1850_INVALID_SYMBOL;
  /*
   * Set J1850 VPW transmit port, DIGITAL PIN 4
   */
  DDRD = DDRD | B00010000;
  /*
   * Set J1850 VPW receive port, DIGITAL PIN 3,
   * enable internal pull-up resistor.
   */
  DDRD = DDRD & B11110111;
  PORTD |= B00001000;
  
  /*
   * Set HIP7020 Loopback Mode pin to HIGH, i.e normal mode,
   * set DIGITAL PIN 13
   */
  DDRB = DDRB | B00100000;
  PORTB |= B00100000;

  j1850_timer_init( );
}

/*
 * 3.2 j1850_vpw_stm_idle
 * 
 * Description: State machine functionality for J8150 VPW IDLE state
 *
 * input: entity - message target entity
 *        service - requested service id
 *        data - optional service specific numerical data
 * output: void
 * Function resource consumption: 2.5 us
 */
void j1850_vpw_stm_idle( J1850_ENTITY_ID entity, PHYS_SERVICE_ID service, int data )
{

  if( service == PHYS_START_LISTEN )
  {  
    cli();
    /*
     * Enable External INT1
     */
    EIMSK = EIMSK | B00000010; // INT1 = 1
    /*
     * Set External interrupt mode for pin change to falling edge.
     * Also, initial level is checked and explicit interrupt done if needed.
     * Reason for this is that in case level is readily LOW then the interrupt
     * will happen immediately and timer for reception is started.
     */
    EICRA = B00001000; // ISC11 = 1, ISC10 = 0

    sei();
  
    /*
     * Check the pin 3 status and trigger interrupt functionality if needed:
     * In case of LOW, input signal is detected.
     */
    if( ( PIND & B00001000 ) == 0 )
    {
      INT1_vect( );
    }

    /*
     * Change state to MONITORING
     */
    j1850_vpw_stm_func = &j1850_vpw_stm_monitoring;
  }
}

/*
 * 3.3 j1850_vpw_stm_monitoring
 * 
 * Description: State machine functionality for J8150 VPW MONITORING state
 *
 * input: entity - message target entity
 *        service - requested service id
 *        data - optional service specific numerical data
 * output: void
 * Function resource consumption: 8 us
 */
void j1850_vpw_stm_monitoring( J1850_ENTITY_ID entity, PHYS_SERVICE_ID service, int data )
{
  unsigned int bit_duration;

  switch( service )
  {
    case PHYS_SEND_SYMBOL:
      /*
       * Messages are always started with Break, so it is only
       * symbol supported in this state by current implementation.
       * Set line to HIGH for the Break symbol.
       */
      PORTD |= B00010000;
      bit_duration = J1850_BRK_LEN;
      /*
       * Request a timeout event
       */
      j1850_tx_timer_request( bit_duration );
      /*
       * Change state to SENDING
       */
      j1850_vpw_stm_func = &j1850_vpw_stm_sending;
      break;
    
    case PHYS_STOP_LISTEN:
      /*
       * Disbale External INT1, stop the port listening and return to idle
       */
      cli();
      EIMSK = EIMSK & B11111101; // INT1 = 0
      sei();

      j1850_timer_remove_entity( J1850_VPW_RX );
      j1850_vpw_stm_func = &j1850_vpw_stm_idle;
      break;
  }
}

/*
 * 3.4 j1850_vpw_stm_sending
 * 
 * Description: State machine functionality for J8150 VPW SENDING state
 *
 * input: entity - message target entity
 *        service - requested service id
 *        data - optional service specific numerical data
 * output: void
 * Function resource consumption: 2.5-17 us
 */
void j1850_vpw_stm_sending( J1850_ENTITY_ID entity, PHYS_SERVICE_ID service, int data )
{
  unsigned int bit_duration = 0;
  uint8_t current_line_state;

  switch( service )
  {
    /*
     * Handle next input symbol by putting it to queue and
     * informing timer_service if line is currently low
     * and next is high.
     */
    case PHYS_SEND_SYMBOL:
      
      next_queued_symbol = ( J1850_SYMBOL_ID )data;
      current_line_state = ( PORTD & B00010000 );
      /*
       * Check if next symbol will be HIGH
       */
      if( ( ( current_line_state == 0 ) &&
          ( ( next_queued_symbol == J1850_BIT_ZERO_SYMBOL ) ||
            ( next_queued_symbol == J1850_BIT_ONE_SYMBOL ) ) ) ||
          ( next_queued_symbol == J1850_SOF_SYMBOL ) )
      {
        j1850_tx_timer_next_pending( 1 ); // is_high == TRUE
      }
      break;

    /*
     * Handle interrupt input message, send next symbol if in queue.
     * NOTE: current_line_state reflects the ongoing line state for given
     * symbol as it is already toggled in timer_service.
     */
    case PHYS_TIMER_ELAPSED:
    
      current_line_state = ( PORTD & B00010000 );
      switch( next_queued_symbol )
      {
        case J1850_BIT_ZERO_SYMBOL:
          if( current_line_state != 0 )
          {
            bit_duration = J1850_BIT_ZERO_HIGH_LEN;
          }
          else
          {
            bit_duration = J1850_BIT_ZERO_LOW_LEN;
          }
          break;
        
        case J1850_BIT_ONE_SYMBOL:
          if( current_line_state != 0 )
          {
            bit_duration = J1850_BIT_ONE_HIGH_LEN;
          }
          else
          {
            bit_duration = J1850_BIT_ONE_LOW_LEN;
          }
          break;
      
        /*
         * BREAK symbol can be transmitted onto the bus at any time
         * but in current OBD2 reader implementation it is used only as first
         * transmission symbol, i.e it is not handled in this state.
         */
        
        /*
         * SOF is always HIGH and it is never the first symbol, i.e
         * line is already set by timer_service to correct position.
         */
        case J1850_SOF_SYMBOL:
          bit_duration = J1850_SOF_LEN;
          break;
          
        /*
         * Current OBD2 reader implementation does not support IFR, i.e
         * IFS will always contain both EOD and EOF and doesn't need to
         * be handled separately. IFS is always LOW and already set.
         */
        case J1850_IFS_SYMBOL:
          bit_duration = J1850_IFS_LEN;
          break;
      
        case J1850_INVALID_SYMBOL:
          /*
           * No queued symbol, transmission is done. Stop tx timer and
           * go to monitoring state.
           */
          j1850_timer_remove_entity( J1850_VPW_TX );
          j1850_vpw_stm_func = &j1850_vpw_stm_monitoring;
          break;
      }

      next_queued_symbol = J1850_INVALID_SYMBOL;

      if( bit_duration != 0 )
      {
        /*
         * Request a timeout event
         */
        j1850_tx_timer_request( bit_duration );
      }
      break;

    /*
     * Cancel transmission, go to monitoring or idle state.
     */
    case PHYS_CANCEL_SYMBOL:
      next_queued_symbol = J1850_INVALID_SYMBOL;
      j1850_timer_remove_entity( J1850_VPW_TX );
      /*
       * Check if monitoring is ongoing
       */
      if( ( EIMSK & B00000010 ) != 0 )
      {
        j1850_vpw_stm_func = &j1850_vpw_stm_monitoring;
      }
      else
      {
        j1850_vpw_stm_func = &j1850_vpw_stm_idle;
        
      }
      break;
      
    /*
     * Stop listen will be followed by CANCEL SYMBOL, i.e
     * state is not changed.
     */
    case PHYS_STOP_LISTEN:
      /*
       * Disable External INT1, stop the port listening
       */
      cli();
      EIMSK = EIMSK & B11111101; // INT1 = 0
      sei();
      j1850_timer_remove_entity( J1850_VPW_RX );
      break;
  }
}
