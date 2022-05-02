#pragma once
// MESSAGE CUSTOM_CMD PACKING

#define MAVLINK_MSG_ID_CUSTOM_CMD 10546


typedef struct __mavlink_custom_cmd_t {
 uint64_t timestamp; /*<  Timestamp in milliseconds since system boot*/
 float data1; /*<  t.*/
 float data2; /*<  r.*/
 float data3; /*<  p*/
 float data4; /*<  y*/
 float data5; /*<  d*/
 uint8_t cmd; /*<  cmd types.*/
 char tips[50]; /*<  tips msg*/
} mavlink_custom_cmd_t;

#define MAVLINK_MSG_ID_CUSTOM_CMD_LEN 79
#define MAVLINK_MSG_ID_CUSTOM_CMD_MIN_LEN 79
#define MAVLINK_MSG_ID_10546_LEN 79
#define MAVLINK_MSG_ID_10546_MIN_LEN 79

#define MAVLINK_MSG_ID_CUSTOM_CMD_CRC 163
#define MAVLINK_MSG_ID_10546_CRC 163

#define MAVLINK_MSG_CUSTOM_CMD_FIELD_TIPS_LEN 50

#if MAVLINK_COMMAND_24BIT
#define MAVLINK_MESSAGE_INFO_CUSTOM_CMD { \
    10546, \
    "CUSTOM_CMD", \
    8, \
    {  { "timestamp", NULL, MAVLINK_TYPE_UINT64_T, 0, 0, offsetof(mavlink_custom_cmd_t, timestamp) }, \
         { "cmd", NULL, MAVLINK_TYPE_UINT8_T, 0, 28, offsetof(mavlink_custom_cmd_t, cmd) }, \
         { "data1", NULL, MAVLINK_TYPE_FLOAT, 0, 8, offsetof(mavlink_custom_cmd_t, data1) }, \
         { "data2", NULL, MAVLINK_TYPE_FLOAT, 0, 12, offsetof(mavlink_custom_cmd_t, data2) }, \
         { "data3", NULL, MAVLINK_TYPE_FLOAT, 0, 16, offsetof(mavlink_custom_cmd_t, data3) }, \
         { "data4", NULL, MAVLINK_TYPE_FLOAT, 0, 20, offsetof(mavlink_custom_cmd_t, data4) }, \
         { "data5", NULL, MAVLINK_TYPE_FLOAT, 0, 24, offsetof(mavlink_custom_cmd_t, data5) }, \
         { "tips", NULL, MAVLINK_TYPE_CHAR, 50, 29, offsetof(mavlink_custom_cmd_t, tips) }, \
         } \
}
#else
#define MAVLINK_MESSAGE_INFO_CUSTOM_CMD { \
    "CUSTOM_CMD", \
    8, \
    {  { "timestamp", NULL, MAVLINK_TYPE_UINT64_T, 0, 0, offsetof(mavlink_custom_cmd_t, timestamp) }, \
         { "cmd", NULL, MAVLINK_TYPE_UINT8_T, 0, 28, offsetof(mavlink_custom_cmd_t, cmd) }, \
         { "data1", NULL, MAVLINK_TYPE_FLOAT, 0, 8, offsetof(mavlink_custom_cmd_t, data1) }, \
         { "data2", NULL, MAVLINK_TYPE_FLOAT, 0, 12, offsetof(mavlink_custom_cmd_t, data2) }, \
         { "data3", NULL, MAVLINK_TYPE_FLOAT, 0, 16, offsetof(mavlink_custom_cmd_t, data3) }, \
         { "data4", NULL, MAVLINK_TYPE_FLOAT, 0, 20, offsetof(mavlink_custom_cmd_t, data4) }, \
         { "data5", NULL, MAVLINK_TYPE_FLOAT, 0, 24, offsetof(mavlink_custom_cmd_t, data5) }, \
         { "tips", NULL, MAVLINK_TYPE_CHAR, 50, 29, offsetof(mavlink_custom_cmd_t, tips) }, \
         } \
}
#endif

/**
 * @brief Pack a custom_cmd message
 * @param system_id ID of this system
 * @param component_id ID of this component (e.g. 200 for IMU)
 * @param msg The MAVLink message to compress the data into
 *
 * @param timestamp  Timestamp in milliseconds since system boot
 * @param cmd  cmd types.
 * @param data1  t.
 * @param data2  r.
 * @param data3  p
 * @param data4  y
 * @param data5  d
 * @param tips  tips msg
 * @return length of the message in bytes (excluding serial stream start sign)
 */
static inline uint16_t mavlink_msg_custom_cmd_pack(uint8_t system_id, uint8_t component_id, mavlink_message_t* msg,
                               uint64_t timestamp, uint8_t cmd, float data1, float data2, float data3, float data4, float data5, const char *tips)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    char buf[MAVLINK_MSG_ID_CUSTOM_CMD_LEN];
    _mav_put_uint64_t(buf, 0, timestamp);
    _mav_put_float(buf, 8, data1);
    _mav_put_float(buf, 12, data2);
    _mav_put_float(buf, 16, data3);
    _mav_put_float(buf, 20, data4);
    _mav_put_float(buf, 24, data5);
    _mav_put_uint8_t(buf, 28, cmd);
    _mav_put_char_array(buf, 29, tips, 50);
        memcpy(_MAV_PAYLOAD_NON_CONST(msg), buf, MAVLINK_MSG_ID_CUSTOM_CMD_LEN);
#else
    mavlink_custom_cmd_t packet;
    packet.timestamp = timestamp;
    packet.data1 = data1;
    packet.data2 = data2;
    packet.data3 = data3;
    packet.data4 = data4;
    packet.data5 = data5;
    packet.cmd = cmd;
    mav_array_memcpy(packet.tips, tips, sizeof(char)*50);
        memcpy(_MAV_PAYLOAD_NON_CONST(msg), &packet, MAVLINK_MSG_ID_CUSTOM_CMD_LEN);
#endif

    msg->msgid = MAVLINK_MSG_ID_CUSTOM_CMD;
    return mavlink_finalize_message(msg, system_id, component_id, MAVLINK_MSG_ID_CUSTOM_CMD_MIN_LEN, MAVLINK_MSG_ID_CUSTOM_CMD_LEN, MAVLINK_MSG_ID_CUSTOM_CMD_CRC);
}

/**
 * @brief Pack a custom_cmd message on a channel
 * @param system_id ID of this system
 * @param component_id ID of this component (e.g. 200 for IMU)
 * @param chan The MAVLink channel this message will be sent over
 * @param msg The MAVLink message to compress the data into
 * @param timestamp  Timestamp in milliseconds since system boot
 * @param cmd  cmd types.
 * @param data1  t.
 * @param data2  r.
 * @param data3  p
 * @param data4  y
 * @param data5  d
 * @param tips  tips msg
 * @return length of the message in bytes (excluding serial stream start sign)
 */
static inline uint16_t mavlink_msg_custom_cmd_pack_chan(uint8_t system_id, uint8_t component_id, uint8_t chan,
                               mavlink_message_t* msg,
                                   uint64_t timestamp,uint8_t cmd,float data1,float data2,float data3,float data4,float data5,const char *tips)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    char buf[MAVLINK_MSG_ID_CUSTOM_CMD_LEN];
    _mav_put_uint64_t(buf, 0, timestamp);
    _mav_put_float(buf, 8, data1);
    _mav_put_float(buf, 12, data2);
    _mav_put_float(buf, 16, data3);
    _mav_put_float(buf, 20, data4);
    _mav_put_float(buf, 24, data5);
    _mav_put_uint8_t(buf, 28, cmd);
    _mav_put_char_array(buf, 29, tips, 50);
        memcpy(_MAV_PAYLOAD_NON_CONST(msg), buf, MAVLINK_MSG_ID_CUSTOM_CMD_LEN);
#else
    mavlink_custom_cmd_t packet;
    packet.timestamp = timestamp;
    packet.data1 = data1;
    packet.data2 = data2;
    packet.data3 = data3;
    packet.data4 = data4;
    packet.data5 = data5;
    packet.cmd = cmd;
    mav_array_memcpy(packet.tips, tips, sizeof(char)*50);
        memcpy(_MAV_PAYLOAD_NON_CONST(msg), &packet, MAVLINK_MSG_ID_CUSTOM_CMD_LEN);
#endif

    msg->msgid = MAVLINK_MSG_ID_CUSTOM_CMD;
    return mavlink_finalize_message_chan(msg, system_id, component_id, chan, MAVLINK_MSG_ID_CUSTOM_CMD_MIN_LEN, MAVLINK_MSG_ID_CUSTOM_CMD_LEN, MAVLINK_MSG_ID_CUSTOM_CMD_CRC);
}

/**
 * @brief Encode a custom_cmd struct
 *
 * @param system_id ID of this system
 * @param component_id ID of this component (e.g. 200 for IMU)
 * @param msg The MAVLink message to compress the data into
 * @param custom_cmd C-struct to read the message contents from
 */
static inline uint16_t mavlink_msg_custom_cmd_encode(uint8_t system_id, uint8_t component_id, mavlink_message_t* msg, const mavlink_custom_cmd_t* custom_cmd)
{
    return mavlink_msg_custom_cmd_pack(system_id, component_id, msg, custom_cmd->timestamp, custom_cmd->cmd, custom_cmd->data1, custom_cmd->data2, custom_cmd->data3, custom_cmd->data4, custom_cmd->data5, custom_cmd->tips);
}

/**
 * @brief Encode a custom_cmd struct on a channel
 *
 * @param system_id ID of this system
 * @param component_id ID of this component (e.g. 200 for IMU)
 * @param chan The MAVLink channel this message will be sent over
 * @param msg The MAVLink message to compress the data into
 * @param custom_cmd C-struct to read the message contents from
 */
static inline uint16_t mavlink_msg_custom_cmd_encode_chan(uint8_t system_id, uint8_t component_id, uint8_t chan, mavlink_message_t* msg, const mavlink_custom_cmd_t* custom_cmd)
{
    return mavlink_msg_custom_cmd_pack_chan(system_id, component_id, chan, msg, custom_cmd->timestamp, custom_cmd->cmd, custom_cmd->data1, custom_cmd->data2, custom_cmd->data3, custom_cmd->data4, custom_cmd->data5, custom_cmd->tips);
}

/**
 * @brief Send a custom_cmd message
 * @param chan MAVLink channel to send the message
 *
 * @param timestamp  Timestamp in milliseconds since system boot
 * @param cmd  cmd types.
 * @param data1  t.
 * @param data2  r.
 * @param data3  p
 * @param data4  y
 * @param data5  d
 * @param tips  tips msg
 */
#ifdef MAVLINK_USE_CONVENIENCE_FUNCTIONS

static inline void mavlink_msg_custom_cmd_send(mavlink_channel_t chan, uint64_t timestamp, uint8_t cmd, float data1, float data2, float data3, float data4, float data5, const char *tips)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    char buf[MAVLINK_MSG_ID_CUSTOM_CMD_LEN];
    _mav_put_uint64_t(buf, 0, timestamp);
    _mav_put_float(buf, 8, data1);
    _mav_put_float(buf, 12, data2);
    _mav_put_float(buf, 16, data3);
    _mav_put_float(buf, 20, data4);
    _mav_put_float(buf, 24, data5);
    _mav_put_uint8_t(buf, 28, cmd);
    _mav_put_char_array(buf, 29, tips, 50);
    _mav_finalize_message_chan_send(chan, MAVLINK_MSG_ID_CUSTOM_CMD, buf, MAVLINK_MSG_ID_CUSTOM_CMD_MIN_LEN, MAVLINK_MSG_ID_CUSTOM_CMD_LEN, MAVLINK_MSG_ID_CUSTOM_CMD_CRC);
#else
    mavlink_custom_cmd_t packet;
    packet.timestamp = timestamp;
    packet.data1 = data1;
    packet.data2 = data2;
    packet.data3 = data3;
    packet.data4 = data4;
    packet.data5 = data5;
    packet.cmd = cmd;
    mav_array_memcpy(packet.tips, tips, sizeof(char)*50);
    _mav_finalize_message_chan_send(chan, MAVLINK_MSG_ID_CUSTOM_CMD, (const char *)&packet, MAVLINK_MSG_ID_CUSTOM_CMD_MIN_LEN, MAVLINK_MSG_ID_CUSTOM_CMD_LEN, MAVLINK_MSG_ID_CUSTOM_CMD_CRC);
#endif
}

/**
 * @brief Send a custom_cmd message
 * @param chan MAVLink channel to send the message
 * @param struct The MAVLink struct to serialize
 */
static inline void mavlink_msg_custom_cmd_send_struct(mavlink_channel_t chan, const mavlink_custom_cmd_t* custom_cmd)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    mavlink_msg_custom_cmd_send(chan, custom_cmd->timestamp, custom_cmd->cmd, custom_cmd->data1, custom_cmd->data2, custom_cmd->data3, custom_cmd->data4, custom_cmd->data5, custom_cmd->tips);
#else
    _mav_finalize_message_chan_send(chan, MAVLINK_MSG_ID_CUSTOM_CMD, (const char *)custom_cmd, MAVLINK_MSG_ID_CUSTOM_CMD_MIN_LEN, MAVLINK_MSG_ID_CUSTOM_CMD_LEN, MAVLINK_MSG_ID_CUSTOM_CMD_CRC);
#endif
}

#if MAVLINK_MSG_ID_CUSTOM_CMD_LEN <= MAVLINK_MAX_PAYLOAD_LEN
/*
  This varient of _send() can be used to save stack space by re-using
  memory from the receive buffer.  The caller provides a
  mavlink_message_t which is the size of a full mavlink message. This
  is usually the receive buffer for the channel, and allows a reply to an
  incoming message with minimum stack space usage.
 */
static inline void mavlink_msg_custom_cmd_send_buf(mavlink_message_t *msgbuf, mavlink_channel_t chan,  uint64_t timestamp, uint8_t cmd, float data1, float data2, float data3, float data4, float data5, const char *tips)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    char *buf = (char *)msgbuf;
    _mav_put_uint64_t(buf, 0, timestamp);
    _mav_put_float(buf, 8, data1);
    _mav_put_float(buf, 12, data2);
    _mav_put_float(buf, 16, data3);
    _mav_put_float(buf, 20, data4);
    _mav_put_float(buf, 24, data5);
    _mav_put_uint8_t(buf, 28, cmd);
    _mav_put_char_array(buf, 29, tips, 50);
    _mav_finalize_message_chan_send(chan, MAVLINK_MSG_ID_CUSTOM_CMD, buf, MAVLINK_MSG_ID_CUSTOM_CMD_MIN_LEN, MAVLINK_MSG_ID_CUSTOM_CMD_LEN, MAVLINK_MSG_ID_CUSTOM_CMD_CRC);
#else
    mavlink_custom_cmd_t *packet = (mavlink_custom_cmd_t *)msgbuf;
    packet->timestamp = timestamp;
    packet->data1 = data1;
    packet->data2 = data2;
    packet->data3 = data3;
    packet->data4 = data4;
    packet->data5 = data5;
    packet->cmd = cmd;
    mav_array_memcpy(packet->tips, tips, sizeof(char)*50);
    _mav_finalize_message_chan_send(chan, MAVLINK_MSG_ID_CUSTOM_CMD, (const char *)packet, MAVLINK_MSG_ID_CUSTOM_CMD_MIN_LEN, MAVLINK_MSG_ID_CUSTOM_CMD_LEN, MAVLINK_MSG_ID_CUSTOM_CMD_CRC);
#endif
}
#endif

#endif

// MESSAGE CUSTOM_CMD UNPACKING


/**
 * @brief Get field timestamp from custom_cmd message
 *
 * @return  Timestamp in milliseconds since system boot
 */
static inline uint64_t mavlink_msg_custom_cmd_get_timestamp(const mavlink_message_t* msg)
{
    return _MAV_RETURN_uint64_t(msg,  0);
}

/**
 * @brief Get field cmd from custom_cmd message
 *
 * @return  cmd types.
 */
static inline uint8_t mavlink_msg_custom_cmd_get_cmd(const mavlink_message_t* msg)
{
    return _MAV_RETURN_uint8_t(msg,  28);
}

/**
 * @brief Get field data1 from custom_cmd message
 *
 * @return  t.
 */
static inline float mavlink_msg_custom_cmd_get_data1(const mavlink_message_t* msg)
{
    return _MAV_RETURN_float(msg,  8);
}

/**
 * @brief Get field data2 from custom_cmd message
 *
 * @return  r.
 */
static inline float mavlink_msg_custom_cmd_get_data2(const mavlink_message_t* msg)
{
    return _MAV_RETURN_float(msg,  12);
}

/**
 * @brief Get field data3 from custom_cmd message
 *
 * @return  p
 */
static inline float mavlink_msg_custom_cmd_get_data3(const mavlink_message_t* msg)
{
    return _MAV_RETURN_float(msg,  16);
}

/**
 * @brief Get field data4 from custom_cmd message
 *
 * @return  y
 */
static inline float mavlink_msg_custom_cmd_get_data4(const mavlink_message_t* msg)
{
    return _MAV_RETURN_float(msg,  20);
}

/**
 * @brief Get field data5 from custom_cmd message
 *
 * @return  d
 */
static inline float mavlink_msg_custom_cmd_get_data5(const mavlink_message_t* msg)
{
    return _MAV_RETURN_float(msg,  24);
}

/**
 * @brief Get field tips from custom_cmd message
 *
 * @return  tips msg
 */
static inline uint16_t mavlink_msg_custom_cmd_get_tips(const mavlink_message_t* msg, char *tips)
{
    return _MAV_RETURN_char_array(msg, tips, 50,  29);
}

/**
 * @brief Decode a custom_cmd message into a struct
 *
 * @param msg The message to decode
 * @param custom_cmd C-struct to decode the message contents into
 */
static inline void mavlink_msg_custom_cmd_decode(const mavlink_message_t* msg, mavlink_custom_cmd_t* custom_cmd)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    custom_cmd->timestamp = mavlink_msg_custom_cmd_get_timestamp(msg);
    custom_cmd->data1 = mavlink_msg_custom_cmd_get_data1(msg);
    custom_cmd->data2 = mavlink_msg_custom_cmd_get_data2(msg);
    custom_cmd->data3 = mavlink_msg_custom_cmd_get_data3(msg);
    custom_cmd->data4 = mavlink_msg_custom_cmd_get_data4(msg);
    custom_cmd->data5 = mavlink_msg_custom_cmd_get_data5(msg);
    custom_cmd->cmd = mavlink_msg_custom_cmd_get_cmd(msg);
    mavlink_msg_custom_cmd_get_tips(msg, custom_cmd->tips);
#else
        uint8_t len = msg->len < MAVLINK_MSG_ID_CUSTOM_CMD_LEN? msg->len : MAVLINK_MSG_ID_CUSTOM_CMD_LEN;
        memset(custom_cmd, 0, MAVLINK_MSG_ID_CUSTOM_CMD_LEN);
    memcpy(custom_cmd, _MAV_PAYLOAD(msg), len);
#endif
}
