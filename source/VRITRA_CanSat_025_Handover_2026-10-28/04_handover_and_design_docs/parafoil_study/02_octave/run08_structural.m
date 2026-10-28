% run08_structural.m - line, riser, seam and fabric loads. Replaces the LE output,
% whose section 17 says "Don't try to solve equilibrium equacions" (loads unsolved).
source('parafoil_lib.m');
g=geom_LE(); SITE=80; [~,~,rho6]=isa_atm(SITE+600);
m=0.860; W=m*9.80665; g0=9.80665;
% LE line inventory (lines.txt), per half-wing; mirrored for the full canopy
n_riser=3; n_main=6; n_upper=9; n_brake=6;
R_riser=100*10; R_main=40*10; R_upper=20*10; R_brkmid=30*10; R_brkup=20*10;  % daN -> N
printf('=== 1. STEADY FLIGHT LOADS (m=%.3f kg, W=%.2f N) ===\n',m,W);
printf('  %-10s %5s %10s %10s %10s\n','type','n x2','load each','rating','margin');
for k=1:3
  nm={'riser','main','upper'}{k}; nn=[n_riser n_main n_upper](k)*2; rt=[R_riser R_main R_upper](k);
  L=W/nn;
  printf('  %-10s %5d %9.2f N %9.0f N %9.0fx\n',nm,nn,L,rt,rt/L);
end

printf('\n=== 2. OPENING SHOCK vs the CDR 11.1 G structural design point ===\n');
printf('CDR states the load path is "sized to handle the recovery system''s peak\n');
printf('opening shock (up to 11.1 G)". Primary descent rate is stated three ways.\n\n');
printf('  %-8s %8s %7s %9s %9s %9s  %s\n','Vdep','q[Pa]','Cx','F[N]','g-load','vs 11.1G','verdict');
for Vd=[14.4 15.0 18.0 20.0 22.0]
  q=0.5*rho6*Vd^2;
  for Cx=[1.2 1.5 1.8]
    F=q*g.S_proj*Cx; gl=F/W;
    v='OK'; if gl>11.1, v='EXCEEDS'; end
    printf('  %-8.1f %8.0f %7.1f %9.1f %9.1f %9.2f  %s\n',Vd,q,Cx,F,gl,gl/11.1,v);
  end
end

printf('\n=== 3. LINE STRENGTH AT PEAK OPENING LOAD ===\n');
Vd=18.0; Cx=1.5; q=0.5*rho6*Vd^2; Fpk=q*g.S_proj*Cx;
printf('  peak load %.0f N (%.1f g) at Vdep=%.0f m/s, Cx=%.1f\n',Fpk,Fpk/W,Vd,Cx);
printf('  %-10s %5s %10s %10s %10s\n','type','n x2','load each','rating','margin');
for k=1:3
  nm={'riser','main','upper'}{k}; nn=[n_riser n_main n_upper](k)*2; rt=[R_riser R_main R_upper](k);
  L=Fpk/nn;
  printf('  %-10s %5d %9.1f N %9.0f N %9.1fx\n',nm,nn,L,rt,rt/L);
end
printf('  bought cord: 1.8 mm UHMWPE ~300 daN = 3000 N -> margin %.0fx on a riser\n',3000/(Fpk/(n_riser*2)));

printf('\n=== 4. FABRIC AND SEAM STRESS (cell treated as pressurised cylinder) ===\n');
wcell=0.142; rcell=wcell/2;
printf('  cell width %.3f m -> radius %.3f m\n',wcell,rcell);
printf('  %-10s %9s %14s %16s %10s\n','Vdep','p[Pa]','tension[N/m]','per 50mm [N]','margin');
for Vd=[6.0 14.4 18.0 22.0]
  p=0.5*rho6*Vd^2;
  Tl=p*rcell; per50=Tl*0.05;
  % ripstop nylon 40D: ~200-400 N per 50 mm strip (take the low end, 200 N)
  printf('  %-10.1f %9.0f %14.2f %16.2f %9.0fx\n',Vd,p,Tl,per50,200/max(per50,1e-9));
end
printf('  (ripstop 40D taken at 200 N per 50 mm strip, LOW end; seams ~60%% of that)\n');

printf('\n=== 5. CONCLUSIONS ===\n');
printf('  Lines and fabric are massively overstrength - by 2-3 orders of magnitude.\n');
printf('  The 1.8 mm cord is ~15x stronger than the design 1.2 mm riser already is.\n');
printf('  The binding structural case is NOT the canopy: it is the CanSat body and the\n');
printf('  riser attachment at %.0f-%.0f N (%.0f-%.0f g), against a CDR design point of 11.1 G.\n', ...
   0.5*rho6*14.4^2*g.S_proj*1.2, 0.5*rho6*22^2*g.S_proj*1.8, ...
   0.5*rho6*14.4^2*g.S_proj*1.2/W, 0.5*rho6*22^2*g.S_proj*1.8/W);
printf('  Fabric STRENGTH is not the risk; fabric PERMEABILITY (unspecified) governs\n');
printf('  whether the canopy inflates at all, and cannot be assessed without a sample.\n');
