/****************************************************************************
 *
 *   Copyright (C) 2015 Mark Charlebois. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file hello_example.cpp
 * Example for Linux
 *
 * @author Mark Charlebois <charlebm@gmail.com>
 */

#include "muorb_test_example.h"
#include <px4_log.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "uORB/topics/sensor_combined.h"
#include "uORB/topics/pwm_input.h"
#include "uORB.h"
#include "px4_middleware.h"
#include "px4_defines.h"
#include <stdlib.h>
//#include <fstream>
//#include <iostream>


px4::AppState MuorbTestExample::appState;

int MuorbTestExample::main()
{
        int rc;
	appState.setRunning(true);
        rc = PingPongTest();
        //rc = FileReadTest();
        appState.setRunning( false );
        return rc;
}

int MuorbTestExample::DefaultTest()
{
        struct pwm_input_s pwm;
        struct sensor_combined_s sc;
        //sc = new sensor_combined_s;
        memset( &pwm, 0, sizeof(pwm_input_s) );
        memset( &sc, 0, sizeof(sensor_combined_s) );
        PX4_WARN( "Suucessful after memset... " );
        orb_advert_t pub_fd = orb_advertise( ORB_ID( pwm_input ), &pwm );
        if( pub_fd == nullptr )
        {
           PX4_WARN( "Error: advertizing  pwm_input topic" );
           return -1;
        }
        orb_advert_t pub_sc = orb_advertise( ORB_ID( sensor_combined ), &sc );
        if( pub_sc == nullptr )
        {
           PX4_WARN( "Error: advertizing  sensor_combined topic" );
           return -1;
        }
        

	int i=0;
        pwm.error_count++;
        sc.gyro_errcount++;
	//while (!appState.exitRequested() && i<5) {
	while (!appState.exitRequested() && i < 10 ) {

		PX4_INFO("  Doing work...");
                orb_publish( ORB_ID( pwm_input), pub_fd, &pwm );
                orb_publish( ORB_ID( sensor_combined ), pub_sc, &sc );
                //px4::usleep( 1000000 );
                //sleep( 1 );
                for( int64_t j = 0; j < 0x80; ++j ) 
                { 
                   volatile int x = 0;
                   ++x;
                }
        	++i;
	}
        return 0;
}

int MuorbTestExample::PingPongTest()
{
        int i=0;
        orb_advert_t pub_id_esc_status = orb_advertise( ORB_ID( esc_status ), & m_esc_status );
        if( pub_id_esc_status == 0 )
        {
            PX4_ERR( "error publishing esc_status" );
            return -1;
        }
        if( orb_publish( ORB_ID( esc_status ), pub_id_esc_status, &m_esc_status ) == PX4_ERROR )
        {
           PX4_ERR( "[%d]Error publishing the esc_status message", i );
           return -1;
        }
        int sub_vc = orb_subscribe( ORB_ID( vehicle_command ) );
        if ( sub_vc == PX4_ERROR )
        {
           PX4_ERR( "Error subscribing to vehicle_command topic" );
           return -1;
        }

        while (!appState.exitRequested() ) {

                PX4_DEBUG("[%d]  Doing work...", i );
                bool updated = false;
                if( orb_check( sub_vc, &updated ) == 0 )
                {
                    if( updated )
                    {
                       PX4_DEBUG( "[%d]vechile command status is updated... reading new value", i );
                       if( orb_copy( ORB_ID( vehicle_command ), sub_vc, &m_vc ) != 0 )
                       {
                           PX4_ERR( "[%d]Error calling orb copy for vechicle... ", i );
                           break;
                       }
                       if( orb_publish( ORB_ID( esc_status ), pub_id_esc_status, &m_esc_status ) == PX4_ERROR )
                       {
                          PX4_ERR( "[%d]Error publishing the esc_status message", i );
                          break;
                       }
                    }
                    else
                    {
                       PX4_DEBUG( "[%d] vechicle command topic is not updated", i );
                    }
                }
                else
                {
                    PX4_ERR( "[%d]Error checking the updated status for vehicle command ", i );
                    break;
                }
                // sleep for 1 sec.
                usleep( 1000000 ); 
                
                ++i;
        }
        return 0;
}

int MuorbTestExample::FileReadTest()
{
   int rc = OK;
   //static const char TEST_FILE_PATH[] = "/home/linaro/test.txt";
   static const char TEST_FILE_PATH[] = "./test.txt";
   FILE* fp;
   char* line = NULL;
   size_t len = 0;
   ssize_t read;

   fp = fopen( TEST_FILE_PATH, "r" );
   if( fp == NULL )
   {
     PX4_WARN( "unable to open file[%s] for reading", TEST_FILE_PATH );
     rc = PX4_ERROR;
   }
   else
   {
     int i = 0;
     //while( ( read = getline( &line, &len, fp ) ) != -1 )
     //{
     //   ++i;
     //   PX4_WARN( "LineNum[%d] LineLength[%d]", i, len );
     //   PX4_WARN( "LineNum[%d] Line[%s]", i, line );
     //}
     PX4_WARN( "Successfully opened file [%s]", TEST_FILE_PATH );
     fclose( fp );
     if( line != NULL )
     {
        free( line );
     }
   }

/* 
   std::fstream fs( TEST_FILE_PATH, std::fstream::in );
   if( fs.is_open() )
   {
      int i = 0;
      char line[1024];
      while( !fs.eof() )
      {
         ++i;
         fs.getline( line, 1024 );
         PX4_WARN( "ReadLine[%d] Line[%s]", i, line );
      } 
      fs.close();
   }
   else
   {
      PX4_WARN( "Unable to open file[%s] for reading", TEST_FILE_PATH );
      rc = PX4_ERROR;
   }
*/
   return rc;
}
