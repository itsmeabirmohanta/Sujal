% run04_guidance.m - why naive homing fails, and energy-managed guidance
source('parafoil_lib.m');
a=aero_cfg(); a.CD0_canopy=0.18; g=geom_LE();
SITE=80; [~,~,rho6]=isa_atm(SITE+600); m=0.860; d=0.0006;
r0=glide(g,a,m,rho6,g.alpha_trim_deg,d);

printf('=== THE ENERGY PROBLEM ===\n');
printf('  glide ratio          %.2f\n',r0.GR);
printf('  still-air range      %.0f m from 600 m\n',600*r0.GR);
printf('  typical offset at 600 m (wind 1-3 m/s)  47-140 m\n');
printf('  EXCESS RANGE RATIO   %.1fx to %.1fx\n',600*r0.GR/140,600*r0.GR/47);
printf('  -> the system has far more energy than it needs. Altitude MUST be\n');
printf('     deliberately burned off, or the vehicle wanders until it hits ground.\n\n');

function w=wind_at(h,w10,al), w=w10.*max(h,1).^al./(10^al); end

% ---- guidance mode 1: naive homing ----
function [xf,yf]=fly_homing(r,w10,wdir,x0,y0,h0,trmax)
  dt=0.25; x=x0;y=y0;h=h0;psi=atan2(-x,-y);
  while h>0
    w=wind_at(h,w10,0.17); wx=w*sin(wdir); wy=w*cos(wdir);
    pc=atan2(-x,-y);
    e=atan2(sin(pc-psi),cos(pc-psi));
    psi=psi+max(-trmax*dt,min(trmax*dt,e));
    x=x+(r.Vx*sin(psi)+wx)*dt; y=y+(r.Vx*cos(psi)+wy)*dt; h=h-r.Vz*dt;
  end
  xf=x;yf=y;
end

% ---- guidance mode 2: energy-managed (loiter + into-wind final) ----
function [xf,yf,ph]=fly_em(r,w10,wdir,x0,y0,h0,trmax)
  dt=0.25; x=x0;y=y0;h=h0;psi=atan2(-x,-y);
  % final approach: into wind, from downwind of target
  h_fin=100;                                  % start final at 100 m AGL
  wf=wind_at(h_fin/2,w10,0.17);
  gs_fin=max(r.Vx-wf,0.5);                    % into-wind groundspeed
  d_fin=gs_fin*(h_fin/r.Vz);                  % ground distance covered on final
  R_loit=60;                                  % loiter radius, m
  ph=0;
  while h>0
    w=wind_at(h,w10,0.17); wx=w*sin(wdir); wy=w*cos(wdir);
    rng=hypot(x,y);
    if h>h_fin
      % hold over the IP: a point d_fin DOWNWIND of the target
      ipx=d_fin*sin(wdir); ipy=d_fin*cos(wdir);
      dx=ipx-x; dy=ipy-y; dr=hypot(dx,dy);
      if dr>R_loit
        pc=atan2(dx,dy); ph=1;                % transit to IP
      else
        pc=atan2(dx,dy)+pi/2; ph=2;           % orbit the IP, burning altitude
      end
    else
      pc=atan2(-wx,-wy); ph=3;                % final: into wind, toward target
    end
    e=atan2(sin(pc-psi),cos(pc-psi));
    psi=psi+max(-trmax*dt,min(trmax*dt,e));
    x=x+(r.Vx*sin(psi)+wx)*dt; y=y+(r.Vx*cos(psi)+wy)*dt; h=h-r.Vz*dt;
  end
  xf=x;yf=y;
end

printf('=== DETERMINISTIC COMPARISON (offset = downwind drift from phase 1) ===\n');
printf('%9s %9s %14s %14s\n','w10[m/s]','offset[m]','homing miss[m]','energy-mgd[m]');
t1=400/18;
for w10=[0.5 1.0 1.5 2.0 2.5 3.0 3.5 4.0]
  off=wind_at(800,w10,0.17)*t1; wdir=0;
  x0=-off*sin(wdir); y0=-off*cos(wdir);
  [xh,yh]=fly_homing(r0,w10,wdir,x0,y0,600,0.25);
  [xe,ye]=fly_em(r0,w10,wdir,x0,y0,600,0.25);
  printf('%9.1f %9.0f %14.1f %14.1f\n',w10,off,hypot(xh,yh),hypot(xe,ye));
end

printf('\n=== MONTE CARLO, both guidance laws (2000 runs) ===\n');
N=2000; rand('seed',7); randn('seed',7);
mh=zeros(N,1); me=zeros(N,1); nopen=0;
for i=1:N
  w10=min(1.0+1.2*abs(randn()),7.0); wdir=2*pi*rand();
  cd0=0.15+0.08*rand(); mm=0.86+0.04*randn();
  aa=a; aa.CD0_canopy=cd0; rr=glide(g,aa,mm,rho6,g.alpha_trim_deg,d);
  if wind_at(300,w10,0.17)>rr.Vx, nopen=nopen+1; end
  off=wind_at(800,w10,0.17)*t1;
  x0=-off*sin(wdir); y0=-off*cos(wdir);
  [xh,yh]=fly_homing(rr,w10,wdir,x0,y0,600,0.25); mh(i)=hypot(xh,yh);
  [xe,ye]=fly_em(rr,w10,wdir,x0,y0,600,0.25);     me(i)=hypot(xe,ye);
end
sh=sort(mh); se=sort(me);
printf('%-14s %10s %10s\n','metric','homing','energy-mgd');
printf('%-14s %10.1f %10.1f\n','CEP50 [m]',sh(round(.50*N)),se(round(.50*N)));
printf('%-14s %10.1f %10.1f\n','CEP90 [m]',sh(round(.90*N)),se(round(.90*N)));
printf('%-14s %10.1f %10.1f\n','CEP95 [m]',sh(round(.95*N)),se(round(.95*N)));
printf('%-14s %10.1f %10.1f\n','mean [m]',mean(mh),mean(me));
printf('%-14s %10.1f %10.1f\n','worst [m]',max(mh),max(me));
printf('%-14s %9.1f%% %9.1f%%\n','within 25 m',100*sum(mh<25)/N,100*sum(me<25)/N);
printf('%-14s %9.1f%% %9.1f%%\n','within 50 m',100*sum(mh<50)/N,100*sum(me<50)/N);
printf('%-14s %9.1f%% %9.1f%%\n','within 100 m',100*sum(mh<100)/N,100*sum(me<100)/N);
printf('\n  P(wind aloft exceeds airspeed, unrecoverable) = %.1f %%\n',100*nopen/N);
