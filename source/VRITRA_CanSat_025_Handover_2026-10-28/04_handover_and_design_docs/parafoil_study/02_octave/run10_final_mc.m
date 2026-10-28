% run10_final_mc.m - FINAL dispersion, CFD-calibrated section aerodynamics
source('parafoil_lib.m');
g=geom_LE(); SITE=80; [~,~,rho]=isa_atm(SITE+600); t1=400/18; d=0.0006;
CL0=0.0334; CDp=0.053;
function w=wind_at(h,w10,al), w=w10.*max(h,1).^al./(10^al); end
function [xf,yf]=fly(r,w10,wdir,x0,y0,h0,trmax)
  dt=0.25; x=x0;y=y0;h=h0;psi=atan2(-x,-y); hf=120;
  while h>0
    w=wind_at(h,w10,0.17); wx=w*sin(wdir); wy=w*cos(wdir);
    if h>hf
      wfin=wind_at(hf/2,w10,0.17); gs=max(r.Vx-wfin,0.4); dfin=gs*(hf/r.Vz);
      dx=dfin*sin(wdir)-x; dy=dfin*cos(wdir)-y; dr=hypot(dx,dy);
      if dr>40, pc=atan2(dx,dy); else pc=atan2(dx,dy)+pi/2; end
    else
      pc=atan2(-x,-y); ca=-wx*cos(pc)+wy*sin(pc);
      pc=pc+asin(max(-0.9,min(0.9,ca/max(r.Vx,0.5))));
    end
    e=atan2(sin(pc-psi),cos(pc-psi));
    psi=psi+max(-trmax*dt,min(trmax*dt,e));
    x=x+(r.Vx*sin(psi)+wx)*dt; y=y+(r.Vx*cos(psi)+wy)*dt; h=h-r.Vz*dt;
  end
  xf=x;yf=y;
end
a=aero_cfg(); a.CL0=CL0; a.CD0_canopy=CDp+0.0825;   % mid of the 0.075-0.090 band
rn=glide(g,a,0.860,rho,g.alpha_trim_deg,d);
printf('=== CFD-CALIBRATED NOMINAL ===\n');
printf('  CL %.3f  CD %.3f  GR %.2f  Vz %.2f  Vx %.2f m/s  descent %.0f s\n', ...
        rn.CL,rn.CD,rn.GR,rn.Vz,rn.Vx,600/rn.Vz);
printf('=== MONTE CARLO 3000 runs ===\n');
N=3000; rand('seed',2024); randn('seed',2024);
ms=zeros(N,1); nb=0; vz=zeros(N,1);
for i=1:N
  w10=min(1.0+1.2*abs(randn()),7.0); wdir=2*pi*rand();
  aa=aero_cfg(); aa.CL0=CL0+0.02*randn();
  aa.CD0_canopy=CDp+0.05+0.08*rand();           % inlet increment 0.05-0.13
  rr=glide(g,aa,0.860+0.04*randn(),rho,g.alpha_trim_deg+1.0*randn(),d);
  vz(i)=rr.Vz;
  if wind_at(300,w10,0.17)>rr.Vx, nb=nb+1; end
  off=wind_at(800,w10,0.17)*t1;
  [xf,yf]=fly(rr,w10,wdir,-off*sin(wdir),-off*cos(wdir),600,0.25);
  ms(i)=hypot(xf,yf);
end
s=sort(ms);
printf('  CEP50 %6.1f m   CEP90 %6.1f m   CEP95 %6.1f m\n', ...
   s(round(.5*N)),s(round(.9*N)),s(round(.95*N)));
printf('  mean %6.1f m   worst %6.1f m\n',mean(ms),max(ms));
printf('  within  25 m %5.1f %%\n',100*sum(ms<25)/N);
printf('  within  50 m %5.1f %%\n',100*sum(ms<50)/N);
printf('  within 100 m %5.1f %%\n',100*sum(ms<100)/N);
printf('  within 200 m %5.1f %%\n',100*sum(ms<200)/N);
printf('  P(blown downwind) %.1f %%   (19.8 %% first pass, 12.3 %% prev)\n',100*nb/N);
printf('  R-08 met in %.1f %% of runs; Vz %.2f..%.2f m/s\n', ...
   100*sum(vz>=1&vz<=3)/N,min(vz),max(vz));
