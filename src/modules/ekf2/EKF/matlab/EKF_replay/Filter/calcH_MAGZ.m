function H_MAGZ = calcH_MAGZ(magD,magE,magN,q0,q1,q2,q3)
%CALCH_MAGZ
%    H_MAGZ = CALCH_MAGZ(MAGD,MAGE,MAGN,Q0,Q1,Q2,Q3)

%    This function was generated by the Symbolic Math Toolbox version 6.2.
%    29-May-2017 00:16:13

H_MAGZ = [magD.*q0.*2.0-magE.*q1.*2.0+magN.*q2.*2.0,magD.*q1.*-2.0-magE.*q0.*2.0+magN.*q3.*2.0,magD.*q2.*-2.0+magE.*q3.*2.0+magN.*q0.*2.0,magD.*q3.*2.0+magE.*q2.*2.0+magN.*q1.*2.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,q0.*q2.*2.0+q1.*q3.*2.0,q0.*q1.*-2.0+q2.*q3.*2.0,q0.^2-q1.^2-q2.^2+q3.^2,0.0,0.0,1.0,0.0,0.0];
