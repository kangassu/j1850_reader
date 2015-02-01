/*
 * Project: OBD II reader with Arduino
 *
 * Author: Tommi Kangassuo (kangassu@gmail.com)
 *
 * Work for LUT CT10A4000
 */

/*
 * Main file for the project, overall control of interfaces
 *
 * Contents:
 * 1. Inclusions
 * 2. Constants and global variables
 * 3. Function list:
 * 3.1 setup
 * 3.2 loop 
 */

/*
 * 1. Inclusions, needed only in main file
 */
#include <avr/pgmspace.h>
#include <LiquidCrystal.h>
#include "j1850_vpw.h"
#include "j1979_lcd_defs.h"

/*
 * 2. Constants and global variables
 */

/*
 * 3. Functions
 */

/*
 * 3.1 setup
 */
void setup()
{
  /*
   * Initialize peripherals
   */
  obd2_messaging_init( );
  j1979_app_init( );
  j1850_dl_init( );
  j1850_vpw_init( );
}

/*
 * 3.2 loop
 */ 
void loop()
{
  while( 1 ) // reduces loop() functionality from 22us to 5us loops
  {
    /*
     * Check entity mailboxes in priority order
     */
    j1850_check_queue( );
  }
}
