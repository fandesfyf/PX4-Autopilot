#include <systemlib/param/param.h>

/*PARAM_DEFINE_FLOAT(NAME,0.0f);*/
PARAM_DEFINE_FLOAT(KF_V_GYRO, 0.008f);
PARAM_DEFINE_FLOAT(KF_V_ACCEL, 1.0f);
PARAM_DEFINE_FLOAT(KF_R_MAG, 1.0f);
PARAM_DEFINE_FLOAT(KF_R_GPS_VEL, 1.0f);
PARAM_DEFINE_FLOAT(KF_R_GPS_POS, 5.0f);
PARAM_DEFINE_FLOAT(KF_R_GPS_ALT, 5.0f);
PARAM_DEFINE_FLOAT(KF_R_PRESS_ALT, 0.1f);
PARAM_DEFINE_FLOAT(KF_R_ACCEL, 1.0f);
PARAM_DEFINE_FLOAT(KF_FAULT_POS, 10.0f);
PARAM_DEFINE_FLOAT(KF_FAULT_ATT, 10.0f);
PARAM_DEFINE_FLOAT(KF_ENV_G, 9.765f);
PARAM_DEFINE_FLOAT(KF_ENV_MAG_DIP, 60.0f);
PARAM_DEFINE_FLOAT(KF_ENV_MAG_DEC, 0.0f);
