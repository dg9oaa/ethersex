/*
*
* Copyright (c) 2009 by Jonny Roeker <dg9oaa@darc.de>
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 3
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*
* For more information on the GPL, please go to:
* http://www.gnu.org/copyleft/gpl.html
*/

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <avr/pgmspace.h>
#include <stdbool.h>

#include "config.h"
#include "core/debug.h"
#include "core/eeprom.h"

#include "protocols/ecmd/ecmd-base.h"

#include "hardware/rotor/rotor.h"


char* rtstr[] = {"not defined", "hold", "cw", "ccw"};

/**
* rotor status
* returns String cw,speed=75, preset=100, alpha=89
*/
int16_t
parse_cmd_rotor_status(char *cmd, char *output, uint16_t len)
{

  return ECMD_FINAL(snprintf_P(output, len, PSTR("%-4s az=%d el=%d v=%d ad0=%d"),
                               rtstr[rot.az_movement],
			       rot.azimuth,
			       rot.elevation,
                               rot.speed,
                               get_adc(0)
			       ));
}

int16_t
parse_cmd_rotor_state(char *cmd, char *output, uint16_t len)
{
#ifdef ROTOR_HAM4_SUPPORT
  return ECMD_FINAL(snprintf_P(output, len, PSTR("-180/180 0/90")));
#else
  return ECMD_FINAL(snprintf_P(output, len, PSTR("0/360 0/90")));
#endif
}

/**
* rotor preset DIRECTION SPEED
* @param azimuth in degree
* @param elevation in degree
* @param speed in percent
*/
int16_t
parse_cmd_rotor_move(char *cmd, char *output, uint16_t len)
{
  uint16_t speed;
  int16_t az_angle;
  int16_t el_angle;
  
  speed = 0;
  az_angle = 0;
  el_angle = 0;

  while(*cmd == ' ') cmd++;

  int ret = sscanf_P(cmd, PSTR("%d %d %d"), &az_angle, &el_angle, &speed);
  if (ret < 1)
    return ECMD_ERR_PARSE_ERROR;

#ifdef DEBUG_ROTOR
       debug_printf("ROTOR parse move: az=%d el=%d v=%d\n", az_angle, el_angle, speed);
#endif

  if (az_angle > 180)
    rot.az_preset = (360 - az_angle) * -1;
  else
    rot.az_preset = az_angle;

  if (ret >= 2) {
    rot.el_preset = el_angle;
    rot.elevation = el_angle;
  }
  if (ret >= 3)
    rot.speed = speed;

  uint8_t mvto = get_az_movement();

  rotor_turn(mvto, AUTO);
  
  return ECMD_FINAL(snprintf_P(output, len, PSTR("OK az=%d el=%d v=%d move=%s"),
    rot.az_preset, rot.el_preset, rot.speed, rtstr[mvto]));
}


/**
* rotor cw
* @params [speed]  in percent
*/
int16_t
parse_cmd_rotor_cw(char *cmd, char *output, uint16_t len)
{
  while(*cmd == ' ') cmd++;
    if(strlen(cmd) > 0) {
      rot.speed = atol(cmd);
    }
  
  rotor_turn(CW, MANUEL);
  return ECMD_FINAL(snprintf_P(output, len, PSTR("OK move=%s speed=%d"), rtstr[CW], rot.speed));

}


/**
* rotor ccw
* @params [speed]  in percent
*/
int16_t parse_cmd_rotor_ccw(char *cmd, char *output, uint16_t len)
{
  while(*cmd == ' ') cmd++;
    if(strlen(cmd) > 0) {
      rot.speed = atol(cmd);
     }

  rotor_turn(CCW, MANUEL);
  
  return ECMD_FINAL(snprintf_P(output, len, PSTR("OK move=%s speed=%d"), rtstr[CCW], rot.speed));
}

/**
* rotor stop
*/
int16_t parse_cmd_rotor_stop(char *cmd, char *output, uint16_t len)
{
  rotor_stop(true);
  return ECMD_FINAL_OK;
}

/**
* rotor park
*/
int16_t
parse_cmd_rotor_park(char *cmd, char *output, uint16_t len)
{
  rotor_park();
  return ECMD_FINAL_OK;
}

/**
* rotor calibrate
* @param min max
*/
int16_t parse_cmd_rotor_cal_azimuth(char *cmd, char *output, uint16_t len)
{
  uint16_t min;
  uint16_t max;
  while(*cmd == ' ') cmd++;

  if (sscanf_P(cmd, PSTR("%d %d"), &min, &max) != 2)
    return ECMD_ERR_PARSE_ERROR;

  eeprom_save_int(rotor_azimuth_min, min);
  eeprom_save_int(rotor_azimuth_max, max);
  eeprom_update_chksum();
  return ECMD_FINAL_OK;
}

/**
* rotor get calibrate
* returns String min:0 max:1023
*/
int16_t
parse_cmd_rotor_getcal_azimuth(char *cmd, char *output, uint16_t len)
{
  uint16_t min_store, max_store;
  eeprom_restore_int(rotor_azimuth_min, &min_store);
  eeprom_restore_int(rotor_azimuth_max, &max_store);
 
  return ECMD_FINAL(snprintf_P(output, len, PSTR("min:%d max:%d"), min_store, max_store ));
}

/**
* set park parameter azimuth elevation
*/
int16_t
parse_cmd_rotor_parkpos(char *cmd, char *output, uint16_t len)
{
  int16_t azpos;
  int16_t elpos;
  while(*cmd == ' ') cmd++;

  if (sscanf_P(cmd, PSTR("%d %d"), &azpos, &elpos) != 2)
    return ECMD_ERR_PARSE_ERROR;

  if (azpos > 180)
    azpos = (360 - azpos) * -1;

  eeprom_save_int(rotor_azimuth_parkpos, azpos);
  eeprom_save_int(rotor_elevation_parkpos, elpos);
  eeprom_update_chksum();

  int16_t az_park_pos, el_park_pos;
  eeprom_restore_int(rotor_azimuth_parkpos, &az_park_pos);
  eeprom_restore_int(rotor_elevation_parkpos, &el_park_pos);
  return ECMD_FINAL(snprintf_P(output, len, PSTR("park azimuth=%d elevation=%d"), az_park_pos, el_park_pos ));
}

/*
  -- Ethersex META --
  block([[Rotor Interface]]))
  ecmd_feature(rotor_move, "rotor move", [DIRECTION] [SPEED], 'Set Geographic Direction (Angle: 0-359) Speed in % : 1-100')
  ecmd_feature(rotor_status, "rotor status",, Display the current rotor status.)
  ecmd_feature(rotor_state, "rotor state",, Display the rotor state.)
  ecmd_feature(rotor_cw,  "rotor cw" , [SPEED], Turn clockwise (speed in percent))
  ecmd_feature(rotor_ccw, "rotor ccw", [SPEED], Turn counterclockwise (speed in percent))
  ecmd_feature(rotor_stop, "rotor stop",, Stop rotor)
  ecmd_feature(rotor_park, "rotor park",, Park rotor)
  ecmd_feature(rotor_parkpos, "rotor setparkpos", azimuth elevation, Set park position)
  ecmd_feature(rotor_cal_azimuth, "rotor calibrate", [MIN] [MAX], Calibrate azimuth min and max.)
  ecmd_feature(rotor_getcal_azimuth, "rotor get calibrate",, Get calibrate azimuth min and max.)
*/

