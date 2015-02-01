/*
 * Project: OBD II reader with Arduino
 *
 * Author: Tommi Kangassuo (kangassu@gmail.com)
 *
 * Work for LUT CT10A4000
 */
 
/*
 * SAE J1979 protocol application layer part implementation for
 * the project, and LCD UI state machine
 *
 * Contents:
 * 1. Inclusions
 * 2. Constants and global variables
 * 3. Function list:
 * 3.1 j1979_app_init
 * 3.2 j1979_app_stm_setup
 * 3.3 j1979_app_stm_ready
 * 3.4 j1979_app_stm_wait
 * 3.5 j1979_app_decode_message
 * 3.6 j1979_app_int_to_char
 * 3.7 j1979_app_lcd_menuitem
 * 3.8 j1979_app_menu_print
 * 4. Interrupt handlers
 * 4.1 Timer1 overflow int
 * 4.2 Button int
 */
 
/*
 * 1. Inclusions
 */

/*
 * 2. Constants and global variables
 */
LiquidCrystal lcd(12, 11, 10, 9, 8, 7);
LCD_STATE lcd_state;
char lcd_text_buffer[ 32 ]; // Enough for 2x16 display
/*
 * J1979 message results table (max 3 results supported) in chars:
 * [ ][ 0 ] = ECU address (optional)
 * [ ][ 0/1..6 ] = 5 characters for printout (6 if ECU address not used)
 */
char j1979_app_result[ 3 ][ 6 ];
int j1979_app_nbr_results; // Number of results to wait and store
volatile int app_timer_counter; // Timer1 overflow counter

/*
 * 3. Functions
 */
  
/*
 * 3.1 j1979_app_init
 *
 * Description: J1979 initialization
 *
 * input: void
 * output: void
 */
void j1979_app_init( void )
{ 
  /*
   * Configure button input pins and pull-up resistors for Analog port C:
   * Pin A0 for SELECT -button, corresponds to PCINT8
   * Pin A1 for NEXT -button, corresponds to PCINT9
   * Pin A2 for EXIT -button, corresponds to PCINT10
   */
  pinMode(A0, INPUT);
  digitalWrite(A0, HIGH);
  pinMode(A1, INPUT);
  digitalWrite(A1, HIGH);
  pinMode(A2, INPUT);
  digitalWrite(A2, HIGH);
  
  //LiquidCrystal lcd(12, 11, 10, 9, 8, 7);
  lcd.begin(16, 2);
  /*
   * Erase lcd_text_buffer by copying needed empty text from Flash to SRAM
   */
  strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
  
  /*
   * Setup 16-bit Timer1 for Application Layer usage,
   * wait one second for settling the LCD.
   * NOTE: When Timer0 with prescaler is used, Timer1 cannot be
   * used with prescaler or unexpected artifacts can be experienced. This is due
   * to fact that ATMega328 has a common prescaler module for Timer0 and Timer1.
   */
  app_timer_counter = 250; // 1,024 secs count
   
  cli();
  TCNT1 = 0;
  TCCR1A = 0;
  TCCR1B = 1; // Normal mode, no prescaler to avoid conflict with Timer0
  TIMSK1 = 1; // Enable overflow interrupts (each 65536 * 1/16 us = 4096us)
  sei();
      
  lcd_state = LCD_INVALID_STATE;
  j1979_app_stm_func = &j1979_app_stm_setup;
}

/*
 * 3.2 j1979_app_stm_setup
 *
 * Description: J1979 setup state
 *
 * input: service - requested service
 *        data - service specific data
 * output: void
 */
void j1979_app_stm_setup( APP_SERVICE_ID service, int data )
{
  LCD_OUTPUT action;

  switch( service )
  {
    case APP_TIMER_ELAPSED:
      /*
       * Go to READY -state and wait for user input by
       * enabling button interrupts.
       */
      j1979_app_stm_func = &j1979_app_stm_ready;
      
      /*
       * Start LCD state machine
       */
      action = j1979_app_lcd_menuitem( LCD_INVALID_INPUT, 0 );
      
      /*
       * NOTE: PCINT change interrupts are port specific,
       * note pin specific. This means that any button change
       * will cause the iterrupt and ISR routine needs to check
       * the actual button pressed.
       * Stop Application timer.
       */
      cli();
      PCICR = 2; // Enable PCINT1 (forPCINT[14:8] pins) interrupt
      PCMSK1 = 7; // Enable individual pins with mask B00000111: PCINT 10,9,8
      TCNT1 = 0; // Initialize Timer1 counter
      TIMSK1 = 0; // Disable overflow interrupt: TOIE1
      sei();

      break;
  }
}

/*
 * 3.3 j1979_app_stm_ready
 *
 * Description: J1979 idle state
 *
 * input: service - requested service
 *        data - service specific data
 * output: void
 */
void j1979_app_stm_ready( APP_SERVICE_ID service, int data )
{
  LCD_OUTPUT action;

  switch( service )
  {
    case APP_BUTTON_PRESSED:
      /*
       * Stop possible LCD timer
       */
      cli();
      TCNT1 = 0; // Initialize Timer1 counter
      TIMSK1 = 0; // Disable overflow interrupt: TOIE1
      sei();
      
      /*
       * Send the button information to LCD menu statemachine
       */
      action = j1979_app_lcd_menuitem( ( LCD_INPUT )data, 0 );
      if( action == LCD_WAIT_RESP )
      {
        /*
         * Go to wait -state for message being sent and wait for response.
         * Disable button interrupts and start 3 second application timer
         * to prevent hangup in case transmission doesn't succeed.
         */
        app_timer_counter = 750; // 3 secs count
        cli();
        PCICR = 0; // Disable PCINT1 (forPCINT[14:8] pins) interrupt
        TCNT1 = 0; // Initialize Timer1 counter
        TIMSK1 = 1; // Enable overflow interrupts (each 65536 * 1/16 us = 4096us)
        sei();
        
        j1979_app_stm_func = &j1979_app_stm_wait;
      }
      break;
      
    case APP_TIMER_ELAPSED:
      /*
       * LCD timer elapsed, send information to LCD menu statemachine
       */
      action = j1979_app_lcd_menuitem( LCD_TIMER_ELAPSED, 0 );
      
      if( action == LCD_WAIT_RESP )
      {
        /*
         * Disable button interrupts, start J1979 communication timer,
         * and go to WAIT -state
         */
        app_timer_counter = 25; // 102,4 msecs count
        cli();
        PCICR = 0; // Disable PCINT1 (forPCINT[14:8] pins) interrupt
        TCNT1 = 0; // Initialize Timer1 counter
        TIMSK1 = 1; // Enable overflow interrupts (each 65536 * 1/16 us = 4096us)
        sei();
        
        j1979_app_stm_func = &j1979_app_stm_wait;
      }
      break;
  }
}

/*
 * 3.4 j1979_app_stm_wait
 *
 * Description: J1979 wait state
 *
 * input: service - requested service
 *        data - service specific data
 * output: void
 */
void j1979_app_stm_wait( APP_SERVICE_ID service, int data )
{
  LCD_OUTPUT action;

  switch( service )
  {
    case APP_MESSAGE_SENT:
      /*
       * J1850 protocol has successfully sent the request and P2 timer shall be started.
       */
      cli();
      TCNT1 = 0; // Initialize Timer1 counter
      app_timer_counter = 25; // 102,4 msecs count for 100ms P2
      TIMSK1 = 1; // Enable overflow interrupts (each 65536 * 1/16 us = 4096us)
      sei();
      break;

    case APP_MESSAGE_RECEIVED:
      /*
       * Response message is received, forward message info to
       * lcd menu state machine which decodes the message and
       * checks if this is response to sent request.
       * If decoded successfully, cancel J1850 reception.
       * If not, continue in WAIT -state.
       * NOTE: Current implementation supports only one ECU and maximum
       * of 3 results as responses. This needs to be received via
       * single ECU and single message.
       * NOTE2: In order to implement multiple ECU and responses, response
       * array needs to be grown, P2 timer needs to be restarted after each
       * response until all responses are received, and ECU information needs
       * to be tracked to see when all ECUs has responded.
       */
      action = j1979_app_lcd_menuitem( LCD_RESULT, data );
      
      if( action == LCD_MSG_SUCCESS )
      {
        /*
         * Enable button interrupts and stop Application timer
         */
        cli();
        PCICR = 2; // Enable PCINT1 (forPCINT[14:8] pins) interrupt
        PCMSK1 = 7; // Enable individual pins with mask B00000111: PCINT 10,9,8
        TCNT1 = 0; // Initialize Timer1 counter
        TIMSK1 = 0; // Disable overflow interrupt: TOIE1
        sei();
        /*
         * Go to READY -state.
         */
        j1979_app_stm_func = &j1979_app_stm_ready;
      }
      else if( action == LCD_TIMER_START )
      {
        /*
         * Start timer for LCD text delay
         */
        app_timer_counter = 750; // 3 secs count
          
        cli();
        PCICR = 2; // Enable PCINT1 (forPCINT[14:8] pins) interrupt
        PCMSK1 = 7; // Enable individual pins with mask B00000111: PCINT 10,9,8
        TCNT1 = 0; // Initialize Timer1 counter
        TIMSK1 = 1; // Enable overflow interrupts (each 65536 * 1/16 us = 4096us)
        sei();
        /*
         * Go to READY -state.
         */
        j1979_app_stm_func = &j1979_app_stm_ready;
      }
      else if( action == LCD_REFR_TIMER_START )
      {
        /*
         * Start 500ms refresh timer for LCD text delay
         */
        app_timer_counter = 125; // 0.5 secs count
          
        cli();
        PCICR = 2; // Enable PCINT1 (forPCINT[14:8] pins) interrupt
        PCMSK1 = 7; // Enable individual pins with mask B00000111: PCINT 10,9,8
        TCNT1 = 0; // Initialize Timer1 counter
        TIMSK1 = 1; // Enable overflow interrupts (each 65536 * 1/16 us = 4096us)
        sei();
        /*
         * Go to READY -state.
         */
        j1979_app_stm_func = &j1979_app_stm_ready;
      }
      else if( action == LCD_WAIT_RESP )
      {
        /*
         * Disable button interrupts and start 3 second application timer
         * to prevent hangup in case transmission doesn't succeed.
         */
        app_timer_counter = 750; // 3 secs count
        cli();
        PCICR = 0; // Disable PCINT1 (forPCINT[14:8] pins) interrupt
        TCNT1 = 0; // Initialize Timer1 counter
        TIMSK1 = 1; // Enable overflow interrupts (each 65536 * 1/16 us = 4096us)
        sei();
      }
      break;
      
    case APP_TIMER_ELAPSED:
      /*
       * Response wait timer P2 (100ms) or safety timer (3 secs) elapsed, stop application timer,
       * send msg cancel to J1850 and return to main menu.
       * NOTE: According to J1979 specification retry message for the request can be
       * sent after 30 seconds in case response is not received within P2 timer.
       * No more retries for the same request are allowed after one minute from original
       * request. However, in this implementation it is not feasible to wait for 30-60 secs
       * for response. Instead, it is left for the user to apply a new request if needed via
       * GUI menu system.
       */
      cli();
      PCICR = 2; // Enable PCINT1 (forPCINT[14:8] pins) interrupt
      PCMSK1 = 7; // Enable individual pins with mask B00000111: PCINT 10,9,8
      TCNT1 = 0; // Initialize Timer1 counter
      TIMSK1 = 0; // Disable overflow interrupt: TOIE1
      sei();
      
      action = j1979_app_lcd_menuitem( LCD_NO_RESULT, 0 );
      
      if( action == LCD_TIMER_START )
        {
        /*
         * Start timer for LCD text delay
         */
        app_timer_counter = 750; // 3 secs count
          
        cli();
        TCNT1 = 0; // Initialize Timer1 counter
        TIMSK1 = 1; // Enable overflow interrupts (each 65536 * 1/16 us = 4096us)
        sei();
        }
      /*
       * Go to READY -state.
       */
      j1979_app_stm_func = &j1979_app_stm_ready;
      break;
  }
}

/*
 * 3.5 j1979_app_decode_message
 *
 * Description: Decodes received J1979 message
 *
 * input: nbr_of_bytes - message byte count
 *        type - message type to decode
 * output: SUCCESS | FAILURE
 */
int j1979_app_decode_message( int nbr_of_bytes, J1979_APP_DECODER_TYPE type )
{
  int return_value = FAILURE;
  int index = 0;
  int result_index = 0;
  char expected_values[ 12 ];
  float ratio;
  
  switch( type )
  {
    case J1979_APP_DEC_NBR_OF_DTCS:
      /*
       * Check that message is response to correct request:
       * Header byte 1 (Priority/Type): 48
       * Header byte 2 (Target address): 6B
       * Header byte 3 (ECU address): <to be stored>
       * Data byte 1 (Request current powertrain diagnostic data response SID): 41
       * Data byte 2 (PID: Number of emission-related  DTCs and MIL status): 01
       * Data byte 3.. see SAE J2178:
       *   PKT-32-1 Number of Emission-Related Trouble Codes and MIL Status (PRN 0001)
       *   MSB PRN 1000 MIL Status                           1 bit
       *   PRN 1001 Number of Emission-Related Trouble Codes 7 bits <to be stored>
       *   PRN 1002 Continuous Evaluation Supported          8 bits
       *   PRN 1003 Trip Evaluation Supported                8 bits
       *   LSB PRN 1004 Trip Evaluation Complete             8 bits
       */
      memcpy_P( expected_values, ( char* )pgm_read_word( &( j1979_messages[ 8 ] ) ), 12 );
      while( index < nbr_of_bytes )
      {
        /*
         * Store ECU address
         */
        if( index == 2 )
        {
          j1979_app_result[ 0 ][ 0 ] = obd2_message[ index ];
        }
        /*
         * Store number of DTCs
         */
        else if( index == 5 )
        {
          j1979_app_nbr_results = ( obd2_message[ index ] & B01111111 );
          /*
           * Maximum of 3 results supported in current implementation
           */
          if( j1979_app_nbr_results > 3 )
          {
            j1979_app_nbr_results = 3;
          }
          /*
           * Decoding can be stopped as needed info is stored
           */
          return_value = SUCCESS;
          break;
        }
        /*
         * Skips out of while -loop with current return_value.
         */
        else if( obd2_message[ index ] != expected_values[ index ] )
        {
          break;
        }
        index++;
      }
      break;
  
    case J1979_APP_DEC_DTCS:
      /*
       * Check that message is response to correct request:
       * Header byte 1 (Priority/Type): 48
       * Header byte 2 (Target address): 6B
       * Header byte 3 (ECU address): <to be compared>
       * Data byte 1 (Request emission-related DTC response SID): 43
       * Data byte 2: DTC#1 (High Byte)
       * Data byte 3: DTC#1 (Low Byte)
       * Data byte 4: DTC#2 (High Byte)
       * Data byte 5: DTC#2 (Low Byte)
       * Data byte 6: DTC#3 (High Byte)
       * Data byte 7: DTC#3 (Low Byte)
       *   DTC is coded as follows with two bytes each (SAE J2012):
       *   |H01234567|L01234567|
       *   H0+H1 = P_Powertrain (00)|C_Chassis (01)|B_Body (10)|U_Network (11)
       *   H2+H3 = First (decimal) char of the DTC
       *   H4..H7 = Second (hex) char of the DTC
       *   L0..L3 = Third (hex) char of the DTC
       *   L4..L7 = Fourth (hex) char of the DTC
       */
      memcpy_P( expected_values, ( char* )pgm_read_word( &( j1979_messages[ 9 ] ) ), 12 );
      while( index < nbr_of_bytes )
      {
        /*
         * Compare ECU address
         */
        if( index == 2 )
        {
          if( obd2_message[ index ] != j1979_app_result[ 0 ][ 0 ] )
          {
            break;
          }
        }
        
        /*
         * Store DTCs
         */
        else if( index > 3 )
        {
          /*
           * Check message integration for DTC info, i.e there must be two bytes per DTC
           */
          if( ( index + 1 ) >= nbr_of_bytes )
          {
            break;
          }
          /*
           * Store the DTC system (B, C, P, U)
           */
          switch( obd2_message[ index ] >> 6 )
          {
            case 0:
              j1979_app_result[ result_index ][ 1 ] = 'P';
              break;
            case 1:
              j1979_app_result[ result_index ][ 1 ] = 'C';
              break;
            case 2:
              j1979_app_result[ result_index ][ 1 ] = 'B';
              break;
            case 3:
              j1979_app_result[ result_index ][ 1 ] = 'U';
              break;
          }          
          /*
           * Store the DTC set (0-3)
           */
          j1979_app_result[ result_index ][ 2 ] = j1979_app_int_to_char( ( int )( ( obd2_message[ index ] & B00110000 ) >> 4 ) );
          /*
           * Store the first DTC char (0-F)
           */
          j1979_app_result[ result_index ][ 3 ] = j1979_app_int_to_char( ( int )( obd2_message[ index ] & B00001111 ) );

          index++;
          /*
           * Store the second DTC char (0-F)
           */
          j1979_app_result[ result_index ][ 4 ] = j1979_app_int_to_char( ( int )( ( obd2_message[ index ] & B11110000 ) >> 4 ) );
          /*
           * Store the third DTC char (0-F)
           */
          j1979_app_result[ result_index ][ 5 ] = j1979_app_int_to_char( ( int )( obd2_message[ index ] & B00001111 ) );

          result_index++;
          
          /*
           * Check if needed amount of results are stored (max 3 per message)
           */
          if( result_index == j1979_app_nbr_results )
          {
            return_value = SUCCESS;
            break;
          }
        }
        index++;
      }
    break;
    
    case J1979_APP_DEC_CLEAR_DTCS:
      /*
       * Check that message is response to correct request:
       * Header byte 1 (Priority/Type): 48
       * Header byte 2 (Target address): 6B
       * Header byte 3 (ECU address): <not relevant as only one ECU is currently supported>
       * Data byte 1 (Clear/reset emission-related diagnostic information response SID): 44
       */
      memcpy_P( expected_values, ( char* )pgm_read_word( &( j1979_messages[ 10 ] ) ), 12 );
      return_value = SUCCESS;
      /*
       * Loop through the input message from relevant parts, max 4 bytes are needed to check
       */
      while( ( index < nbr_of_bytes ) && ( index < 4 ) )
      {
        /*
         * Index 2 is ECU address and is not relevant
         */
        if( index == 2 )
        {
          index++;
          continue;
        }
        /*
         * Skips out of while -loop with FAILURE return_value.
         */
        else if( obd2_message[ index ] != expected_values[ index ] )
        {
          return_value = FAILURE;
          break;
        }
        index++;
      }
      break;
      
    default: // PID responses
      /*
       * Check that message is response to correct request:
       * Header byte 1 (Priority/Type): 48
       * Header byte 2 (Target address): 6B
       * Header byte 3 (ECU address): <not relevant as only one ECU is currently supported>
       * Data byte 1 (Request current powertrain diagnostic data response SID): 41
       * Data byte 2..x: PID specific data
       */
      memcpy_P( expected_values, ( char* )pgm_read_word( &( j1979_messages[ 11 ] ) ), 12 );
      /*
       * Loop through the input message
       */
      while( index < nbr_of_bytes )
      {
        /*
         * Check that response is correct
         */
        if( ( index == 0 || index == 1 || index == 3 ) &&
          ( obd2_message[ index ] != expected_values[ index ] ) )
          {
            break;
          }
        /*
         * Relevant data part for PID info
         */
        else if( index > 3 )
        {
          switch( type )
          {
            /*
             * Lambda 1 and Lambda 2:
             * Data bytes 2(A) and 3(B) = Oxygen/Fuel ratio [0 - 1.999]
             * Data bytes 4 and 5 = Voltage [0 - 7.999]
             * Calculate ratio as ((A*256)+B)*2/65535
             */
            case J1979_APP_DEC_PID1:
            case J1979_APP_DEC_PID2:
              /*
               * Check message integration for Lambda info, i.e there must be two bytes
               */
              if( ( index + 1 ) >= nbr_of_bytes )
              {
                break;
              }
              ratio = ( unsigned int )( obd2_message[ index ] << 8 ); // A*256
              index++;
              ratio += ( byte )obd2_message[ index ]; // +B
              ratio /= 32768; // ~ *2/65535
              /*
               * Empty result buffer first
               */
              memset( &j1979_app_result[ 0 ][ 0 ], ' ', sizeof( char ) * 6 );
              /*
               * Convert float value to char array as result
               */
              dtostrf( ratio, 1, 3, &j1979_app_result[ 0 ][ 0 ] );
              return_value = SUCCESS;
              break;
              
            /*
             * Engine coolant temperatur in Celsius:
             * Data byte 2(A) = Temp in unsigned val [0 - 255]
             * Calculate temperature as A-40
             */
            case J1979_APP_DEC_PID3:
              ratio = ( byte )obd2_message[ index ];
              ratio -= 40;
              /*
               * Empty result buffer first
               */
              memset( &j1979_app_result[ 0 ][ 0 ], ' ', sizeof( char ) * 6 );
              dtostrf( ratio, 3, 0, &j1979_app_result[ 0 ][ 0 ] );
              j1979_app_result[ 0 ][ 3 ] = ' ';
              j1979_app_result[ 0 ][ 4 ] = ( char )223; // degree character
              j1979_app_result[ 0 ][ 5 ] = 'C';
              return_value = SUCCESS;
              break;
            
            /*
             * Throttle position in percentage:
             * Data byte 2(A) = Position [ 0 - 255 ] in 1/2.55 steps
             * Calculated as A*100/255
             */
            case J1979_APP_DEC_PID4:
              ratio = ( byte )obd2_message[ index ];
              ratio = ratio * 100 / 255;
              /*
               * Empty result buffer first
               */
              memset( &j1979_app_result[ 0 ][ 0 ], ' ', sizeof( char ) * 6 );
              dtostrf( ratio, 5, 1, &j1979_app_result[ 0 ][ 0 ] );
              j1979_app_result[ 0 ][ 5 ] = '%';
              return_value = SUCCESS;
              break;
            
            /*
             * Tachometer, engine RPM:
             * Data byte 2(A) and 3(B) = RPM in 16bit value of 0.25 round steps [0 - 16383.75]
             * RPM is calculated as ((A*256)+B)/4
             */
            case J1979_APP_DEC_PID5:
              /*
               * Check message integration for RPM info, i.e there must be two bytes
               */
              if( ( index + 1 ) >= nbr_of_bytes )
              {
                break;
              }
              ratio = ( unsigned int )( obd2_message[ index ] << 8 ); // A*256
              index++;
              ratio += ( byte )obd2_message[ index ]; // +B
              ratio /= 4; // /4
              /*
               * Empty result buffer first
               */
              memset( &j1979_app_result[ 0 ][ 0 ], ' ', sizeof( char ) * 6 );
              /*
               * Convert float value to char array as result, 5 chars for max on 16384, decimals ignored
               */
              dtostrf( ratio, 6, 0, &j1979_app_result[ 0 ][ 0 ] );
              return_value = SUCCESS;
              break;
          }
        }
        /*
         * All needed info stored
         */
        if( return_value == SUCCESS )
        {
          break;
        }
        index++;
      }
      break;
  }

  return return_value;
}

/*
 * 3.6 j1979_app_int_to_char
 *
 * Description: Converts integer to hexadecimal character for LCD string
 *
 * input: nbr - integer to convert (0-15)
 * output: converted character ('0'-'F')
 */
char j1979_app_int_to_char( int nbr )
{
  /*
   * Sanity check
   */
  if( nbr > 15 || nbr < 0 )
  {
    return 0;
  }
  
  /*
   * Characters from '0' to '9'
   */
  if( nbr < 10 )
  {
    return( ( char )( ( ( int ) '0' ) + nbr ) );
  }
  /*
   * Characters from 'A' to 'F'
   */
  else
  {
    return( ( char )( ( ( int ) 'A' ) + ( nbr - 10 ) ) );
  }
}
  
/*
 * 3.7 j1979_app_lcd_menuitem
 *
 * Description: LCD menu statemachine and io handling
 *
 * input: input - LCD stm input
 *        data - Input specific data
 * output: LCD output value
 */
LCD_OUTPUT j1979_app_lcd_menuitem( LCD_INPUT input, int data )
{
  LCD_OUTPUT return_value = LCD_INVALID_OUTPUT;

  switch( lcd_state )
  {
    case LCD_INVALID_STATE:
      /*
       * Print menu item 1 to LCD
       */
      strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 1 ] ) ) );
      j1979_app_menu_print( lcd_text_buffer );
      strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
      lcd_state = LCD_DTC_STATE;
      break;
      
    case LCD_DTC_STATE:
      if( input == LCD_BUTTON_SELECT )
      {
        /*
         * Print menu item 0 "Please wait" to LCD
         */
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 0 ] ) ) );
        j1979_app_menu_print( lcd_text_buffer );
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
        /*
         * Initialize number of results
         */
        j1979_app_nbr_results = 0;
        /*
         * Copy nbr_dtc message from Flash to SRAM into global message buffer and send
         * the message to J1850 protocol with message length in bytes.
         * J1979 Service $03 - Request Emission-Related Diagnostic Trouble Codes:
         * Step 1: Request to get number of emission-related DTCs from all ECUs
         */
        memcpy_P( obd2_message, ( char* )pgm_read_word( &( j1979_messages[ 0 ] ) ), 5 );
        j1850_dl_input_msg( DATA_LINK_SEND_MESSAGE, 5 );
        return_value = LCD_WAIT_RESP;
      }
      else if( input == LCD_RESULT )
      {
        /*
         * Result shall be response for nbr of emission-related DTCs.
         * NOTE: Only one ECU is supported, i.e one answer!
         */
        if( j1979_app_decode_message( data, J1979_APP_DEC_NBR_OF_DTCS ) == SUCCESS )
        {
          /*
           * If message was correct one, stop reception from J1850
           */
          j1850_dl_input_msg( DATA_LINK_CANCEL_MESSAGE, 0 );
          
          if( j1979_app_nbr_results == 0 )
          {
            /*
             * Print menu item 15 "no DTCs" to LCD and start 3 sec timer by returning LCD_TIMER_START info
             */
            strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 15 ] ) ) );
            j1979_app_menu_print( lcd_text_buffer );
            strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
            return_value = LCD_TIMER_START;
          }
          else
          {
            /*
             * J1979 Service $03 - Request Emission-Related Diagnostic Trouble Codes:
             * Step 2: Send request for each DTCs
             */
            memcpy_P( obd2_message, ( char* )pgm_read_word( &( j1979_messages[ 1 ] ) ), 4 );        
            j1850_dl_input_msg( DATA_LINK_SEND_MESSAGE, 4 );
            return_value = LCD_WAIT_RESP;
            lcd_state = LCD_DTC1_STATE;
          }
        }
      }
      else if( input == LCD_BUTTON_NEXT )
      {
        /*
         * Print menuitem 5 "Clear DTCs"
         */
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 5 ] ) ) );
        j1979_app_menu_print( lcd_text_buffer );
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
        lcd_state = LCD_CLEAR_DTC_STATE;
      }
      else if( input == LCD_NO_RESULT )
      {
        /*
         * If no message was received in time, stop reception from J1850
         */
        j1850_dl_input_msg( DATA_LINK_CANCEL_MESSAGE, 0 );
        
        /*
         * Print menu item 13 "error" to LCD and start 3 sec timer by returning LCD_TIMER_START info
         */
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 13 ] ) ) );
        j1979_app_menu_print( lcd_text_buffer );
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
        return_value = LCD_TIMER_START;
      }
      else // LCD timer elapsed or actionless button pressed -> print current LCD menuitem
      {
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 1 ] ) ) );
        j1979_app_menu_print( lcd_text_buffer );
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
      }
      break;
      
    case LCD_DTC1_STATE:
      if( input == LCD_BUTTON_NEXT )
      {
        if( j1979_app_nbr_results > 1 )
        {
          /*
           * Print second DTC
           */
          strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 3 ] ) ) );
          lcd_text_buffer[ 6 ] = ( char )( ( ( int )'0' ) + j1979_app_nbr_results );
          memcpy( &lcd_text_buffer[ 16 ], &j1979_app_result[ 1 ][ 1 ], sizeof( char ) * 5 );
          j1979_app_menu_print( lcd_text_buffer );
          strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
          lcd_state = LCD_DTC2_STATE;
        }
      }
      else if( input == LCD_BUTTON_EXIT )
      {
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 1 ] ) ) );
        j1979_app_menu_print( lcd_text_buffer );
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
        lcd_state = LCD_DTC_STATE;
      }
      else if( input == LCD_RESULT )
      {
        if( j1979_app_decode_message( data, J1979_APP_DEC_DTCS ) == SUCCESS )
        {
          /*
           * If message was correct one, stop reception from J1850
           */
          j1850_dl_input_msg( DATA_LINK_CANCEL_MESSAGE, 0 );
          /*
           * Print first DTC
           */
          strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 2 ] ) ) );
          lcd_text_buffer[ 6 ] = ( char )( ( ( int )'0' ) + j1979_app_nbr_results );
          memcpy( &lcd_text_buffer[ 16 ], &j1979_app_result[ 0 ][ 1 ], sizeof( char ) * 5 );
          j1979_app_menu_print( lcd_text_buffer );
          strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
          return_value = LCD_MSG_SUCCESS;
        }
      }
      else if( input == LCD_NO_RESULT )
      {
        /*
         * If no message was received in time, stop reception from J1850
         */
        j1850_dl_input_msg( DATA_LINK_CANCEL_MESSAGE, 0 );
        
        /*
         * Print menu item 13 "error" to LCD and start 3 sec timer by returning LCD_TIMER_START info
         */
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 13 ] ) ) );
        j1979_app_menu_print( lcd_text_buffer );
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
        return_value = LCD_TIMER_START;
        lcd_state = LCD_DTC_STATE;
      }
      // LCD_BUTTON_SELECT does nothing
      break;
      
    case LCD_DTC2_STATE:
      if( input == LCD_BUTTON_NEXT )
      {
        if( j1979_app_nbr_results > 2 )
        {
          /*
           * Print third DTC, maximum of three DTCs are supported in
           * current implementation, i.e one response for DTC request.
           */
          strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 4 ] ) ) );
          lcd_text_buffer[ 6 ] = ( char )( ( ( int )'0' ) + j1979_app_nbr_results );
          memcpy( &lcd_text_buffer[ 16 ], &j1979_app_result[ 2 ][ 1 ], sizeof( char ) * 5 );
          j1979_app_menu_print( lcd_text_buffer );
          strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
          lcd_state = LCD_DTC3_STATE;
        }
        else
        {
          /*
           * Print first DTC
           */
          strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 2 ] ) ) );
          lcd_text_buffer[ 6 ] = ( char )( ( ( int )'0' ) + j1979_app_nbr_results );
          memcpy( &lcd_text_buffer[ 16 ], &j1979_app_result[ 0 ][ 1 ], sizeof( char ) * 5 );
          j1979_app_menu_print( lcd_text_buffer );
          strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
          lcd_state = LCD_DTC1_STATE;
        }
      }
      else if( input == LCD_BUTTON_EXIT )
      {
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 1 ] ) ) );
        j1979_app_menu_print( lcd_text_buffer );
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
        lcd_state = LCD_DTC_STATE;
      }
      // LCD_BUTTON_SELECT does nothing
      break;
      
    case LCD_DTC3_STATE:
      if( input == LCD_BUTTON_NEXT )
      {
        /*
         * Print first DTC
         */
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 2 ] ) ) );
        lcd_text_buffer[ 6 ] = ( char )( ( ( int )'0' ) + j1979_app_nbr_results );
        memcpy( &lcd_text_buffer[ 16 ], &j1979_app_result[ 0 ][ 1 ], sizeof( char ) * 5 );
        j1979_app_menu_print( lcd_text_buffer );
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
        lcd_state = LCD_DTC1_STATE;
      }
      else if( input == LCD_BUTTON_EXIT )
      {
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 1 ] ) ) );
        j1979_app_menu_print( lcd_text_buffer );
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
        lcd_state = LCD_DTC_STATE;
      }
      // LCD_BUTTON_SELECT does nothing
      break;
    
    case LCD_CLEAR_DTC_STATE:
      if( input == LCD_BUTTON_SELECT )
      {
        /*
         * Print menu item 0 "Please wait" to LCD
         */
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 0 ] ) ) );
        j1979_app_menu_print( lcd_text_buffer );
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
        /*
         * Copy clear_dtc message from Flash to SRAM into global message buffer and send
         * the message to J1850 protocol with message length in bytes.
         * J1979 Service $04 - Request Clear/Reset Emission-Related Diagnostic Information
         */
        memcpy_P( obd2_message, ( char* )pgm_read_word( &( j1979_messages[ 2 ] ) ), 4 );
        j1850_dl_input_msg( DATA_LINK_SEND_MESSAGE, 4 );
        return_value = LCD_WAIT_RESP;
      }
      else if( input == LCD_RESULT )
      {
        if( j1979_app_decode_message( data, J1979_APP_DEC_CLEAR_DTCS ) == SUCCESS )
        {
          /*
           * If message was correct one, stop reception from J1850
           */
          j1850_dl_input_msg( DATA_LINK_CANCEL_MESSAGE, 0 );
          /*
           * Print menu item 12 "DTCs cleared" to LCD and start 3 sec timer by returning LCD_TIMER_START info
           */
          strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 12 ] ) ) );
          j1979_app_menu_print( lcd_text_buffer );
          strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
          return_value = LCD_TIMER_START;
          lcd_state = LCD_DTC_STATE;
        }
      }
      else if( input == LCD_NO_RESULT )
      {
        /*
         * If no message was received in time, stop reception from J1850
         */
        j1850_dl_input_msg( DATA_LINK_CANCEL_MESSAGE, 0 );
        
        /*
         * Print menu item 13 "error" to LCD and start 3 sec timer by returning LCD_TIMER_START info
         */
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 13 ] ) ) );
        j1979_app_menu_print( lcd_text_buffer );
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
        return_value = LCD_TIMER_START;
        lcd_state = LCD_DTC_STATE;
      }
      else if( input == LCD_BUTTON_NEXT )
      {
        /*
         * Print menuitem 6 "Read PIDs"
         */
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 6 ] ) ) );
        j1979_app_menu_print( lcd_text_buffer );
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
        lcd_state = LCD_PID_STATE;
      }
      // LCD_BUTTON_EXIT does nothing
      break;

    case LCD_PID_STATE:
      if( input == LCD_BUTTON_SELECT )
      {
        /*
         * Print menu item 0 "Please wait" to LCD
         */
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 0 ] ) ) );
        j1979_app_menu_print( lcd_text_buffer );
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
        /*
         * Copy read_pid1 message from Flash to SRAM into global message buffer and send
         * the message to J1850 protocol with message length in bytes.
         * J1979 Service $01 - Current powertrain Diagnostic Data Request
         */
        memcpy_P( obd2_message, ( char* )pgm_read_word( &( j1979_messages[ 3 ] ) ), 5 );
        j1850_dl_input_msg( DATA_LINK_SEND_MESSAGE, 5 );
        return_value = LCD_WAIT_RESP;
        /*
         * Receive response in PID1 state
         */
        lcd_state = LCD_PID1_STATE;
      }
      else if( input == LCD_BUTTON_NEXT )
      {
        /*
         * Print menu item 1 to LCD
         */
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 1 ] ) ) );
        j1979_app_menu_print( lcd_text_buffer );
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
        lcd_state = LCD_DTC_STATE;
      }
      // LCD_BUTTON_EXIT does nothing
      break;

    case LCD_PID1_STATE: //Lambda1 (Bank1)
      if( input == LCD_TIMER_ELAPSED )
      {
        /*
         * Copy read_pid1 message from Flash to SRAM into global message buffer and send
         * the message to J1850 protocol with message length in bytes.
         * J1979 Service $01 - Current powertrain Diagnostic Data Request
         */
        memcpy_P( obd2_message, ( char* )pgm_read_word( &( j1979_messages[ 3 ] ) ), 5 );
        j1850_dl_input_msg( DATA_LINK_SEND_MESSAGE, 5 );
        return_value = LCD_WAIT_RESP;
      }
      else if( input == LCD_BUTTON_NEXT )
      {
        /*
         * Copy read_pid2 message from Flash to SRAM into global message buffer and send
         * the message to J1850 protocol with message length in bytes.
         * J1979 Service $01 - Current powertrain Diagnostic Data Request
         */
        memcpy_P( obd2_message, ( char* )pgm_read_word( &( j1979_messages[ 4 ] ) ), 5 );
        j1850_dl_input_msg( DATA_LINK_SEND_MESSAGE, 5 );
        return_value = LCD_WAIT_RESP;
        /*
         * Receive response in PID2 state
         */
        lcd_state = LCD_PID2_STATE;
      }
      else if( input == LCD_RESULT )
      {
        if( j1979_app_decode_message( data, J1979_APP_DEC_PID1 ) == SUCCESS )
        {
          /*
           * If message was correct one, stop reception from J1850
           */
          j1850_dl_input_msg( DATA_LINK_CANCEL_MESSAGE, 0 );
          /*
           * Print the PID1 and start 500ms timer before refresh
           */
          strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 7 ] ) ) );
          memcpy( &lcd_text_buffer[ 16 ], &j1979_app_result[ 0 ][ 0 ], sizeof( char ) * 5 );
          j1979_app_menu_print( lcd_text_buffer );
          strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
          return_value = LCD_REFR_TIMER_START;
        }
      }
      else if( input == LCD_NO_RESULT )
      {
        /*
         * If no message was received in time, stop reception from J1850
         */
        j1850_dl_input_msg( DATA_LINK_CANCEL_MESSAGE, 0 );        
        /*
         * Print menu item 13 "error" to LCD and start 3 sec timer by returning LCD_TIMER_START info
         */
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 13 ] ) ) );
        j1979_app_menu_print( lcd_text_buffer );
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
        return_value = LCD_TIMER_START;
        lcd_state = LCD_DTC_STATE;
      }
      else if( input == LCD_BUTTON_EXIT )
      {
        /*
         * Go to read PIDs main menu
         */
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 6 ] ) ) );
        j1979_app_menu_print( lcd_text_buffer );
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
        lcd_state = LCD_PID_STATE;
      }
      break;

    case LCD_PID2_STATE: //Lambda2 (Bank2)
      if( input == LCD_TIMER_ELAPSED )
      {
        /*
         * Copy read_pid2 message from Flash to SRAM into global message buffer and send
         * the message to J1850 protocol with message length in bytes.
         * J1979 Service $01 - Current powertrain Diagnostic Data Request
         */
        memcpy_P( obd2_message, ( char* )pgm_read_word( &( j1979_messages[ 4 ] ) ), 5 );
        j1850_dl_input_msg( DATA_LINK_SEND_MESSAGE, 5 );
        return_value = LCD_WAIT_RESP;
      }
      else if( input == LCD_BUTTON_NEXT )
      {
        /*
         * Copy read_pid3 message from Flash to SRAM into global message buffer and send
         * the message to J1850 protocol with message length in bytes.
         * J1979 Service $01 - Current powertrain Diagnostic Data Request
         */
        memcpy_P( obd2_message, ( char* )pgm_read_word( &( j1979_messages[ 5 ] ) ), 5 );
        j1850_dl_input_msg( DATA_LINK_SEND_MESSAGE, 5 );
        return_value = LCD_WAIT_RESP;
        /*
         * Receive response in PID3 state
         */
        lcd_state = LCD_PID3_STATE;
      }
      else if( input == LCD_RESULT )
      {
        if( j1979_app_decode_message( data, J1979_APP_DEC_PID2 ) == SUCCESS )
        {
          /*
           * If message was correct one, stop reception from J1850
           */
          j1850_dl_input_msg( DATA_LINK_CANCEL_MESSAGE, 0 );
          /*
           * Print the PID2 and start 500ms timer before refresh
           */
          strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 8 ] ) ) );
          memcpy( &lcd_text_buffer[ 16 ], &j1979_app_result[ 0 ][ 0 ], sizeof( char ) * 5 );
          j1979_app_menu_print( lcd_text_buffer );
          strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
          return_value = LCD_REFR_TIMER_START;
        }
      }
      else if( input == LCD_NO_RESULT )
      {
        /*
         * If no message was received in time, stop reception from J1850
         */
        j1850_dl_input_msg( DATA_LINK_CANCEL_MESSAGE, 0 );        
        /*
         * Print menu item 13 "error" to LCD and start 3 sec timer by returning LCD_TIMER_START info
         */
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 13 ] ) ) );
        j1979_app_menu_print( lcd_text_buffer );
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
        return_value = LCD_TIMER_START;
        lcd_state = LCD_DTC_STATE;
      }
      else if( input == LCD_BUTTON_EXIT )
      {
        /*
         * Go to read PIDs main menu
         */
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 6 ] ) ) );
        j1979_app_menu_print( lcd_text_buffer );
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
        lcd_state = LCD_PID_STATE;
      }
      break;
      
    case LCD_PID3_STATE: //Coolant temp
      if( input == LCD_TIMER_ELAPSED )
      {
        /*
         * Copy read_pid3 message from Flash to SRAM into global message buffer and send
         * the message to J1850 protocol with message length in bytes.
         * J1979 Service $01 - Current powertrain Diagnostic Data Request
         */
        memcpy_P( obd2_message, ( char* )pgm_read_word( &( j1979_messages[ 5 ] ) ), 5 );
        j1850_dl_input_msg( DATA_LINK_SEND_MESSAGE, 5 );
        return_value = LCD_WAIT_RESP;
      }
      else if( input == LCD_BUTTON_NEXT )
      {
        /*
         * Copy read_pid4 message from Flash to SRAM into global message buffer and send
         * the message to J1850 protocol with message length in bytes.
         * J1979 Service $01 - Current powertrain Diagnostic Data Request
         */
        memcpy_P( obd2_message, ( char* )pgm_read_word( &( j1979_messages[ 6 ] ) ), 5 );
        j1850_dl_input_msg( DATA_LINK_SEND_MESSAGE, 5 );
        return_value = LCD_WAIT_RESP;
        /*
         * Receive response in PID4 state
         */
        lcd_state = LCD_PID4_STATE;
      }
      else if( input == LCD_RESULT )
      {
        if( j1979_app_decode_message( data, J1979_APP_DEC_PID3 ) == SUCCESS )
        {
          /*
           * If message was correct one, stop reception from J1850
           */
          j1850_dl_input_msg( DATA_LINK_CANCEL_MESSAGE, 0 );
          /*
           * Print the PID3 and start 500ms timer before refresh
           */
          strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 9 ] ) ) );
          memcpy( &lcd_text_buffer[ 16 ], &j1979_app_result[ 0 ][ 0 ], sizeof( char ) * 6 );
          j1979_app_menu_print( lcd_text_buffer );
          strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
          return_value = LCD_REFR_TIMER_START;
        }
      }
      else if( input == LCD_NO_RESULT )
      {
        /*
         * If no message was received in time, stop reception from J1850
         */
        j1850_dl_input_msg( DATA_LINK_CANCEL_MESSAGE, 0 );        
        /*
         * Print menu item 13 "error" to LCD and start 3 sec timer by returning LCD_TIMER_START info
         */
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 13 ] ) ) );
        j1979_app_menu_print( lcd_text_buffer );
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
        return_value = LCD_TIMER_START;
        lcd_state = LCD_DTC_STATE;
      }
      else if( input == LCD_BUTTON_EXIT )
      {
        /*
         * Go to read PIDs main menu
         */
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 6 ] ) ) );
        j1979_app_menu_print( lcd_text_buffer );
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
        lcd_state = LCD_PID_STATE;
      }
      break;
      
    case LCD_PID4_STATE: //Throttle pos
      if( input == LCD_TIMER_ELAPSED )
      {
        /*
         * Copy read_pid4 message from Flash to SRAM into global message buffer and send
         * the message to J1850 protocol with message length in bytes.
         * J1979 Service $01 - Current powertrain Diagnostic Data Request
         */
        memcpy_P( obd2_message, ( char* )pgm_read_word( &( j1979_messages[ 6 ] ) ), 5 );
        j1850_dl_input_msg( DATA_LINK_SEND_MESSAGE, 5 );
        return_value = LCD_WAIT_RESP;
      }
      else if( input == LCD_BUTTON_NEXT )
      {
        /*
         * Copy read_pid5 message from Flash to SRAM into global message buffer and send
         * the message to J1850 protocol with message length in bytes.
         * J1979 Service $01 - Current powertrain Diagnostic Data Request
         */
        memcpy_P( obd2_message, ( char* )pgm_read_word( &( j1979_messages[ 7 ] ) ), 5 );
        j1850_dl_input_msg( DATA_LINK_SEND_MESSAGE, 5 );
        return_value = LCD_WAIT_RESP;
        /*
         * Receive response in PID5 state
         */
        lcd_state = LCD_PID5_STATE;
      }
      else if( input == LCD_RESULT )
      {
        if( j1979_app_decode_message( data, J1979_APP_DEC_PID4 ) == SUCCESS )
        {
          /*
           * If message was correct one, stop reception from J1850
           */
          j1850_dl_input_msg( DATA_LINK_CANCEL_MESSAGE, 0 );
          /*
           * Print the PID4 and start 500ms timer before refresh
           */
          strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 10 ] ) ) );
          memcpy( &lcd_text_buffer[ 16 ], &j1979_app_result[ 0 ][ 0 ], sizeof( char ) * 6 );
          j1979_app_menu_print( lcd_text_buffer );
          strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
          return_value = LCD_REFR_TIMER_START;
        }
      }
      else if( input == LCD_NO_RESULT )
      {
        /*
         * If no message was received in time, stop reception from J1850
         */
        j1850_dl_input_msg( DATA_LINK_CANCEL_MESSAGE, 0 );        
        /*
         * Print menu item 13 "error" to LCD and start 3 sec timer by returning LCD_TIMER_START info
         */
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 13 ] ) ) );
        j1979_app_menu_print( lcd_text_buffer );
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
        return_value = LCD_TIMER_START;
        lcd_state = LCD_DTC_STATE;
      }
      else if( input == LCD_BUTTON_EXIT )
      {
        /*
         * Go to read PIDs main menu
         */
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 6 ] ) ) );
        j1979_app_menu_print( lcd_text_buffer );
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
        lcd_state = LCD_PID_STATE;
      }
      break;
    
    case LCD_PID5_STATE: //Tachometer
      if( input == LCD_TIMER_ELAPSED )
      {
        /*
         * Copy read_pid5 message from Flash to SRAM into global message buffer and send
         * the message to J1850 protocol with message length in bytes.
         * J1979 Service $01 - Current powertrain Diagnostic Data Request
         */
        memcpy_P( obd2_message, ( char* )pgm_read_word( &( j1979_messages[ 7 ] ) ), 5 );
        j1850_dl_input_msg( DATA_LINK_SEND_MESSAGE, 5 );
        return_value = LCD_WAIT_RESP;
      }
      else if( input == LCD_BUTTON_NEXT )
      {
        /*
         * Copy read_pid1 message from Flash to SRAM into global message buffer and send
         * the message to J1850 protocol with message length in bytes.
         * J1979 Service $01 - Current powertrain Diagnostic Data Request
         */
        memcpy_P( obd2_message, ( char* )pgm_read_word( &( j1979_messages[ 3 ] ) ), 5 );
        j1850_dl_input_msg( DATA_LINK_SEND_MESSAGE, 5 );
        return_value = LCD_WAIT_RESP;
        /*
         * Receive response in PID1 state
         */
        lcd_state = LCD_PID1_STATE;
      }
      else if( input == LCD_RESULT )
      {
        if( j1979_app_decode_message( data, J1979_APP_DEC_PID5 ) == SUCCESS )
        {
          /*
           * If message was correct one, stop reception from J1850
           */
          j1850_dl_input_msg( DATA_LINK_CANCEL_MESSAGE, 0 );
          /*
           * Print the PID5 and start 500ms timer before refresh
           */
          strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 11 ] ) ) );
          memcpy( &lcd_text_buffer[ 16 ], &j1979_app_result[ 0 ][ 0 ], sizeof( char ) * 6 );
          j1979_app_menu_print( lcd_text_buffer );
          strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
          return_value = LCD_REFR_TIMER_START;
        }
      }
      else if( input == LCD_NO_RESULT )
      {
        /*
         * If no message was received in time, stop reception from J1850
         */
        j1850_dl_input_msg( DATA_LINK_CANCEL_MESSAGE, 0 );        
        /*
         * Print menu item 13 "error" to LCD and start 3 sec timer by returning LCD_TIMER_START info
         */
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 13 ] ) ) );
        j1979_app_menu_print( lcd_text_buffer );
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
        return_value = LCD_TIMER_START;
        lcd_state = LCD_DTC_STATE;
      }
      else if( input == LCD_BUTTON_EXIT )
      {
        /*
         * Go to read PIDs main menu
         */
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 6 ] ) ) );
        j1979_app_menu_print( lcd_text_buffer );
        strcpy_P( lcd_text_buffer, ( char* )pgm_read_word( &( menuitems[ 14 ] ) ) );
        lcd_state = LCD_PID_STATE;
      }
      break;
  }
  return return_value;
}

/*
 * 3.8 j1979_app_menu_print
 *
 * Description: LCD menu writing functionality
 *
 * input: input_string - pointer to char string to be written
 * output: void
 */
void j1979_app_menu_print( char* input_string )
{
  int i = 0;
  
  lcd.clear( );
  
  /*
   * Write the text in two rows
   */
  for( ; i < 16; i++ )
  {
    lcd.setCursor( i, 0 );
    lcd.write( input_string[ i ] );
  }  
  for( ; i < 32; i++ )
  {
    lcd.setCursor( i - 16, 1 );
    lcd.write( input_string[ i ] );
  }
}
    
/*
 * 4.1 Timer1 (Application timer) Overflow Int
 *
 * Description: OVF interrupt for 16-bit Timer1
 */
ISR( TIMER1_OVF_vect )
{
  app_timer_counter--;
  /*
   * Disable interrupt and send timeout message
   */
  if( app_timer_counter == 0 )
  {
    TIMSK1 = 0;
    j1979_app_input_msg( APP_TIMER_ELAPSED, 0 );
  }
}

/*
 * 4.2 Pin change interrupt on port C
 *
 * Description: Interrupt for buttons
 */
ISR( PCINT1_vect )
{
  /*
   * Check which button was pressed
   */
  if( digitalRead( A0 ) == 1 )
  {
    j1979_app_input_msg( APP_BUTTON_PRESSED, LCD_BUTTON_SELECT );
  }
  else if( digitalRead( A1 ) == 1 )
  {
    j1979_app_input_msg( APP_BUTTON_PRESSED, LCD_BUTTON_NEXT );
  }
  else if( digitalRead( A2 ) == 1 )
  {
    j1979_app_input_msg( APP_BUTTON_PRESSED, LCD_BUTTON_EXIT );
  }
  // else - do nothing. This happens when button is depressed.
}

