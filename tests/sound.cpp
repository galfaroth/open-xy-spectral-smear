#include "../fx.cpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
using namespace opxy;
void require(bool value,const char* text) { if(!value){std::fprintf(stderr,"%s\n",text);std::exit(1);} }
const double pi=3.14159265358979323846;
struct Runner {
    spectralsmear::State state;
    std::vector<i32> memory;
    i32 p[8];
    Runner():memory(spectralsmear::buffer_words,0x55555555),p{0,0,0,0x7fffffff} {spectralsmear::initialize(state);}
    void block(i32* l,i32* r) {spectralsmear::process(state,Buffer{memory.data(),static_cast<u32>(memory.size())},l,r,p);}
};
int main() {
    i32 real[128],imag[128],original[128];
    for(u32 i=0;i<128;++i){original[i]=real[i]=static_cast<i32>((1<<27)*std::sin(2*pi*5*i/128));imag[i]=0;}
    spectralsmear::fft(real,imag,false);
    require(std::abs(imag[5]+(1<<26))<256&&std::abs(imag[123]-(1<<26))<256,"FFT must identify signed bin-five sine components");
    for(u32 k=0;k<128;++k) if(k!=5&&k!=123) require(std::abs(real[k])<256&&std::abs(imag[k])<256,"FFT bin leakage");
    spectralsmear::fft(real,imag,true);
    for(u32 i=0;i<128;++i)require(std::abs(real[i]-original[i])<4096,"normalized FFT/IFFT identity");
    Runner clean;
    i32 l[64],r[64];
    for(u32 b=0;b<30;++b){for(u32 i=0;i<64;++i)l[i]=r[i]=0;clean.block(l,r);for(u32 i=0;i<64;++i)require(!l[i]&&!r[i],"poisoned scratch must not leak into silence");}
    std::vector<i32> input(8192),output(8192);
    u32 rng=7; long long leakage=0;
    for(u32 b=0;b<128;++b) {
        for(u32 i=0;i<64;++i){rng=rng*1664525u+1013904223u;input[b*64+i]=l[i]=static_cast<i32>(rng>>4)-(1<<27);r[i]=0;}
        clean.block(l,r);
        for(u32 i=0;i<64;++i){output[b*64+i]=l[i];if(std::abs(static_cast<long long>(r[i]))>leakage)leakage=std::abs(static_cast<long long>(r[i]));}
    }
    double error=0,energy=0;long long maximum=0;
    for(u32 i=512;i<input.size();++i){const long long e=static_cast<long long>(output[i])-input[i-128];maximum=std::abs(e)>maximum?std::abs(e):maximum;error+=double(e)*e;energy+=double(input[i-128])*input[i-128];}
    std::printf("neutral max error %lld Q31, relative RMS %.8f\n",maximum,std::sqrt(error/energy));
    require(maximum<32768&&error/energy<0.00000001,"neutral Hann overlap-add must preserve delayed broadband signal");
    Runner impulse;u32 peak_at=0;i32 peak=0;
    for(u32 b=0;b<64;++b){for(u32 i=0;i<64;++i){l[i]=b*64+i==2048?1<<28:0;r[i]=0;}impulse.block(l,r);for(u32 i=0;i<64;++i)if(l[i]>peak){peak=l[i];peak_at=b*64+i;}}
    require(peak_at==2048+spectralsmear::latency&&std::abs(peak-(1<<28))<32768,"neutral impulse amplitude and exact 128-sample latency");
    Runner smear;smear.p[0]=0x7fffffff;
    double early=0,late=0;
    for(u32 b=0;b<3000;++b){
        for(u32 i=0;i<64;++i){l[i]=b<320?static_cast<i32>((1<<28)*std::sin(2*pi*5*(b*64+i)/128)):0;r[i]=0;}
        smear.block(l,r);
        for(u32 i=0;i<64;++i){if(b>=330&&b<400)early+=double(l[i])*l[i];if(b>=2900)late+=double(l[i])*l[i];if(std::abs(static_cast<long long>(r[i]))>leakage)leakage=std::abs(static_cast<long long>(r[i]));}
    }
    std::printf("stereo leakage %lld Q31\n",leakage);
    require(leakage<32768,"packed stereo leakage must stay below -96 dBFS");
    require(early>1e15&&late<early*0.0001,"phase-propagated spectral memory must sustain then decay");
    Runner flat,spread;spread.p[1]=0x7fffffff;
    double difference=0,flat_energy=0;
    for(u32 b=0;b<100;++b){
        i32 l2[64],r2[64];
        for(u32 i=0;i<64;++i){l[i]=l2[i]=static_cast<i32>((1<<27)*std::sin(2*pi*5*(b*64+i)/128));r[i]=r2[i]=0;}
        flat.block(l,r);spread.block(l2,r2);
        if(b>10)for(u32 i=0;i<64;++i){const double d=double(l[i])-l2[i];difference+=d*d;flat_energy+=double(l[i])*l[i];}
    }
    require(difference>flat_energy*.01,"frequency diffusion must change real spectral resynthesis");
    for(u32 i=0;i<4;++i)smear.p[i]=0x7fffffff;
    for(u32 b=0;b<100;++b){for(u32 i=0;i<64;++i){l[i]=(i&1)?0x7fffffff:(-0x7fffffff-1);r[i]=-(l[i]/2);}smear.block(l,r);}
    spectralsmear::initialize(smear.state);
    for(u32 b=0;b<10;++b){for(u32 i=0;i<64;++i)l[i]=r[i]=0;smear.block(l,r);for(u32 i=0;i<64;++i)require(!l[i]&&!r[i],"reset removes a previously loud spectral tail");}
    for(u32 i=0;i<64;++i)l[i]=r[i]=1<<28;
    spectralsmear::process(smear.state,Buffer{nullptr,0},l,r,smear.p);
    for(u32 i=0;i<64;++i)require(!l[i]&&!r[i],"missing scratch returns silence");
    return 0;
}
