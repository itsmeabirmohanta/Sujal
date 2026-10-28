% run03_trajectory.m - guided descent, wind profile, Monte Carlo landing dispersion
% Target == launch pad (rules 3.1 + CDR Table 16). Return-to-launch-site problem.
source('parafoil_lib.m');
a=aero_cfg(); a.CD0_canopy=0.18;      % mid estimate; LE/CDR imply 0.20-0.23
g=geom_LE();
SITE=80; [~,~,rho6]=isa_atm(SITE+600);
m=0.860; d=0.0006;
r0=glide(g,a,m,rho6,g.alpha_trim_deg,d);
printf('Trim: GR=%.2f  V=%.2f  Vz=%.2f  Vx=%.2f m/s\n\n',r0.GR,r0.V,r0.Vz,r0.Vx);

% --- wind profile: power law over flat farmland ---
function w = wind_at(h, w10, alpha)
  w = w10 .* max(h,1.0).^alpha ./ (10.0^alpha);
end

printf('=== WIND PROFILE (power law, alpha=0.17, flat farmland) ===\n');
printf('%8s %10s %10s %10s\n','h[m]','w10=1.5','w10=2.0','w10=3.0');
for h=[10 100 300 600]
  printf('%8.0f %10.2f %10.2f %10.2f\n',h,wind_at(h,1.5,0.17),wind_at(h,2.0,0.17),wind_at(h,3.0,0.17));
end

% --- drift during drag-chute phase 1000 -> 600 m ---
printf('\n=== PHASE-1 DRIFT (drag chute 1000->600 m) ===\n');
Vp1=18.0;  % CDR Table 6 design 18-22 m/s
t1=400/Vp1;
printf('  descent 400 m at %.0f m/s -> %.1f s\n',Vp1,t1);
printf('%8s %14s\n','w10[m/s]','drift[m]');
for w10=[1.0 1.5 2.0 3.0 4.0]
  wm=wind_at(800,w10,0.17);
  printf('%8.1f %14.0f\n',w10,wm*t1);
end

% --- guided descent integration ---
% State: x (east), y (north), h. Target at origin. Homing guidance with
% into-wind final approach below h_final. Servo/turn-rate limited.
function [xf,yf,tf,reach] = fly(r,w10,wdir,x0,y0,h0,alpha_p,turn_rate_max)
  dt=0.25; x=x0; y=y0; h=h0; t=0;
  psi = atan2(-x,-y);               % initial heading toward target
  reach=1;
  while h>0 && t<1200
    w = w10*max(h,1)^alpha_p/(10^alpha_p);
    wx = w*sin(wdir); wy = w*cos(wdir);   % wind blows TOWARD wdir
    % desired heading: straight at target
    psi_c = atan2(-x,-y);
    if h < 80
      psi_c = atan2(-wx,-wy);        % final approach into wind
    end
    e = atan2(sin(psi_c-psi),cos(psi_c-psi));
    psi = psi + max(-turn_rate_max*dt,min(turn_rate_max*dt,e));
    vx = r.Vx*sin(psi)+wx; vy = r.Vx*cos(psi)+wy;
    x=x+vx*dt; y=y+vy*dt; h=h-r.Vz*dt; t=t+dt;
  end
  xf=x; yf=y; tf=t;
end

printf('\n=== DETERMINISTIC CASES: offset downwind, must fly back to pad ===\n');
printf('airspeed Vx=%.2f m/s, Vz=%.2f m/s, descent time %.0f s\n',r0.Vx,r0.Vz,600/r0.Vz);
printf('%9s %10s %10s %12s %10s\n','w10[m/s]','w@300m','offset[m]','miss[m]','verdict');
for w10=[0.5 1.0 1.5 2.0 2.5 3.0 3.5 4.0]
  wm=wind_at(800,w10,0.17); off=wm*t1;          % starts downwind of pad
  wdir=0;                                        % wind toward north => drift +y
  [xf,yf,tf]=fly(r0,w10,wdir,0,off,600,0.17,0.25);
  miss=hypot(xf,yf);
  v='reaches pad'; if miss>50, v='MISSES'; end
  printf('%9.1f %10.2f %10.0f %12.1f %10s\n',w10,wind_at(300,w10,0.17),off,miss,v);
end

printf('\n=== MONTE CARLO (2000 runs) ===\n');
N=2000; rand('seed',42); randn('seed',42);
miss=zeros(N,1);
for i=1:N
  w10 = 1.0 + 1.2*abs(randn());            % mean ~1.96 m/s, right-skewed
  w10 = min(w10,7.0);
  wdir = 2*pi*rand();
  cd0 = 0.15 + 0.08*rand();                % CD0 uncertainty 0.15-0.23
  mm  = 0.86 + 0.04*randn();
  aa=a; aa.CD0_canopy=cd0;
  rr=glide(g,aa,mm,rho6,g.alpha_trim_deg,d);
  wm=wind_at(800,w10,0.17); off=wm*t1;
  x0 = -off*sin(wdir); y0 = -off*cos(wdir);   % drifted downwind
  [xf,yf,tf]=fly(rr,w10,wdir,x0,y0,600,0.17,0.25);
  miss(i)=hypot(xf,yf);
end
ms=sort(miss);
printf('  CEP50  = %7.1f m\n',ms(round(0.50*N)));
printf('  CEP90  = %7.1f m\n',ms(round(0.90*N)));
printf('  CEP95  = %7.1f m\n',ms(round(0.95*N)));
printf('  mean   = %7.1f m\n',mean(miss));
printf('  worst  = %7.1f m\n',max(miss));
printf('  within  50 m: %5.1f %%\n',100*sum(miss<50)/N);
printf('  within 100 m: %5.1f %%\n',100*sum(miss<100)/N);
printf('  within 200 m: %5.1f %%\n',100*sum(miss<200)/N);

printf('\n=== PENETRATION FAILURE RATE ===\n');
for wl=[3.5 4.0 4.5 5.0]
  printf('  wind aloft > %.1f m/s (Vx=%.2f) -> cannot make headway upwind\n',wl,r0.Vx);
end
nfail=0;
for i=1:N
  w10=min(1.0+1.2*abs(randn()),7.0);
  if wind_at(300,w10,0.17) > r0.Vx, nfail=nfail+1; end
end
printf('  P(wind@300m exceeds airspeed %.2f m/s) = %.1f %%\n',r0.Vx,100*nfail/N);
