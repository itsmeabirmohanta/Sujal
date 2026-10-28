% run02_sizing.m - CD0 sensitivity, area sizing, wind-penetration trade
source('parafoil_lib.m');
a=aero_cfg(); g=geom_LE();
SITE=80; [~,~,rho]=isa_atm(SITE+600);
m=0.860; d=0.0006;

printf('=== CD0_canopy SENSITIVITY (the dominant unknown) ===\n');
printf('as-designed 0.71 m2, m=%.3f kg, alpha=%.2f deg\n',m,g.alpha_trim_deg);
printf('%10s %7s %7s %7s %7s  %s\n','CD0','CD_tot','GR','Vz','Vx','R-08');
for cd0=[0.10 0.12 0.15 0.18 0.22 0.26 0.30]
  a2=a; a2.CD0_canopy=cd0;
  r=glide(g,a2,m,rho,g.alpha_trim_deg,d);
  ok='PASS'; if r.Vz<1.0, ok='FAIL(too slow)'; elseif r.Vz>3.0, ok='FAIL(too fast)'; end
  printf('%10.3f %7.3f %7.2f %7.2f %7.2f  %s\n',cd0,r.CD,r.GR,r.Vz,r.Vx,ok);
end
printf('  CDR implies CD=CL/GR=0.90/2.50=0.360 -> i.e. CD0_canopy ~ 0.23\n');
printf('  LE implies  GR 2.70 -> CD=%.3f -> CD0_canopy ~ %.2f\n', 0.867/2.70, 0.867/2.70-0.1015-0.0088-0.0150);

printf('\n=== AREA SIZING SWEEP (what area centres R-08 and penetrates wind?) ===\n');
printf('m=%.3f kg, CD0=0.18 (mid estimate), alpha=%.2f deg\n',m,g.alpha_trim_deg);
printf('%8s %8s %7s %7s %7s %7s %8s  %s\n','S[m2]','W/S','AR','GR','Vz','Vx','t600[s]','R-08');
a3=a; a3.CD0_canopy=0.18;
for S=[0.25 0.30 0.35 0.40 0.45 0.50 0.60 0.71]
  gg=g; gg.S=S; gg.b=sqrt(S*g.AR_proj); gg.AR_proj=g.AR_proj;
  gg.R_line=0.60*sqrt(S/0.71);
  r=glide(gg,a3,m,rho,g.alpha_trim_deg,d);
  ok='PASS'; if r.Vz<1.0, ok='FAIL low'; elseif r.Vz>3.0, ok='FAIL high'; end
  printf('%8.2f %8.2f %7.2f %7.2f %7.2f %7.2f %8.0f  %s\n',S,m*9.80665/S,gg.AR_proj,r.GR,r.Vz,r.Vx,600/r.Vz,ok);
end

printf('\n=== WIND PENETRATION vs AREA (CD0=0.18) ===\n');
printf('Bihar/UP surface winds in competition season commonly 2-6 m/s with gusts higher.\n');
printf('%8s %8s %12s %14s\n','S[m2]','Vx[m/s]','Vx[km/h]','margin @4 m/s wind');
for S=[0.30 0.40 0.50 0.71]
  gg=g; gg.S=S; gg.R_line=0.60*sqrt(S/0.71);
  r=glide(gg,a3,m,rho,g.alpha_trim_deg,d);
  printf('%8.2f %8.2f %12.0f %14.2f m/s\n',S,r.Vx,r.Vx*3.6,r.Vx-4.0);
end

printf('\n=== R-08 MARGIN SUMMARY, as-designed 0.71 m2 ===\n');
for cd0=[0.12 0.18 0.23]
  a4=a; a4.CD0_canopy=cd0;
  r=glide(g,a4,m,rho,g.alpha_trim_deg,d);
  printf('  CD0=%.2f -> Vz=%.2f m/s : margin to 1.0 m/s floor = %+.2f m/s (%.0f %% of band)\n', ...
     cd0,r.Vz,r.Vz-1.0,100*(r.Vz-1.0)/2.0);
end
