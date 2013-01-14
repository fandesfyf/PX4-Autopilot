#include <systemlib/param/param.h>

// currently tuned for easystar from arkhangar in HIL
//https://github.com/arktools/arkhangar

// 16 is max name length

// gyro low pass filter
PARAM_DEFINE_FLOAT(FWB_P_LP, 300.0f); // roll rate low pass cut freq
PARAM_DEFINE_FLOAT(FWB_Q_LP, 300.0f); // pitch rate low pass cut freq
PARAM_DEFINE_FLOAT(FWB_R_LP, 300.0f); // yaw rate low pass cut freq

// yaw washout
PARAM_DEFINE_FLOAT(FWB_R_HP, 1.0f); // yaw rate high pass

// stabilization mode
PARAM_DEFINE_FLOAT(FWB_P2AIL, 0.5f); // roll rate 2 aileron
PARAM_DEFINE_FLOAT(FWB_Q2ELV, 0.5f); // pitch rate 2 elevator
PARAM_DEFINE_FLOAT(FWB_R2RDR, 0.2f); // yaw rate 2 rudder

//  psi -> phi -> p
PARAM_DEFINE_FLOAT(FWB_PSI2PHI, 0.5f);      // heading 2 roll
PARAM_DEFINE_FLOAT(FWB_PHI2P, 1.0f);        // roll to roll rate
PARAM_DEFINE_FLOAT(FWB_PHI_LIM_MAX, 0.5f);  // roll limit, 28 deg

// velocity -> theta
PARAM_DEFINE_FLOAT(FWB_V2THE_P, 0.2f);
PARAM_DEFINE_FLOAT(FWB_V2THE_I, 0.0f);
PARAM_DEFINE_FLOAT(FWB_V2THE_D, 0.0f);
PARAM_DEFINE_FLOAT(FWB_V2THE_D_LP, 0.0f);
PARAM_DEFINE_FLOAT(FWB_V2THE_I_MAX, 0.0f);
PARAM_DEFINE_FLOAT(FWB_THE_MIN, -0.5f);
PARAM_DEFINE_FLOAT(FWB_THE_MAX, 0.5f);


// theta -> q
PARAM_DEFINE_FLOAT(FWB_THE2Q_P, 1.0f);
PARAM_DEFINE_FLOAT(FWB_THE2Q_I, 0.0f);
PARAM_DEFINE_FLOAT(FWB_THE2Q_D, 0.0f);
PARAM_DEFINE_FLOAT(FWB_THE2Q_D_LP, 0.0f);
PARAM_DEFINE_FLOAT(FWB_THE2Q_I_MAX, 0.0f);

//  h -> thr
PARAM_DEFINE_FLOAT(FWB_H2THR_P, 0.01f);
PARAM_DEFINE_FLOAT(FWB_H2THR_I, 0.0f);
PARAM_DEFINE_FLOAT(FWB_H2THR_D, 0.0f);
PARAM_DEFINE_FLOAT(FWB_H2THR_D_LP, 0.0f);
PARAM_DEFINE_FLOAT(FWB_H2THR_I_MAX, 0.0f);

// crosstrack
PARAM_DEFINE_FLOAT(FWB_XT2YAW_MAX, 1.57f); // 90 deg
PARAM_DEFINE_FLOAT(FWB_XT2YAW, 0.002f);

// speed command
PARAM_DEFINE_FLOAT(FWB_V_MIN, 20.0f);
PARAM_DEFINE_FLOAT(FWB_V_CMD, 22.0f);
PARAM_DEFINE_FLOAT(FWB_V_MAX, 24.0f);

// trim
PARAM_DEFINE_FLOAT(FWB_TRIM_AIL, 0.0f);
PARAM_DEFINE_FLOAT(FWB_TRIM_ELV, 0.005f);
PARAM_DEFINE_FLOAT(FWB_TRIM_RDR, 0.0f);
PARAM_DEFINE_FLOAT(FWB_TRIM_THR, 0.81f);
