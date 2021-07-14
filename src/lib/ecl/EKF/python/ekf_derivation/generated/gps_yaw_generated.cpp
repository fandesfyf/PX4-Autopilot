// Sub Expressions
const float HK0 = cosf(ant_yaw);
const float HK1 = sinf(ant_yaw);
const float HK2 = q0*q3;
const float HK3 = q1*q2;
const float HK4 = 2*HK1;
const float HK5 = 2*powf(q3, 2) - 1;
const float HK6 = HK0*(HK5 + 2*powf(q2, 2)) + HK4*(HK2 - HK3);
const float HK7 = 1.0F/HK6;
const float HK8 = 2*HK0;
const float HK9 = -HK1*(HK5 + 2*powf(q1, 2)) + HK8*(HK2 + HK3);
const float HK10 = HK7*HK9;
const float HK11 = HK1*HK10;
const float HK12 = q3*(-HK0 + HK11);
const float HK13 = powf(HK6, -2);
const float HK14 = HK13*powf(HK9, 2) + 1;
const float HK15 = 2*HK7/HK14;
const float HK16 = HK0*q2;
const float HK17 = HK1*q1;
const float HK18 = HK11*q2 + HK16 - 2*HK17;
const float HK19 = HK0*q1 + HK10*(-2*HK16 + HK17);
const float HK20 = -HK0*q0 + HK10*(HK1*q0 + HK8*q3) + HK4*q3;
const float HK21 = HK12*P(0,0) - HK18*P(0,1) - HK19*P(0,2) + HK20*P(0,3);
const float HK22 = 4*HK13/powf(HK14, 2);
const float HK23 = HK12*P(0,1) - HK18*P(1,1) - HK19*P(1,2) + HK20*P(1,3);
const float HK24 = HK12*P(0,2) - HK18*P(1,2) - HK19*P(2,2) + HK20*P(2,3);
const float HK25 = HK12*P(0,3) - HK18*P(1,3) - HK19*P(2,3) + HK20*P(3,3);
const float HK26 = HK15/(HK12*HK21*HK22 - HK18*HK22*HK23 - HK19*HK22*HK24 + HK20*HK22*HK25 + R_YAW);


// Observation Jacobians
Hfusion.at<0>() = HK12*HK15;
Hfusion.at<1>() = -HK15*HK18;
Hfusion.at<2>() = -HK15*HK19;
Hfusion.at<3>() = HK15*HK20;
Hfusion.at<4>() = 0;
Hfusion.at<5>() = 0;
Hfusion.at<6>() = 0;
Hfusion.at<7>() = 0;
Hfusion.at<8>() = 0;
Hfusion.at<9>() = 0;
Hfusion.at<10>() = 0;
Hfusion.at<11>() = 0;
Hfusion.at<12>() = 0;
Hfusion.at<13>() = 0;
Hfusion.at<14>() = 0;
Hfusion.at<15>() = 0;
Hfusion.at<16>() = 0;
Hfusion.at<17>() = 0;
Hfusion.at<18>() = 0;
Hfusion.at<19>() = 0;
Hfusion.at<20>() = 0;
Hfusion.at<21>() = 0;
Hfusion.at<22>() = 0;
Hfusion.at<23>() = 0;


// Kalman gains
Kfusion(0) = HK21*HK26;
Kfusion(1) = HK23*HK26;
Kfusion(2) = HK24*HK26;
Kfusion(3) = HK25*HK26;
Kfusion(4) = HK26*(HK12*P(0,4) - HK18*P(1,4) - HK19*P(2,4) + HK20*P(3,4));
Kfusion(5) = HK26*(HK12*P(0,5) - HK18*P(1,5) - HK19*P(2,5) + HK20*P(3,5));
Kfusion(6) = HK26*(HK12*P(0,6) - HK18*P(1,6) - HK19*P(2,6) + HK20*P(3,6));
Kfusion(7) = HK26*(HK12*P(0,7) - HK18*P(1,7) - HK19*P(2,7) + HK20*P(3,7));
Kfusion(8) = HK26*(HK12*P(0,8) - HK18*P(1,8) - HK19*P(2,8) + HK20*P(3,8));
Kfusion(9) = HK26*(HK12*P(0,9) - HK18*P(1,9) - HK19*P(2,9) + HK20*P(3,9));
Kfusion(10) = HK26*(HK12*P(0,10) - HK18*P(1,10) - HK19*P(2,10) + HK20*P(3,10));
Kfusion(11) = HK26*(HK12*P(0,11) - HK18*P(1,11) - HK19*P(2,11) + HK20*P(3,11));
Kfusion(12) = HK26*(HK12*P(0,12) - HK18*P(1,12) - HK19*P(2,12) + HK20*P(3,12));
Kfusion(13) = HK26*(HK12*P(0,13) - HK18*P(1,13) - HK19*P(2,13) + HK20*P(3,13));
Kfusion(14) = HK26*(HK12*P(0,14) - HK18*P(1,14) - HK19*P(2,14) + HK20*P(3,14));
Kfusion(15) = HK26*(HK12*P(0,15) - HK18*P(1,15) - HK19*P(2,15) + HK20*P(3,15));
Kfusion(16) = HK26*(HK12*P(0,16) - HK18*P(1,16) - HK19*P(2,16) + HK20*P(3,16));
Kfusion(17) = HK26*(HK12*P(0,17) - HK18*P(1,17) - HK19*P(2,17) + HK20*P(3,17));
Kfusion(18) = HK26*(HK12*P(0,18) - HK18*P(1,18) - HK19*P(2,18) + HK20*P(3,18));
Kfusion(19) = HK26*(HK12*P(0,19) - HK18*P(1,19) - HK19*P(2,19) + HK20*P(3,19));
Kfusion(20) = HK26*(HK12*P(0,20) - HK18*P(1,20) - HK19*P(2,20) + HK20*P(3,20));
Kfusion(21) = HK26*(HK12*P(0,21) - HK18*P(1,21) - HK19*P(2,21) + HK20*P(3,21));
Kfusion(22) = HK26*(HK12*P(0,22) - HK18*P(1,22) - HK19*P(2,22) + HK20*P(3,22));
Kfusion(23) = HK26*(HK12*P(0,23) - HK18*P(1,23) - HK19*P(2,23) + HK20*P(3,23));


