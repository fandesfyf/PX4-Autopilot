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

#ifndef _uORBCommunicatorMock_hpp_
#define _uORBCommunicatorMock_hpp_

#include "uORB/uORBCommunicator.hpp"
#include "uORBGtestTopics.hpp"
#include <map>
#include <set>

namespace uORB_test
{
   class uORBCommunicatorMock;
}

class uORB_test::uORBCommunicatorMock : public uORBCommunicator::IChannel
{
 public:

  //counters to track how many times the iterface is called from
  // uorb.
  typedef struct
  {
    int64_t _add_subscriptionCount;
    int64_t _remove_subscriptionCount;
    int64_t _send_messageCount;
  }InterfaceCounters;

  uORBCommunicatorMock();

  /**
   * @brief Interface to notify the remote entity of interest of a
   * subscription for a message.
   *
   * @param messageName
   *  This represents the uORB message name; This message name should be
   *  globally unique.
   * @param msgRate
   *  The max rate at which the subscriber can accept the messages.
   * @return
   *  0 = success; This means the messages is successfully sent to the receiver
   *    Note: This does not mean that the receiver as received it.
   *  otherwise = failure.
   */
  virtual int16_t add_subscription( const std::string& messageName, int32_t msgRateInHz );


  /**
   * @brief Interface to notify the remote entity of removal of a subscription
   *
   * @param messageName
   *  This represents the uORB message name; This message name should be
   *  globally unique.
   * @return
   *  0 = success; This means the messages is successfully sent to the receiver
   *    Note: This does not necessarily mean that the receiver as received it.
   *  otherwise = failure.
   */
  virtual int16_t remove_subscription( const std::string& messageName );

  /**
   * Register Message Handler.  This is internal for the IChannel implementer*
   */
  virtual int16_t register_handler( uORBCommunicator::IChannelRxHandler* handler );


  /**
   * @brief Sends the data message over the communication link.
   * @param messageName
   *  This represents the uORB message name; This message name should be
   *  globally unique.
   * @param length
   *  The length of the data buffer to be sent.
   * @param data
   *  The actual data to be sent.
   * @return
   *  0 = success; This means the messages is successfully sent to the receiver
   *    Note: This does not mean that the receiver as received it.
   *  otherwise = failure.
   */
  virtual int16_t send_message( const std::string& messageName, int32_t length, uint8_t* data);

  uORBCommunicator::IChannelRxHandler* get_rx_handler()
  {
    return _rx_handler;
  }

  bool get_remote_topicA_data( struct orb_topic_A* data );
  bool get_remote_topicB_data( struct orb_topic_B* data );

  void reset_counters();

  InterfaceCounters get_interface_counters( const std::string& messageName );


 private:
  uORBCommunicator::IChannelRxHandler* _rx_handler;
  int _sub_topicA_copy_fd;
  int _sub_topicB_copy_fd;

  std::map<std::string, std::string> _topic_translation_map;

  struct orb_topic_A _topicAData;
  struct orb_topic_B _topicBData;

  std::map<std::string, InterfaceCounters> _msgCounters;
};

#endif /* _uORBCommunicatorMock_test_hpp_ */
