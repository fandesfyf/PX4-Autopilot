function F = calcF24(dax,dax_b,day,day_b,daz,daz_b,dt,dvx,dvx_b,dvy,dvy_b,dvz,dvz_b,q0,q1,q2,q3)
%CALCF24
%    F = CALCF24(DAX,DAX_B,DAY,DAY_B,DAZ,DAZ_B,DT,DVX,DVX_B,DVY,DVY_B,DVZ,DVZ_B,Q0,Q1,Q2,Q3)

%    This function was generated by the Symbolic Math Toolbox version 6.2.
%    29-May-2017 00:16:12

t2 = dax_b.*(1.0./2.0);
t3 = daz_b.*(1.0./2.0);
t4 = day_b.*(1.0./2.0);
t8 = day.*(1.0./2.0);
t5 = t4-t8;
t6 = q3.*(1.0./2.0);
t7 = q2.*(1.0./2.0);
t9 = daz.*(1.0./2.0);
t10 = dax.*(1.0./2.0);
t11 = -t2+t10;
t12 = q1.*(1.0./2.0);
t13 = -t3+t9;
t14 = -t4+t8;
t15 = dvx-dvx_b;
t16 = dvy-dvy_b;
t17 = dvz-dvz_b;
t18 = q1.*t17.*2.0;
t19 = q1.*t16.*2.0;
t20 = q0.*t17.*2.0;
t21 = q1.*t15.*2.0;
t22 = q2.*t16.*2.0;
t23 = q3.*t17.*2.0;
t24 = t21+t22+t23;
t25 = q0.*t15.*2.0;
t26 = q2.*t17.*2.0;
t37 = q3.*t16.*2.0;
t27 = t25+t26-t37;
t28 = q0.*q3.*2.0;
t29 = q0.^2;
t30 = q1.^2;
t31 = q2.^2;
t32 = q3.^2;
t33 = q2.*t15.*2.0;
t34 = q3.*t15.*2.0;
t35 = q0.*t16.*2.0;
t36 = -t18+t34+t35;
t38 = q0.*q1.*2.0;
F = reshape([1.0,t11,t14,t13,t27,t36,t19+t20-t33,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,dax.*(-1.0./2.0)+t2,1.0,t3-t9,t14,t24,-t19-t20+t33,t36,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,t5,t13,1.0,t2-t10,t19+t20-q2.*t15.*2.0,t24,-t25-t26+t37,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,daz.*(-1.0./2.0)+t3,t5,t11,1.0,t18-q0.*t16.*2.0-q3.*t15.*2.0,t27,t24,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,dt,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,dt,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,dt,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,t12,q0.*(-1.0./2.0),-t6,t7,0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,t7,t6,q0.*(-1.0./2.0),-t12,0.0,0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,t6,-t7,t12,q0.*(-1.0./2.0),0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,-t29-t30+t31+t32,-t28-q1.*q2.*2.0,q0.*q2.*2.0-q1.*q3.*2.0,0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,t28-q1.*q2.*2.0,-t29+t30-t31+t32,-t38-q2.*q3.*2.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,q0.*q2.*-2.0-q1.*q3.*2.0,t38-q2.*q3.*2.0,-t29+t30+t31-t32,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,1.0],[24, 24]);
