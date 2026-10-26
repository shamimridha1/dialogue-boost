#include "../src/dsp.h"
#include <cstdio>
#include <random>
int main(){
  const double fs=48000; const int N=fs*10;
  std::vector<float> L(N),R(N);
  std::mt19937 rng(1); std::normal_distribution<double> nd(0,1);
  // speech-like: bursts of formant tones + noise, peaks near -0.9 dBFS, plus a few transients
  double ph1=0,ph2=0,ph3=0;
  for(int i=0;i<N;i++){double t=i/fs; double env=0.5+0.5*std::sin(2*M_PI*3*t); env=env*env;
    ph1+=2*M_PI*140/fs; ph2+=2*M_PI*700/fs; ph3+=2*M_PI*2600/fs;
    double s=env*(0.45*std::sin(ph1)+0.25*std::sin(ph2)+0.12*std::sin(ph3)+0.05*nd(rng));
    if(i%24000<40) s+=0.4; // transient clicks
    L[i]=R[i]=(float)std::max(-0.9,std::min(0.9,s));}
  auto rms=[&](std::vector<float>&v){double a=0;for(float x:v)a+=x*x;return 20*std::log10(std::sqrt(a/v.size()));};
  auto pk=[&](std::vector<float>&v){double a=0;for(float x:v)a=std::max(a,(double)std::fabs(x));return 20*std::log10(a);};
  printf("in : rms %.2f dB peak %.2f dB\n",rms(L),pk(L));
  for(double boost: {0.0,3.0,6.0,12.0}){
    std::vector<float> a=L,b=R; dlb::Processor p; p.boostDb=boost; p.prepare(fs,2);
    for(int i=0;i<N;i+=512){int n=std::min(512,N-i); float* io[2]={a.data()+i,b.data()+i}; p.process(io,io,2,n);}
    bool nan=false; for(float x:a) if(!std::isfinite(x)) nan=true;
    printf("boost %4.1f: rms %.2f dB peak %.2f dB (ceiling -1.5) nan=%d\n",boost,rms(a),pk(a),nan);
  }
}
