//=============================================================================
// File: uORB_test.cpp
//
//  @@-COPYRIGHT-START-@@
//
// Copyright 2014 Qualcomm Technologies, Inc. All rights reserved.
// Confidential & Proprietary - Qualcomm Technologies, Inc. ("QTI")
//
// The party receiving this software directly from QTI (the "Recipient")
// may use this software as reasonably necessary solely for the purposes
// set forth in the agreement between the Recipient and QTI (the
// "Agreement"). The software may be used in source code form solely by
// the Recipient's employees (if any) authorized by the Agreement. Unless
// expressly authorized in the Agreement, the Recipient may not sublicense,
// assign, transfer or otherwise provide the source code to any third
// party. Qualcomm Technologies, Inc. retains all ownership rights in and
// to the software
//
// This notice supersedes any other QTI notices contained within the software
// except copyright notices indicating different years of publication for
// different portions of the software. This notice does not supersede the
// application of any third party copyright notice to that third party's
// code.
//
//  @@-COPYRIGHT-END-@@
//
//=============================================================================

#include "uORBCommunicatorMockLoopback.hpp"
#include "uORB/uORB.h"
#include "uORBGtestTopics.hpp"
#include "uORBManager.hpp"
#include <string.h>
#include "px4_log.h"

#define LOG_TAG "uORBCommunicatorMockLoopback.cpp"


uORB_test::uORBCommunicatorMockLoopback::uORBCommunicatorMockLoopback()
: _rx_handler( nullptr )
{
  //_sub_topicA_clone_fd = orb_subscribe( ORB_ID( topicA_clone ), (void*)&_sub_semaphore );
  //_sub_topicB_clone_fd = orb_subscribe( ORB_ID( topicB_clone ), nullptr );

  _topic_translation_map[ "topicA" ] = "topicA_clone";
  _topic_translation_map[ "topicB" ] = "topicB_clone";
  _topic_translation_map[ "topicA_clone" ] = "topicA";
  _topic_translation_map[ "topicB_clone" ] = "topicB";
}

int16_t uORB_test::uORBCommunicatorMockLoopback::add_subscription
(
    const std::string& messageName,
    int32_t msgRateInHz
)
{

  int16_t rc = -1;
  PX4_INFO( "got add_subscription for msg[%s] rate[%d]", messageName.c_str(), msgRateInHz );

  if( _rx_handler )
  {
    if( _topic_translation_map.find( messageName ) != _topic_translation_map.end() )
    {
       rc = _rx_handler->process_add_subscription
           (
              _topic_translation_map[messageName],
              msgRateInHz
           );
    }
  }
  return rc;
}

int16_t uORB_test::uORBCommunicatorMockLoopback::remove_subscription
(
    const std::string& messageName
)
{
  int16_t rc = -1;
  PX4_INFO( "got remove_subscription for msg[%s]", messageName.c_str() );
  if( _rx_handler )
  {
    if( _topic_translation_map.find( messageName ) != _topic_translation_map.end() )
    {
       rc = _rx_handler->process_remove_subscription
           (
              _topic_translation_map[messageName]
           );
    }
  }
  return rc;
}

int16_t uORB_test::uORBCommunicatorMockLoopback::register_handler
(
    uORBCommunicator::IChannelRxHandler* handler
)
{
  int16_t rc = 0;
  _rx_handler = handler;
  return rc;
}


int16_t uORB_test::uORBCommunicatorMockLoopback::send_message
(
    const std::string& messageName,
    int32_t length,
    uint8_t* data
)
{
  int16_t rc = -1;
  PX4_INFO( "send_message for msg[%s] datalen[%d]", messageName.c_str(), length );
  if( _rx_handler )
  {
    if( _topic_translation_map.find( messageName ) != _topic_translation_map.end() )
    {
      if( uORB::Manager::get_instance()->is_remote_subscriber_present( _topic_translation_map[messageName] ) )
      {
        rc = _rx_handler->process_received_message
            ( _topic_translation_map[messageName], length, data );
        PX4_INFO( "[uORBCommunicatorMockLoopback::send_message] return from[topic(%s)] _rx_handler->process_received_message[%d]", messageName.c_str(), rc );
      }
      else
      {
        // this is eqvuilanet of not sending the message to the remote.
        rc = 0;
      }
    }
  }
  return rc;
}
