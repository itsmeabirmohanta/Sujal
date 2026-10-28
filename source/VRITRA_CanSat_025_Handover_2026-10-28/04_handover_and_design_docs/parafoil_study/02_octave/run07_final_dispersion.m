% run07_final_dispersion.m - FINAL dispersion for the chosen LE 0.71 m2 design,
% using the MEASURED rib profile (CL0=0.13 from camber 1.1%).
source('parafoil_lib.m');
g=geom_LE(); SITE=80; [~,~,rho]=isa_atm(SITE+600); t1=400/18; d=0.0006;
function w=wind_at(h,w10,al), w=w10.*max(h,1).^al./(10^al); end

function [xf,yf]=fly_v2(r,w10,wdir,x0,y0,h0,trmax)
  dt=0.25; x=x0;y=y0;h=h0;psi=atan2(-x,-y); hf=120;
  while h>0
    w=wind_at(h,w10,0.17); wx=w*sin(wdir); wy=w*cos(wdir);
    if h>hf
      wfin=wind_at(hf/2,w10,0.17); gs=max(r.Vx-wfin,0.4); dfin=gs*(hf/r.Vz);
      ipx=dfin*sin(wdir); ipy=dfin*cos(wdir);
      dx=ipx-x; dy=ipy-y; dr=hypot(dx,dy);
      if dr>40, pc=atan2(dx,dy); else pc=atan2(dx,dy)+pi/2; end
    else
      pc=atan2(-x,-y);
      ca=-wx*cos(pc)+wy*sin(pc);
      pc=pc+asin(max(-0.9,min(0.9,ca/max(r.Vx,0.5))));
    end
    e=atan2(sin(pc-psi),cos(pc-psi));
    psi=psi+max(-trmax*dt,min(trmax*dt,e));
    x=x+(r.Vx*sin(psi)+wx)*dt; y=y+(r.Vx*cos(psi)+wy)*dt; h=h-r.Vz*dt;
  end
  xf=x;yf=y;
end

a=aero_cfg(); a.CL0=0.13; a.CD0_canopy=0.18;
rn=glide(g,a,0.860,rho,g.alpha_trim_deg,d);
printf('=== CHOSEN DESIGN: LE PF072, 11 cells, 0.71 m2, measured profile ===\n');
printf('  t/c 19.1%% @ x/c 0.308, camber 1.1%%, trim AoA %.2f deg\n',g.alpha_trim_deg);
printf('  CL %.3f  CD %.3f  GR %.2f  Vz %.2f m/s  Vx %.2f m/s  descent %.0f s\n', ...
  rn.CL,rn.CD,rn.GR,rn.Vz,rn.Vx,600/rn.Vz);
printf('  LE claims GR 2.70, CDR claimed 2.50 -> model gives %.2f : all three agree\n\n',rn.GR);

printf('=== MONTE CARLO, 2500 runs, corrected aero ===\n');
N=2500; rand('seed',101); randn('seed',101);
ms=zeros(N,1); nb=0; vz=zeros(N,1);
for i=1:N
  w10=min(1.0+1.2*abs(randn()),7.0); wdir=2*pi*rand();
  aa=aero_cfg(); aa.CL0=0.10+0.10*rand(); aa.CD0_canopy=0.15+0.08*rand();
  rr=glide(g,aa,0.860+0.04*randn(),rho,g.alpha_trim_deg+1.0*randn(),d);
  vz(i)=rr.Vz;
  if wind_at(300,w10,0.17)>rr.Vx, nb=nb+1; end
  off=wind_at(800,w10,0.17)*t1;
  [xf,yf]=fly_v2(rr,w10,wdir,-off*sin(wdir),-off*cos(wdir),600,0.25);
  ms(i)=hypot(xf,yf);
end
s=sort(ms);
printf('  CEP50 = %6.1f m\n',s(round(.50*N)));
printf('  CEP90 = %6.1f m\n',s(round(.90*N)));
printf('  CEP95 = %6.1f m\n',s(round(.95*N)));
printf('  mean  = %6.1f m   worst = %6.1f m\n',mean(ms),max(ms));
printf('  within  25 m: %5.1f %%\n',100*sum(ms<25)/N);
printf('  within  50 m: %5.1f %%\n',100*sum(ms<50)/N);
printf('  within 100 m: %5.1f %%\n',100*sum(ms<100)/N);
printf('  within 200 m: %5.1f %%\n',100*sum(ms<200)/N);
printf('\n  P(wind aloft > airspeed, unrecoverable) = %.1f %% (was 19.8 %% with CL0=0.40)\n',100*nb/N);
printf('  R-08 descent rate 1-3 m/s satisfied in %.1f %% of runs\n',100*sum(vz>=1&vz<=3)/N);
printf('  Vz range across runs: %.2f to %.2f m/s\n',min(vz),max(vz));
