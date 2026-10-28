% run12_data.m - compute mission data and write CSV for plotting
source('parafoil_lib.m');
g=geom_LE(); SITE=80; [~,~,rho]=isa_atm(SITE+600);
d=0.0006; CL0=0.0334; CDp=0.053; M=1.000;
OUT='D:/linux/Software/Claude/cansat_parafoil/09_plots';
mkdir(OUT);
a=aero_cfg(); a.CL0=CL0; a.CD0_canopy=CDp+0.0825;
r=glide(g,a,M,rho,g.alpha_trim_deg,d);
printf('1.0 kg trim: GR %.2f  Vz %.2f  Vx %.2f\n',r.GR,r.Vz,r.Vx);
function w=wind_at(h,w10,al), w=w10.*max(h,1).^al./(10^al); end
function [X,Y,H,T]=fly(r,w10,wdir,x0,y0,h0,trmax)
  dt=0.5; x=x0;y=y0;h=h0;psi=atan2(-x,-y); hf=120; X=[];Y=[];H=[];T=[];t=0;
  while h>0
    w=wind_at(h,w10,0.17); wx=w*sin(wdir); wy=w*cos(wdir);
    if h>hf
      wf=wind_at(hf/2,w10,0.17); gs=max(r.Vx-wf,0.4); df=gs*(hf/r.Vz);
      dx=df*sin(wdir)-x; dy=df*cos(wdir)-y; dr=hypot(dx,dy);
      if dr>40, pc=atan2(dx,dy); else pc=atan2(dx,dy)+pi/2; end
    else
      pc=atan2(-x,-y); ca=-wx*cos(pc)+wy*sin(pc);
      pc=pc+asin(max(-0.9,min(0.9,ca/max(r.Vx,0.5))));
    end
    e=atan2(sin(pc-psi),cos(pc-psi));
    psi=psi+max(-trmax*dt,min(trmax*dt,e));
    x=x+(r.Vx*sin(psi)+wx)*dt; y=y+(r.Vx*cos(psi)+wy)*dt; h=h-r.Vz*dt; t=t+dt;
    X=[X x];Y=[Y y];H=[H h];T=[T t];
  end
end
t1=400/18;
for w10=[1.0 2.0 3.0]
  off=wind_at(800,w10,0.17)*t1;
  [X,Y,H,T]=fly(r,w10,0,0,-off,600,0.25);
  M2=[T(:) H(:) X(:) Y(:)];
  csvwrite(sprintf('%s/track_w%02d.csv',OUT,round(w10*10)),M2);
  printf('  wind %.1f: start %.0f m downwind, lands %.1f m from pad, %d samples\n', ...
         w10,off,hypot(X(end),Y(end)),numel(T));
end
N=400; rand('seed',5); randn('seed',5); LX=zeros(N,1); LY=zeros(N,1);
for i=1:N
  w10=min(1.0+1.2*abs(randn()),7.0); wdir=2*pi*rand();
  aa=aero_cfg(); aa.CL0=CL0+0.02*randn(); aa.CD0_canopy=CDp+0.05+0.08*rand();
  rr=glide(g,aa,M+0.04*randn(),rho,g.alpha_trim_deg+randn(),d);
  off=wind_at(800,w10,0.17)*t1;
  [X,Y,H,T]=fly(rr,w10,wdir,-off*sin(wdir),-off*cos(wdir),600,0.25);
  LX(i)=X(end); LY(i)=Y(end);
end
csvwrite([OUT '/dispersion.csv'],[LX LY]);
mm=sort(hypot(LX,LY));
printf('dispersion: CEP50 %.0f m  CEP90 %.0f m  (n=%d)\n',mm(round(.5*N)),mm(round(.9*N)),N);
csvwrite([OUT '/trim.csv'],[M r.GR r.Vz r.Vx 600/r.Vz 600*r.GR]);
