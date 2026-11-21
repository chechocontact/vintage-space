// Renders every preset on a synthetic sung vowel at 128 BPM:
// 2 bars dry, 3.5 bars build to 100 %, half a bar at 110 % (silence), drop, 2 bars dry.
// Usage: RenderDemo out.wav   (a tuning aid for presets; not shipped in the plugin)
#include "RiserEngine.h"
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <string>
using namespace voxlift;
static void wav(const char* path,const std::vector<float>& l,const std::vector<float>& r,int sr){
  FILE* f=fopen(path,"wb"); uint32_t n=l.size(), data=n*4, riff=36+data; uint16_t ch=2,bits=16,fmt=1,ba=4; uint32_t sr32=sr,br=sr*4,f16=16;
  fwrite("RIFF",1,4,f);fwrite(&riff,4,1,f);fwrite("WAVEfmt ",1,8,f);fwrite(&f16,4,1,f);fwrite(&fmt,2,1,f);fwrite(&ch,2,1,f);fwrite(&sr32,4,1,f);fwrite(&br,4,1,f);fwrite(&ba,2,1,f);fwrite(&bits,2,1,f);fwrite("data",1,4,f);fwrite(&data,4,1,f);
  for(uint32_t i=0;i<n;++i){for(float v:{l[i],r[i]}){float c=std::fmax(-1.f,std::fmin(1.f,v));int16_t s=(int16_t)lrintf(c*32767);fwrite(&s,2,1,f);}}
  fclose(f);}
int main(int argc,char** argv){
  if(argc<2){std::printf("usage: RenderDemo out.wav\n");return 1;}
  const int sr=48000; const double bpm=128, bar=4*60/bpm; const int barS=(int)(bar*sr);
  std::vector<float> L,R;
  dsp::Svf f1,f2,f3; for(auto*f:{&f1,&f2,&f3}) f->setSampleRate(sr);
  f1.setParams(750,8); f2.setParams(1150,9); f3.setParams(2800,10);
  const double notes[4]={220.0,246.94,261.63,293.66};
  for(int p=0;p<numPresets;++p){
    const int total=8*barS; std::vector<float> l(total),r(total);
    double ph=0;
    for(int i=0;i<total;++i){
      double t=(double)i/sr; double beatPos=t*bpm/60; int note=((int)(beatPos/2))%4;
      double f0=notes[note]*(1+0.006*std::sin(2*3.141592653589793*5.5*t)); ph+=f0/sr; ph-=std::floor(ph);
      float saw=(float)(2*ph-1);
      double sy=std::fmod(beatPos,1.0); float envv=(float)(sy<0.85?std::fmin(1.0,sy*30)*std::fmin(1.0,(0.85-sy)*30):0.0);
      float v=(f1.bandpass(saw)*1.0f+f2.bandpass(saw)*0.6f+f3.bandpass(saw)*0.3f)*0.35f*envv;
      l[i]=r[i]=v;}
    RiserEngine e; Controls c; c.preset=p; c.bpm=bpm; e.prepare(sr,c);
    for(int s=0;s<total;s+=256){
      int len=std::min(256,total-s); double b=(double)s/barS;
      float x= b<2?0.f : b<5.5? (float)((b-2)/3.5) : b<6? 1.1f : 0.f;
      c.intensity=x; c.ppqAtBlockStart=(double)s/sr*bpm/60;
      e.process(l.data()+s,r.data()+s,len,c);}
    L.insert(L.end(),l.begin(),l.end()); R.insert(R.end(),r.begin(),r.end());
    float pk=0; for(int i=0;i<total;++i) pk=std::fmax(pk,std::fabs(l[i]));
    std::printf("preset %d (%s) peak %.2f\n",p,std::string(getPreset(p).name).c_str(),pk);
  }
  wav(argv[1],L,R,sr);
}
