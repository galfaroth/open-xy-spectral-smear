#include "core.h"
#include "tables.h"
#include <opxy/q31.hpp>
namespace spectralsmear {
namespace {
using namespace opxy;
i32 control(i32 value) { return value < 0 ? 0 : value; }
i32 glide(i32 value, i32 target) { const i32 d=target-value; return value+(d/64?d/64:(d>0)-(d<0)); }
// Radix-two DIT: normalized forward, unnormalized inverse. Stereo input is /32:
// Parseval gives spectral l1 <= sqrt(128)/32 per real spectrum; their sum stays below .71 full scale. Phase rotation,
// Reflected edge diffusion preserves the conjugate-spectrum weighted l1 norm
// (interior bins count twice). Convex temporal mixing and tone attenuation preserve that
// bound, so every butterfly/partial inverse sum fits signed Q31 without clipping.
void fft(i32* real, i32* imag, bool inverse) {
    for(u32 i=0;i<transform_size;++i) {
        const u32 j=tables::reverse[i];
        if(j>i) { i32 x=real[i];real[i]=real[j];real[j]=x; x=imag[i];imag[i]=imag[j];imag[j]=x; }
    }
    for(u32 span=2;span<=transform_size;span*=2) {
        const u32 half=span/2, step=transform_size/span;
        for(u32 base=0;base<transform_size;base+=span) for(u32 j=0;j<half;++j) {
            const u32 a=base+j,b=a+half,k=j*step;
            i32 ar=real[a],ai=imag[a],br=real[b],bi=imag[b];
            if(!inverse) { ar=q31::half(ar);ai=q31::half(ai);br=q31::half(br);bi=q31::half(bi); }
            i32 tr,ti;
            if(!k) { tr=br;ti=bi; }
            else if(k==transform_size/4) { tr=inverse?-bi:bi;ti=inverse?br:-br; }
            else {
                const i32 c=tables::cosine[k],s=inverse?tables::sine[k]:-tables::sine[k];
                tr=q31::multiply(br,c)-q31::multiply(bi,s);
                ti=q31::multiply(br,s)+q31::multiply(bi,c);
            }
            real[a]=ar+tr;imag[a]=ai+ti;
            real[b]=ar-tr;imag[b]=ai-ti;
        }
    }
}
const u32 bins=transform_size/2+1;
// Pack two real channels in one complex FFT. Positive A bins use k; B bins
// use N-k, with four separate edge values for DC/Nyquist.
i32& current(i32* work,u32 channel,u32 k,bool imaginary) {
    if(!channel) return work[(imaginary?transform_size:0)+k];
    if(!k||k==transform_size/2) return work[2*transform_size+(k?2:0)+(imaginary?1:0)];
    return work[(imaginary?transform_size:0)+transform_size-k];
}
void spectrum(i32* work,i32* history,u32 channel,i32 retain,i32 spread,i32 tone) {
    for(u32 k=0;k<bins;++k) {
        i32& re=current(work,channel,k,false);i32& im=current(work,channel,k,true);
        const i32 old_re=k&1?-history[k]:history[k],old_im=k&1?-history[bins+k]:history[bins+k];
        re+=q31::multiply(old_re-re,retain);im+=q31::multiply(old_im-im,retain);
    }
    for(u32 k=0;k<bins;++k) {
        const u32 before=k?k-1:1,after=k==bins-1?k-1:k+1;
        const i32 re=current(work,channel,k,false),im=current(work,channel,k,true);
        const i32 near_re=q31::half(current(work,channel,before,false))+q31::half(current(work,channel,after,false));
        const i32 near_im=(!k||k==bins-1)?0:q31::half(current(work,channel,before,true))+q31::half(current(work,channel,after,true));
        const i32 gain=q31::maximum-(tone/static_cast<i32>(transform_size/2))*static_cast<i32>(k);
        history[k]=q31::multiply(re+q31::multiply(near_re-re,spread),gain);
        history[bins+k]=q31::multiply(im+q31::multiply(near_im-im,spread),gain);
    }
}
void frame(i32* buffer,u32 write,i32 retain,i32 spread,i32 tone) {
    i32* real=buffer+4*ring_size;i32* imag=real+transform_size;
    i32* history=real+2*transform_size+4;
    for(u32 i=0;i<transform_size;++i) {
        const u32 at=(write-transform_size+i)&(ring_size-1);
        real[i]=q31::multiply(buffer[at]/32,tables::window[i]);
        imag[i]=q31::multiply(buffer[ring_size+at]/32,tables::window[i]);
    }
    fft(real,imag,false);
    for(u32 k=0;k<bins;++k) {
        const u32 mirror=(transform_size-k)&(transform_size-1);
        const i32 ar=q31::half(real[k]+real[mirror]),ai=q31::half(imag[k]-imag[mirror]);
        const i32 br=q31::half(imag[k]+imag[mirror]),bi=q31::half(real[mirror]-real[k]);
        current(real,0,k,false)=ar;current(real,0,k,true)=ai;
        current(real,1,k,false)=br;current(real,1,k,true)=bi;
    }
    spectrum(real,history,0,retain,spread,tone);
    spectrum(real,history+2*bins,1,retain,spread,tone);
    for(u32 k=0;k<bins;++k) {
        const u32 mirror=(transform_size-k)&(transform_size-1);
        const i32 ar=history[k],ai=history[bins+k],br=history[2*bins+k],bi=history[3*bins+k];
        real[k]=ar-bi;imag[k]=ai+br;real[mirror]=ar+bi;imag[mirror]=br-ai;
    }
    fft(real,imag,true);
    for(u32 i=0;i<transform_size;++i) {
        i32 l=real[i],r=imag[i];
        for(u32 gain=0;gain<5;++gain) {l=q31::add(l,l);r=q31::add(r,r);}
        const u32 at=(write+i)&(ring_size-1);
        buffer[2*ring_size+at]=q31::add(buffer[2*ring_size+at],l);
        buffer[3*ring_size+at]=q31::add(buffer[3*ring_size+at],r);
    }
}
}
void initialize(State& s) { s=State(); }
void process(State& s,opxy::Buffer buffer,opxy::Channel left,opxy::Channel right,opxy::Parameters p) {
    using namespace opxy;
    if(!buffer.words||buffer.count<buffer_words) {
        initialize(s);for(u32 i=0;i<block_frames;++i)left[i]=right[i]=0;return;
    }
    if(!s.initialized) {
        // Fixed 6 KiB preparation on the first process call; initialize stays O(1).
        for(u32 i=0;i<buffer_words;++i)buffer.words[i]=0;
        s.initialized=1;
    }
    i32* in_l=buffer.words; i32* in_r=in_l+ring_size;
    i32* out_l=in_r+ring_size; i32* out_r=out_l+ring_size;
    for(u32 i=0;i<block_frames;++i) {
        const u32 at=s.write;
        in_l[at]=left[i];in_r[at]=right[i];
        s.level=glide(s.level,control(p[3]));
        left[i]=q31::multiply(out_l[at],s.level);right[i]=q31::multiply(out_r[at],s.level);
        out_l[at]=out_r[at]=0;
        s.write=(s.write+1)&(ring_size-1);
    }
    i32 remaining=q31::maximum-control(p[0]);
    remaining=q31::multiply(remaining,remaining);remaining=q31::multiply(remaining,remaining);
    const i32 retain=q31::multiply(q31::maximum-remaining,2141041197);
    frame(buffer.words,s.write,retain,control(p[1])/2,control(p[2]));
}
}
