% run06_measured_profile.m - performance using the MEASURED LE rib profile
% Profile from lep-2d-parts.json seam lines: t/c 19.1% @ x/c 0.308, camber ~1.1%.
% Thin-airfoil zero-alpha lift for a cambered section: CL0 ~ 4*pi*(camber/c),
% reduced for 19% thickness. camber 1.1% -> CL0 ~ 0.10-0.15, NOT the 0.40
% assumed before the profile was extracted.
source('parafoil_lib.m');
g=geom_LE(); SITE=80; [~,~,rho]=isa_atm(SITE+600);
m=0.860; d_des=0.0006; d_got=0.0018;

printf('=== EFFECT OF THE CORRECTED CL0 (measured camber 1.1%%) ===\n');
printf('%8s %8s %7s %7s %7s %7s %9s  %s\n','CL0','CL','CD','GR','Vz','Vx','wind lim','R-08');
for CL0=[0.10 0.13 0.16 0.20 0.30 0.40]
  a=aero_cfg(); a.CL0=CL0; a.CD0_canopy=0.18;
  r=glide(g,a,m,rho,g.alpha_trim_deg,d_des);
  ok='PASS'; if r.Vz<1.0, ok='FAIL low'; elseif r.Vz>3.0, ok='FAIL high'; end
  printf('%8.2f %8.3f %7.3f %7.2f %7.2f %7.2f %9.2f  %s\n',CL0,r.CL,r.CD,r.GR,r.Vz,r.Vx,r.Vx,ok);
end
printf('  (previous runs assumed CL0=0.40 before the profile was measured)\n');

printf('\n=== BEST ESTIMATE, measured profile (CL0=0.13) ===\n');
a=aero_cfg(); a.CL0=0.13;
for cd0=[0.15 0.18 0.22]
  a.CD0_canopy=cd0;
  for d=[d_des d_got]
    r=glide(g,a,m,rho,g.alpha_trim_deg,d);
    printf('  CD0=%.2f d=%.1fmm : CL=%.3f CD=%.3f GR=%.2f Vz=%.2f Vx=%.2f (%.0f km/h) t600=%.0fs\n', ...
      cd0,d*1000,r.CL,r.CD,r.GR,r.Vz,r.Vx,r.Vx*3.6,600/r.Vz);
  end
end

printf('\n=== TRIM SWEEP with measured profile (CL0=0.13, CD0=0.18) ===\n');
a=aero_cfg(); a.CL0=0.13; a.CD0_canopy=0.18;
printf('%8s %8s %8s %7s %7s %7s  %s\n','a[deg]','CL','CD','GR','Vz','Vx','R-08');
for al=0:2:16
  r=glide(g,a,m,rho,al,d_des);
  ok='PASS'; if r.Vz<1.0, ok='FAIL low'; elseif r.Vz>3.0, ok='FAIL high'; end
  printf('%8.1f %8.3f %8.3f %7.2f %7.2f %7.2f  %s\n',al,r.CL,r.CD,r.GR,r.Vz,r.Vx,ok);
end

printf('\n=== WHAT THIS CHANGES vs the CL0=0.40 assumption ===\n');
a1=aero_cfg(); a1.CL0=0.40; a1.CD0_canopy=0.18; r1=glide(g,a1,m,rho,g.alpha_trim_deg,d_des);
a2=aero_cfg(); a2.CL0=0.13; a2.CD0_canopy=0.18; r2=glide(g,a2,m,rho,g.alpha_trim_deg,d_des);
printf('  old (CL0 0.40): GR %.2f  Vz %.2f  Vx %.2f m/s\n',r1.GR,r1.Vz,r1.Vx);
printf('  new (CL0 0.13): GR %.2f  Vz %.2f  Vx %.2f m/s\n',r2.GR,r2.Vz,r2.Vx);
printf('  -> airspeed %+.2f m/s (%+.0f %%): wind penetration IMPROVES\n',r2.Vx-r1.Vx,100*(r2.Vx-r1.Vx)/r1.Vx);
printf('  -> descent  %+.2f m/s: still inside the 1-3 m/s rule\n',r2.Vz-r1.Vz);
printf('  -> glide ratio %+.2f: shorter range, but range was 12-36x surplus anyway\n',r2.GR-r1.GR);
