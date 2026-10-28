% run01_performance.m - steady-glide performance, drag build-up, R-08 compliance
source('parafoil_lib.m');
a = aero_cfg();
SITE_ELEV = 80;            % m AMSL, Gangetic plain - ASSUMPTION, to verify
m_sus     = 0.920 - 0.060; % CDR total 920 g minus primary chute+bay assy (ASSUMED 60 g)
d_design  = 0.0006;        % design mean line dia (0.4-0.6 mm mix)
d_bought  = 0.0018;        % IMAGE throw line 1.8 mm

[T6,p6,rho6] = isa_atm(SITE_ELEV+600);
[T0,p0,rho0] = isa_atm(SITE_ELEV);
printf('=== ATMOSPHERE (ISA, site %d m AMSL) ===\n',SITE_ELEV);
printf('  at 600 m AGL: T=%.1f K  rho=%.4f kg/m3\n',T6,rho6);
printf('  at ground   : T=%.1f K  rho=%.4f kg/m3\n\n',T0,rho0);

printf('=== R-08 COMPLIANCE: descent rate must be 1-3 m/s ===\n');
printf('%-34s %6s %6s %6s %6s %6s %7s %7s\n','config','m[kg]','CL','CD','GR','V','Vz','Vx');
for G = {geom_LE(), geom_CDR()}
  g = G{1};
  for d = [d_design d_bought]
    r = glide(g,a,m_sus,rho6,g.alpha_trim_deg,d);
    tag = sprintf('%s d=%.1fmm', substr(g.name,1,20), d*1000);
    printf('%-34s %6.3f %6.3f %6.3f %6.2f %6.2f %7.2f %7.2f\n', ...
           tag,m_sus,r.CL,r.CD,r.GR,r.V,r.Vz,r.Vx);
  end
end

g = geom_LE();
printf('\n=== DRAG BUILD-UP, as-designed canopy at trim (alpha=%.2f deg) ===\n',g.alpha_trim_deg);
for d = [d_design d_bought]
  r = glide(g,a,m_sus,rho6,g.alpha_trim_deg,d);
  printf('\n  line dia %.1f mm:\n',d*1000);
  printf('    CD0 canopy  %.4f  (%4.1f %%)\n', r.CD0, 100*r.CD0/r.CD);
  printf('    CD induced  %.4f  (%4.1f %%)\n', r.CDi, 100*r.CDi/r.CD);
  printf('    CD lines    %.4f  (%4.1f %%)\n', r.CDl, 100*r.CDl/r.CD);
  printf('    CD payload  %.4f  (%4.1f %%)\n', r.CDb, 100*r.CDb/r.CD);
  printf('    CD TOTAL    %.4f   -> GR %.2f, Vz %.2f m/s, Vx %.2f m/s\n',r.CD,r.GR,r.Vz,r.Vx);
end
rd = glide(g,a,m_sus,rho6,g.alpha_trim_deg,d_design);
rb = glide(g,a,m_sus,rho6,g.alpha_trim_deg,d_bought);
printf('\n  >> Cost of using 1.8 mm cord throughout: GR %.2f -> %.2f (%.1f %%), ', ...
        rd.GR,rb.GR,100*(rb.GR-rd.GR)/rd.GR);
printf('range from 600 m %.0f -> %.0f m\n', 600*rd.GR, 600*rb.GR);
printf('  >> LE claims GR %.2f; build-up gives %.2f (design lines) / %.2f (bought cord)\n', ...
        g.GR_LE, rd.GR, rb.GR);

printf('\n=== MASS SENSITIVITY (as-designed canopy, design lines) ===\n');
printf('%8s %7s %7s %7s %7s  %s\n','m[kg]','W/S','Vz','Vx','t600[s]','R-08');
for mm = 0.70:0.10:1.40
  r = glide(g,a,mm,rho6,g.alpha_trim_deg,d_design);
  ok = 'PASS'; if r.Vz<1.0 || r.Vz>3.0, ok='FAIL'; end
  printf('%8.2f %7.2f %7.2f %7.2f %7.0f  %s\n',mm,r.WS,r.Vz,r.Vx,600/r.Vz,ok);
end

printf('\n=== ALPHA SWEEP (trim search, m=%.3f kg, design lines) ===\n',m_sus);
printf('%8s %7s %7s %7s %7s %7s\n','a[deg]','CL','CD','GR','Vz','Vx');
best=-1; abest=0;
for al = 0:2:16
  r = glide(g,a,m_sus,rho6,al,d_design);
  if r.GR>best, best=r.GR; abest=al; end
  printf('%8.1f %7.3f %7.3f %7.2f %7.2f %7.2f\n',al,r.CL,r.CD,r.GR,r.Vz,r.Vx);
end
printf('  best GR %.2f at alpha %.1f deg (LE trim is %.2f deg)\n',best,abest,g.alpha_trim_deg);

printf('\n=== WIND PENETRATION LIMIT ===\n');
for d = [d_design d_bought]
  r = glide(g,a,m_sus,rho6,g.alpha_trim_deg,d);
  printf('  d=%.1fmm: airspeed Vx=%.2f m/s -> zero ground progress at headwind %.2f m/s (%.0f km/h)\n', ...
         d*1000, r.Vx, r.Vx, r.Vx*3.6);
end
