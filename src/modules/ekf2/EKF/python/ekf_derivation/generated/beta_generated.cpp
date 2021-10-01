// Sub Expressions
const float HK0 = q1*vd;
const float HK1 = vn - vwn;
const float HK2 = HK1*q3;
const float HK3 = q2*vd;
const float HK4 = ve - vwe;
const float HK5 = HK4*q3;
const float HK6 = q0*q2 - q1*q3;
const float HK7 = 2*vd;
const float HK8 = HK6*HK7;
const float HK9 = q0*q3;
const float HK10 = q1*q2;
const float HK11 = 2*HK10 + 2*HK9;
const float HK12 = HK11*HK4;
const float HK13 = 2*powf(q3, 2) - 1;
const float HK14 = HK13 + 2*powf(q2, 2);
const float HK15 = HK1*HK14;
const float HK16 = 1.0F/(-HK12 + HK15 + HK8);
const float HK17 = q0*q1 + q2*q3;
const float HK18 = HK17*HK7;
const float HK19 = 2*HK1*(-HK10 + HK9);
const float HK20 = HK13 + 2*powf(q1, 2);
const float HK21 = HK20*HK4;
const float HK22 = HK16*(-HK18 + HK19 + HK21);
const float HK23 = 2*HK16;
const float HK24 = HK23*(HK0 - HK2 + HK22*(HK3 - HK5));
const float HK25 = q0*vd;
const float HK26 = HK1*q2;
const float HK27 = HK4*q1;
const float HK28 = 2*HK27;
const float HK29 = q3*vd;
const float HK30 = 1.0F/(HK12 - HK15 - HK8);
const float HK31 = HK30*(HK18 - HK19 - HK21);
const float HK32 = HK31*(HK29 + HK4*q2);
const float HK33 = 2*HK30;
const float HK34 = HK23*(HK1*q1 + HK22*(HK25 + 2*HK26 - HK27) + HK29);
const float HK35 = HK33*(HK1*q0 - HK3 + HK31*(HK0 - 2*HK2 + HK4*q0) + 2*HK5);
const float HK36 = HK14*HK31;
const float HK37 = 2*HK9;
const float HK38 = 2*HK10;
const float HK39 = -HK37 + HK38;
const float HK40 = HK30*(HK11*HK31 + HK20);
const float HK41 = HK23*(HK17 + HK22*HK6);
const float HK42 = HK16*(HK14*HK22 + HK39);
const float HK43 = HK16*(HK11*HK22 + HK20);
const float HK44 = HK30*(-HK36 + HK37 - HK38);
const float HK45 = HK33*(-HK25 - HK26 + HK28 + HK32);
const float HK46 = -HK24*P(0,0) - HK34*P(0,2) - HK35*P(0,3) - HK40*P(0,5) - HK41*P(0,6) + HK42*P(0,22) - HK43*P(0,23) - HK44*P(0,4) - HK45*P(0,1);
const float HK47 = -HK24*P(0,6) - HK34*P(2,6) - HK35*P(3,6) - HK40*P(5,6) - HK41*P(6,6) + HK42*P(6,22) - HK43*P(6,23) - HK44*P(4,6) - HK45*P(1,6);
const float HK48 = -HK24*P(0,22) - HK34*P(2,22) - HK35*P(3,22) - HK40*P(5,22) - HK41*P(6,22) + HK42*P(22,22) - HK43*P(22,23) - HK44*P(4,22) - HK45*P(1,22);
const float HK49 = -HK24*P(0,23) - HK34*P(2,23) - HK35*P(3,23) - HK40*P(5,23) - HK41*P(6,23) + HK42*P(22,23) - HK43*P(23,23) - HK44*P(4,23) - HK45*P(1,23);
const float HK50 = -HK24*P(0,5) - HK34*P(2,5) - HK35*P(3,5) - HK40*P(5,5) - HK41*P(5,6) + HK42*P(5,22) - HK43*P(5,23) - HK44*P(4,5) - HK45*P(1,5);
const float HK51 = -HK24*P(0,4) - HK34*P(2,4) - HK35*P(3,4) - HK40*P(4,5) - HK41*P(4,6) + HK42*P(4,22) - HK43*P(4,23) - HK44*P(4,4) - HK45*P(1,4);
const float HK52 = -HK24*P(0,2) - HK34*P(2,2) - HK35*P(2,3) - HK40*P(2,5) - HK41*P(2,6) + HK42*P(2,22) - HK43*P(2,23) - HK44*P(2,4) - HK45*P(1,2);
const float HK53 = -HK24*P(0,1) - HK34*P(1,2) - HK35*P(1,3) - HK40*P(1,5) - HK41*P(1,6) + HK42*P(1,22) - HK43*P(1,23) - HK44*P(1,4) - HK45*P(1,1);
const float HK54 = -HK24*P(0,3) - HK34*P(2,3) - HK35*P(3,3) - HK40*P(3,5) - HK41*P(3,6) + HK42*P(3,22) - HK43*P(3,23) - HK44*P(3,4) - HK45*P(1,3);
const float HK55 = 1.0F/(-HK24*HK46 - HK34*HK52 - HK35*HK54 - HK40*HK50 - HK41*HK47 + HK42*HK48 - HK43*HK49 - HK44*HK51 - HK45*HK53 + R_BETA);


// Observation Jacobians
Hfusion.at<0>() = -HK24;
Hfusion.at<1>() = HK33*(HK25 + HK26 - HK28 - HK32);
Hfusion.at<2>() = -HK34;
Hfusion.at<3>() = -HK35;
Hfusion.at<4>() = HK30*(HK36 + HK39);
Hfusion.at<5>() = -HK40;
Hfusion.at<6>() = -HK41;
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
Hfusion.at<22>() = HK42;
Hfusion.at<23>() = -HK43;


// Kalman gains
Kfusion(0) = HK46*HK55;
Kfusion(1) = HK53*HK55;
Kfusion(2) = HK52*HK55;
Kfusion(3) = HK54*HK55;
Kfusion(4) = HK51*HK55;
Kfusion(5) = HK50*HK55;
Kfusion(6) = HK47*HK55;
Kfusion(7) = HK55*(-HK24*P(0,7) - HK34*P(2,7) - HK35*P(3,7) - HK40*P(5,7) - HK41*P(6,7) + HK42*P(7,22) - HK43*P(7,23) - HK44*P(4,7) - HK45*P(1,7));
Kfusion(8) = HK55*(-HK24*P(0,8) - HK34*P(2,8) - HK35*P(3,8) - HK40*P(5,8) - HK41*P(6,8) + HK42*P(8,22) - HK43*P(8,23) - HK44*P(4,8) - HK45*P(1,8));
Kfusion(9) = HK55*(-HK24*P(0,9) - HK34*P(2,9) - HK35*P(3,9) - HK40*P(5,9) - HK41*P(6,9) + HK42*P(9,22) - HK43*P(9,23) - HK44*P(4,9) - HK45*P(1,9));
Kfusion(10) = HK55*(-HK24*P(0,10) - HK34*P(2,10) - HK35*P(3,10) - HK40*P(5,10) - HK41*P(6,10) + HK42*P(10,22) - HK43*P(10,23) - HK44*P(4,10) - HK45*P(1,10));
Kfusion(11) = HK55*(-HK24*P(0,11) - HK34*P(2,11) - HK35*P(3,11) - HK40*P(5,11) - HK41*P(6,11) + HK42*P(11,22) - HK43*P(11,23) - HK44*P(4,11) - HK45*P(1,11));
Kfusion(12) = HK55*(-HK24*P(0,12) - HK34*P(2,12) - HK35*P(3,12) - HK40*P(5,12) - HK41*P(6,12) + HK42*P(12,22) - HK43*P(12,23) - HK44*P(4,12) - HK45*P(1,12));
Kfusion(13) = HK55*(-HK24*P(0,13) - HK34*P(2,13) - HK35*P(3,13) - HK40*P(5,13) - HK41*P(6,13) + HK42*P(13,22) - HK43*P(13,23) - HK44*P(4,13) - HK45*P(1,13));
Kfusion(14) = HK55*(-HK24*P(0,14) - HK34*P(2,14) - HK35*P(3,14) - HK40*P(5,14) - HK41*P(6,14) + HK42*P(14,22) - HK43*P(14,23) - HK44*P(4,14) - HK45*P(1,14));
Kfusion(15) = HK55*(-HK24*P(0,15) - HK34*P(2,15) - HK35*P(3,15) - HK40*P(5,15) - HK41*P(6,15) + HK42*P(15,22) - HK43*P(15,23) - HK44*P(4,15) - HK45*P(1,15));
Kfusion(16) = HK55*(-HK24*P(0,16) - HK34*P(2,16) - HK35*P(3,16) - HK40*P(5,16) - HK41*P(6,16) + HK42*P(16,22) - HK43*P(16,23) - HK44*P(4,16) - HK45*P(1,16));
Kfusion(17) = HK55*(-HK24*P(0,17) - HK34*P(2,17) - HK35*P(3,17) - HK40*P(5,17) - HK41*P(6,17) + HK42*P(17,22) - HK43*P(17,23) - HK44*P(4,17) - HK45*P(1,17));
Kfusion(18) = HK55*(-HK24*P(0,18) - HK34*P(2,18) - HK35*P(3,18) - HK40*P(5,18) - HK41*P(6,18) + HK42*P(18,22) - HK43*P(18,23) - HK44*P(4,18) - HK45*P(1,18));
Kfusion(19) = HK55*(-HK24*P(0,19) - HK34*P(2,19) - HK35*P(3,19) - HK40*P(5,19) - HK41*P(6,19) + HK42*P(19,22) - HK43*P(19,23) - HK44*P(4,19) - HK45*P(1,19));
Kfusion(20) = HK55*(-HK24*P(0,20) - HK34*P(2,20) - HK35*P(3,20) - HK40*P(5,20) - HK41*P(6,20) + HK42*P(20,22) - HK43*P(20,23) - HK44*P(4,20) - HK45*P(1,20));
Kfusion(21) = HK55*(-HK24*P(0,21) - HK34*P(2,21) - HK35*P(3,21) - HK40*P(5,21) - HK41*P(6,21) + HK42*P(21,22) - HK43*P(21,23) - HK44*P(4,21) - HK45*P(1,21));
Kfusion(22) = HK48*HK55;
Kfusion(23) = HK49*HK55;
