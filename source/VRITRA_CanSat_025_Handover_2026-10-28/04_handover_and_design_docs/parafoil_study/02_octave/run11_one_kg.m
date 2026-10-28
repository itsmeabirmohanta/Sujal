% run11_one_kg.m - performance at the full 1.0 kg the user quotes
source('parafoil_lib.m');
g=geom_LE(); SITE=80; [~,~,rho6]=isa_atm(SITE+600); [~,~,rho0]=isa_atm(SITE);
d=0.0006; CL0=0.0334; CDp=0.053;
printf('=== PF072 CANOPY, 0.71 m2, AT 600 m AGL ===\n');
printf('air density at 600 m AGL = %.4f kg/m3 (ISA, site 80 m)\n\n',rho6);
printf('%10s %8s %7s %7s %7s %8s %9s  %s\n','mass[kg]','W/S','GR','Vz','Vx','t600[s]','range[m]','R-08');
for m=[0.86 0.90 0.95 1.00 1.05]
  a=aero_cfg(); a.CL0=CL0; a.CD0_canopy=CDp+0.0825;
  r=glide(g,a,m,rho6,g.alpha_trim_deg,d);
  ok='PASS'; if r.Vz<1.0, ok='FAIL slow'; elseif r.Vz>3.0, ok='FAIL fast'; end
  printf('%10.2f %8.2f %7.2f %7.2f %7.2f %8.0f %9.0f  %s\n', ...
    m,m*9.80665/g.S,r.GR,r.Vz,r.Vx,600/r.Vz,600*r.GR,ok);
end
printf('\n=== AT EXACTLY 1.000 kg (full CanSat mass) ===\n');
a=aero_cfg(); a.CL0=CL0; a.CD0_canopy=CDp+0.0825;
r=glide(g,a,1.000,rho6,g.alpha_trim_deg,d);
printf('  wing loading      %.2f N/m2 (%.2f kg/m2)\n',r.WS,1.0/g.S);
printf('  lift coefficient  %.3f\n',r.CL);
printf('  drag coefficient  %.3f\n',r.CD);
printf('  glide ratio       %.2f\n',r.GR);
printf('  airspeed          %.2f m/s\n',r.V);
printf('  DESCENT RATE      %.2f m/s   <- rule requires 1 to 3 m/s\n',r.Vz);
printf('  forward speed     %.2f m/s (%.0f km/h)\n',r.Vx,r.Vx*3.6);
printf('  time 600 m -> 0   %.0f s (%.1f min)\n',600/r.Vz,600/r.Vz/60);
printf('  still-air range   %.0f m\n',600*r.GR);
printf('  touchdown speed   %.2f m/s vertical, %.2f m/s forward\n',r.Vz,r.Vx);
rg=glide(g,a,1.000,rho0,g.alpha_trim_deg,d);
printf('  at ground density: Vz %.2f  Vx %.2f m/s\n',rg.Vz,rg.Vx);
printf('\n=== total aerodynamic force the canopy must carry ===\n');
printf('  steady flight  %.2f N  (= 1.0 kg)\n',9.80665);
for Vd=[14.4 18 22]
  q=0.5*rho6*Vd^2; F=q*g.S_proj*1.5;
  printf('  opening at %4.1f m/s: %.0f N = %.1f g\n',Vd,F,F/9.80665);
end
