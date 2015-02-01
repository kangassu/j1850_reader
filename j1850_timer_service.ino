/*
 * Project: OBD II reader with Arduino
 *
 * Author: Tommi Kangassuo (kangassu@gmail.com)
 *
 * Work for LUT CT10A4000
 */
 
/*
 * SAE J1850 VPW protocol hw timer services for
 * protocol stack, including regular event timer (TX) and
 * normal mode running timer (RX).
 *
 * Contents:
 * 1. Inclusions
 * 2. Constants and global variables
 * 3. Functions
 * 3.1 j1850_timer_init
 * 3.2 j1850_timer_start
 * 3.3 j1850_timer_stop
 * 3.4 j1850_tx_timer_request
 * 3.5 j1850_tx_timer_next_pending
 * 3.6 j1850_timer_remove_entity
 * 4. Interrupt handlers
 * 4.1 Regular event timer (TX) int
 * 4.2 External interrupt 1 (RX) int
 */

/*
 * 1. Inclusions
 */
 
/*
 * 2. Constants and global variables
 */

/*
 * Static allocation for tx timer counters
 */
volatile uint8_t tx_timer_elem_time_left;
volatile uint8_t next_tx_element;

/*
 * 3. Functions
 */
 
/*
 * 3.1 j1850_timer_init
 * 
 * Description: J8150 timer data initialization
 *
 * input: void
 * output: void
 * Function resource consumption: 1 us
 */
void j1850_timer_init( void )
{
  /*
   * Initialize tx timer element
   */
  tx_timer_elem_time_left = 0;
  next_tx_element = 0;
  
  /*
   * Initialize reqular event TX timer 0 and RX timer 2
   */
  cli();
  TCCR0B = 0; // Stop TX timer
  TCCR2B = 0; // Stop RX timer
  sei();
}

/*
 * 3.2 j1850_timer_start
 *
 * Description: J8150 timer start functionality
 *
 * input: timer_type - RX or TX (J1850_REG_EVENT_TIMER) timer
 * output: void
 * Function resource consumption: 2 us
 */
void j1850_timer_start( J1850_TIMER_TYPE timer_type )
{
  switch( timer_type )
  {
    case J1850_REG_EVENT_TIMER:
      /*
       * Setup 8-bit Timer0 for regular TX events:
       * In PWM physical layer required timer resolution is 8 usecs.
       * This leads to maximum prescaler of 64 for 16MHz oscillator,
       * i.e timer tick is 4 usecs.
       * In VPW physical layer required timer resolution is 64 usecs
       * which can be achieved with the 4 usecs ticks.
       */
      cli();
      TCNT0 = 0; // Initialize Timer/Counter Register (tick count)
      TCCR0A = 2; // Turn on CTC mode: WGM01
      TCCR0B = 3; // Set prescaler 64: CS01, CS00
      TIMSK0 = 2; // Enable compare match interrupt: OCIE0A
      OCR0A = 15; // Compare match register for 64 usecs - 1
      sei();     
      break;
     
    case J1850_RX_TIMER:
      /*
       * Setup RX timer Timer2:
       * For the marginal of received signals J8150 VPW specifies
       * 30 microseconds (active 1) thus 16,5 microseconds resolution
       * would get differentiation between the received symbols.
       */
      cli();
      TCNT2 = 0; // Initialize Timer/Counter Register (tick count)
      TCCR2A = 0; // Run in normal mode: timer is only needed for RX time
      TCCR2B = 6; // Set prescaler 256: CS22, CS21
      sei();
      break;
  }
}

/*
 * 3.3 j1850_timer_stop
 *
 * Description: J1850 timer stop functionality
 *
 * input: timer_type - RX or TX (J1850_REG_EVENT_TIMER) timer
 * output: void
 * Function resource consumption: 1.5 us
 */
void j1850_timer_stop( J1850_TIMER_TYPE timer_type )
{
  switch( timer_type )
  {
    case J1850_REG_EVENT_TIMER:
      /*
       * Stop running timer and initialize registers
       */
      cli();
      /*
       * Initialize Timer/Counter Control Register B Clock Select bits, i.e
       * timer is stopped when CS2, CS1 and CS0 bits are zero.
       */  
      TCCR0B = 0;
      /*
       * Initialize Timer/Counter Register (tick count)
       */
      TCNT0 = 0;
      sei();
      break;
      
    case J1850_RX_TIMER:
      /*
       * Stop running timer and initialize registers
       */
      cli();
      /*
       * Initialize Timer/Counter Control Register B Clock Select bits, i.e
       * timer is stopped when CS2, CS1 and CS0 bits are zero.
       */  
      TCCR2B = 0;
      /*
       * Initialize Timer/Counter Register (tick count)
       */
      TCNT2 = 0;
      sei();
      break;
  }
}

/*
 * 3.4 j1850_tx_timer_request
 *
 * Description: J8150 tx timer timeout request
 *
 * input: timeout_value [us]
 * output: void
 * Function resource consumption: 4-6 us
 */
void j1850_tx_timer_request( unsigned int timeout_value )
{
  uint8_t timeout_in_cm_count;
  uint8_t prescaler;

  /*
   * Check that regular event timer is running,
   * if not, start the timer.
   */
  if( ( TCCR0B & ( ( 1 << CS02 ) | ( 1 << CS01 ) | ( 1 << CS00 ) ) ) == 0 )
  {
    j1850_timer_start( J1850_REG_EVENT_TIMER );
  }

  prescaler = J1850_VPW_TIMER_PRESCALER;

  /*
   * Calculate compare match counts for timeout:
   * timeout in nbr of counts (nearest count) by
   * dividing timeout with 64 micros cycle and
   * checking the nearest integer of counts.
   */
  timeout_in_cm_count = timeout_value >> J1850_VPW_TIMER_CTC_SHIFT_VAL;

  while( timeout_value >= J1850_VPW_TIMER_CTC_IN_US )
  {
    timeout_value -= J1850_VPW_TIMER_CTC_IN_US;
  }
  if( timeout_value >= ( J1850_VPW_TIMER_CTC_IN_US >> 1 ) )
  {
    timeout_in_cm_count = timeout_in_cm_count + 1;
  }
  tx_timer_elem_time_left = timeout_in_cm_count;
  
  /*
   * Always clear the next pending timer element info
   */
  next_tx_element = 0;
}

/*
 * 3.5 j1850_tx_timer_next_pending
 *
 * Description: Stores info if pending transmission will be HIGH
 *
 * input: is_high - Defines if next symbol will be HIGH or not
 * output: void
 * Function resource consumption: 1 us
 */
void j1850_tx_timer_next_pending( unsigned int is_high )
{ 
  next_tx_element = is_high;
}

/*
 * 3.6 j1850_timer_remove_entity
 *
 * Description: Removes timer request and stops timer.
 *
 * input: entity - RX or TX ENTITY
 * output: void
 * Function resource consumption: 3 us
 */
void j1850_timer_remove_entity( J1850_ENTITY_ID entity )
{
  /*
   * Loop the timer elemements to remove given entity
   */
  if( entity == J1850_VPW_RX )
  {
    j1850_timer_stop( J1850_RX_TIMER );
  }
  else if( entity == J1850_VPW_TX )
  {
    tx_timer_elem_time_left = 0;
    next_tx_element = 0;
    j1850_timer_stop( J1850_REG_EVENT_TIMER );
    /*
     * Set TX line to low
     */
    PORTD &= B11101111;
  }
}

/*
 * 4. Interrupt handlers
 */

/*
 * 4.1 Timer0 (regular event) Compare Match Int
 *
 * Description: CTC interrupt for TX timer (every 64 us)
 *
 * Function resource consumption: 2-4 us
 */
ISR( TIMER0_COMPA_vect )
{
  /*
   * Check the registered entities if timer has elapsed:
   * Toggle tranmitter pin if needed and sent timeout message to
   * TX entity, disable interrupts during messaging.
   */
  if( tx_timer_elem_time_left > 0 )
  {
    tx_timer_elem_time_left -= 1;

    if( tx_timer_elem_time_left == 0 )
    {
      /*
       * Check the need for pin toggling:
       * always draw the line LOW if active. Other way around (LOW->HIGH)
       * toggling is only done if next symbol is known and is HIGH.
       */
      if( next_tx_element == HIGH )
      {
        PORTD |= B00010000;
      }
      else
      {
        PORTD &= B11101111;
      }
      
      j1850_vpw_tx_input_msg( PHYS_TIMER_ELAPSED, 0 );
    }
  }
}

/*
 * 4.2 External interrupt 1 (reception PIN 3)
 *
 * Description: Rising or falling edge interrupt for
 * RX signal (input pin 3).
 *
 * Function resource consumption: 4-6 us
 */
ISR( INT1_vect )
{
  int rx_runtime;
  uint8_t pin_state = LOW;
  J1850_SYMBOL_ID symbol;
  
  /*
   * Toggle interrupt mode raising <-> falling edge
   */
  if( EICRA & B00000100 )
  {
    EICRA = B00001000; // ISC11 = 1, ISC10 = 0: Falling edge
    pin_state = HIGH; // PIN state was HIGH, i.e pulled to ground (0V)
  }
  else
  {
    EICRA = B00001100; // ISC11 = 1, ISC10 = 1: Rising edge
  }

  /*
   * Check if reception starts or continues:
   * If timer is not running, this is start of first reception, otherwise
   * check the length of the received symbol and deliver it to Data Link layer.
   */
  if( ( TCCR2B & ( ( 1 << CS22 ) | ( 1 << CS21 ) | ( 1 << CS20 ) ) ) != 0 )
  {
    /*
     * Read the timer value, pin state, and restart RX clock.
     * NOTE: rx_runtime = timer_counts of 16 us cycles
     */
    rx_runtime = TCNT2;
    
    /*
     * Re-start RX clock counter.
     */
    TCNT2 = 0;
    /*
     * Clarify the received symbols according to VPW timing requirements for
     * reception defined in SAE J8150 reference chapter 7.3.3.1, and send it
     * to Data Link layer.
     */
    switch( rx_runtime )
    {
      /*
       * Received symbol length of 35 - 96 microseconds => 36 - 96 according to
       * Timer2 prescaler of 256 with 16 microsecond's resolution.
       */
      case 3:
      case 4:
      case 5:
      case 6:
        if( pin_state == LOW )
        {
          symbol = J1850_BIT_ZERO_SYMBOL;
        }
        else
        {
          symbol = J1850_BIT_ONE_SYMBOL;
        }
        break;
        
      /*
       * Received symbol length of 97 - 163 microseconds => 97 - 160 according to
       * Timer2 prescaler of 256 with 16 microsecond's resolution.
       */
      case 7:
      case 8:
      case 9:
      case 10:
        if( pin_state == LOW )
        {
          symbol = J1850_BIT_ONE_SYMBOL;
        }
        else
        {
          symbol = J1850_BIT_ZERO_SYMBOL;
        }
        break;

      /*
       * Received symbol length of 164 - 239 microseconds => 161 - 240 according to
       * Timer2 prescaler of 256 with 16 microsecond's resolution.
       */
      case 11:
      case 12:
      case 13:
      case 14:
      case 15:
        if( pin_state == LOW )
        {
          symbol = J1850_EOD_SYMBOL;
        }
        else
        {
          symbol = J1850_SOF_SYMBOL;
        }
        break;

      /*
       * Received symbol length is smaller than 35 microseconds:
       * Symbol is discarded as invalid.
       */
      case 0:
      case 1:
      case 2:
        symbol = J1850_INVALID_SYMBOL;
        break;

      /*
       * Received symbol length is greater than 240 microseconds:
       * Symbol is either BRK or EOF/IFS
       */
      default:
        if( pin_state == LOW )
        {
          symbol = J1850_IFS_SYMBOL;
        }
        else
        {
          symbol = J1850_BRK_SYMBOL;
        }
        break;
    }

    /*
     * Send message for reception done directly to Data Link layer
     */
    j1850_dl_input_msg( DATA_LINK_RECV_SYMBOL, symbol );
  }
  /*
   * This is start of first symbol reception, i.e start timer
   */
  else
  {
    j1850_timer_start( J1850_RX_TIMER );
  }
}
