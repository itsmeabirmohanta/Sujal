% run09_cfd_calibrated.m - rebuild performance on CFD-derived section data.
%
% From the 2D RANS of the actual LE rib-1 section (Re 1.68e5, k-omega SST,
% O-grid, y+~1):   CL0 = 0.0334 ,  dCL/dalpha = 6.26 /rad (= 0.109 /deg)
%                  CD_profile = 0.0290 (a=0) ... ~0.053 (a=8.6 deg)
% The CFD 2D slope reduced by Helmbold for AR 2.62 gives 3.11 /rad, which is
% what the analytical model already used (3.108) - independent confirmation.
%
% Canopy CD0 = 2D profile drag + ram-air inlet + fabric/rigging roughness.
% CFD gives only the FIRST term; the inlet increment is swept explicitly.
source('parafoil_lib.m');
g=geom_LE(); SITE=80; [~,~,rho]=isa_atm(SITE+600);
m=0.860; d_des=0.0006; d_got=0.0018;
CL0_CFD=0.0334; CDp_CFD=0.053;      % at the 8.61 deg design incidence

printf('=== CFD-CALIBRATED SECTION DATA ===\n');
printf('  CL0 = %.4f (CFD)   was 0.13 assumed, 0.40 in the first pass\n',CL0_CFD);
printf('  2D profile CD at trim = %.4f (CFD)\n',CDp_CFD);
printf('  2D lift slope 6.26/rad -> Helmbold 3D (AR %.2f) = %.3f/rad\n', ...
        g.AR_proj,(2*pi*g.AR_proj)/(2+sqrt(g.AR_proj^2+4)));

printf('\n=== PERFORMANCE vs INLET/ROUGHNESS DRAG INCREMENT ===\n');
printf('%12s %8s %7s %7s %7s %7s %9s  %s\n', ...
       'dCD_inlet','CD0tot','CL','CD','GR','Vz','Vx','R-08');
for dci=[0.03 0.05 0.08 0.10 0.13 0.16]
  a=aero_cfg(); a.CL0=CL0_CFD; a.CD0_canopy=CDp_CFD+dci;
  r=glide(g,a,m,rho,g.alpha_trim_deg,d_des);
  ok='PASS'; if r.Vz<1.0, ok='FAIL low'; elseif r.Vz>3.0, ok='FAIL high'; end
  printf('%12.3f %8.3f %7.3f %7.3f %7.2f %7.2f %9.2f  %s\n', ...
         dci,a.CD0_canopy,r.CL,r.CD,r.GR,r.Vz,r.Vx,ok);
end

printf('\n=== WHICH INCREMENT REPRODUCES THE PUBLISHED GLIDE RATIOS? ===\n');
for tgt=[2.70 2.50]
  best=1e9; bi=0;
  for dci=0.0:0.005:0.30
    a=aero_cfg(); a.CL0=CL0_CFD; a.CD0_canopy=CDp_CFD+dci;
    r=glide(g,a,m,rho,g.alpha_trim_deg,d_des);
    if abs(r.GR-tgt)<best, best=abs(r.GR-tgt); bi=dci; end
  end
  a=aero_cfg(); a.CL0=CL0_CFD; a.CD0_canopy=CDp_CFD+bi;
  r=glide(g,a,m,rho,g.alpha_trim_deg,d_des);
  printf('  GR %.2f needs dCD_inlet = %.3f (CD0tot %.3f) -> Vz %.2f  Vx %.2f\n', ...
         tgt,bi,a.CD0_canopy,r.Vz,r.Vx);
end

printf('\n=== BEST ESTIMATE BAND (dCD_inlet 0.05-0.13, Lingard range) ===\n');
for dci=[0.05 0.13]
  for d=[d_des d_got]
    a=aero_cfg(); a.CL0=CL0_CFD; a.CD0_canopy=CDp_CFD+dci;
    r=glide(g,a,m,rho,g.alpha_trim_deg,d);
    printf('  dCD=%.2f d=%.1fmm : GR %.2f  Vz %.2f  Vx %.2f (%.0f km/h)  t600 %.0fs\n', ...
           dci,d*1000,r.GR,r.Vz,r.Vx,r.Vx*3.6,600/r.Vz);
  end
end

printf('\n=== TRIM SWEEP, CFD-calibrated (dCD_inlet=0.09) ===\n');
a=aero_cfg(); a.CL0=CL0_CFD; a.CD0_canopy=CDp_CFD+0.09;
printf('%8s %8s %8s %7s %7s %7s  %s\n','a[deg]','CL','CD','GR','Vz','Vx','R-08');
for al=2:2:16
  r=glide(g,a,m,rho,al,d_des);
  ok='PASS'; if r.Vz<1.0, ok='FAIL low'; elseif r.Vz>3.0, ok='FAIL high'; end
  printf('%8.1f %8.3f %8.3f %7.2f %7.2f %7.2f  %s\n',al,r.CL,r.CD,r.GR,r.Vz,r.Vx,ok);
end
