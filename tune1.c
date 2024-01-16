// ATU-10 QRP 10 watts automatic antenna tuner with AD8361 true RMS detectors
// David Fainitski, N7DDC
// 2020, 2022


// test stubs imiplemented in impedance.c
extern void delay_ms(int ms);
extern void get_SWR();
extern void Relay_set(char L, char C, char I);

// hack for case insentive linking in function names
#define get_swr get_SWR
#define Delay_ms delay_ms

// this allows us to have two 'tune' functions, an old one here and another new one in tune2.c
#define tune(A) tune1(A)


void tune(void);
static void subtune(void);
static void coarse_tune(void);
static void coarse_cap(void);
static void coarse_ind(void);
static void coarse_ind_cap(void);
static void sharp_tune(void);
static void sharp_cap(void);
static void sharp_ind(void);


// The following code is pulled from main.c unmodified. Just the stuff that is needed to test tuning.
// static was added to all functions except 'tune' to allow linking two copies of the tuning algorithm.

int SWR;
char ind = 0, cap = 0, SW = 0;



//
void tune(void){
   int SWR_mem;
   char cap_mem, ind_mem;
   subtune();
   get_SWR();
   if(SWR<=120) return;
   SWR_mem = SWR;
   cap_mem = cap;
   ind_mem = ind;
   if(SW==1) SW = 0;
   else SW = 1;
   subtune();
   get_SWR();
   if(SWR>SWR_mem){
      if(SW==1) SW = 0;
      else SW = 1;
      cap = cap_mem;
      ind = ind_mem;
      Relay_set(ind, cap, SW);
      Delay_ms(5);
      get_SWR();
   }
   if(SWR<=120) return;
   sharp_tune();
   get_SWR();
   return;
}

//
static void subtune(void){
   cap = 0;
   ind = 0;
   Relay_set(ind, cap, SW);
   delay_ms(50);
   get_SWR();
   if(SWR<=120) return;
   coarse_tune();
   get_SWR();
   if(SWR<=120) return;
   sharp_tune();
   return;
}

//
static void coarse_tune(void){
   int SWR_mem1 = 10000, SWR_mem2 = 10000, SWR_mem3 = 10000;
   char ind_mem1, cap_mem1, ind_mem2, cap_mem2, ind_mem3, cap_mem3;
   coarse_cap();
   coarse_ind();
   get_SWR();
   if(SWR<=120) return;
   SWR_mem1 = SWR;
   ind_mem1 = ind;
   cap_mem1 = cap;
   if(cap<=2 & ind<=2){
      cap = 0;
      ind = 0;
      Relay_set(ind, cap, SW);
      Delay_ms(5);
      coarse_ind();
      coarse_cap();
      get_SWR();
      if(SWR<=120) return;
      SWR_mem2 = SWR;
      ind_mem2 = ind;
      cap_mem2 = cap;
   }
   if(cap<=2 & ind<=2){
      cap = 0;
      ind = 0;
      Relay_set(ind, cap, SW);
      Delay_ms(5);
      coarse_ind_cap();
      get_SWR();
      if(SWR<=120) return;
      SWR_mem3 = SWR;
      ind_mem3 = ind;
      cap_mem3 = cap;
   }
   if(SWR_mem1<=SWR_mem2 & SWR_mem1<=SWR_mem3){
      cap = cap_mem1;
      ind = ind_mem1;
   }
   else if(SWR_mem2<=SWR_mem1 & SWR_mem2<=SWR_mem3){
      cap = cap_mem2;
      ind = ind_mem2;
   }
   else if(SWR_mem3<=SWR_mem1 & SWR_mem3<=SWR_mem2){
      cap = cap_mem3;
      ind = ind_mem3;
   }
   return;
}

//
static void coarse_ind_cap(void){
   int SWR_mem;
   char ind_mem;
   ind_mem = 0;
   get_swr();
   SWR_mem = SWR / 10;
   for(ind=1; ind<64; ind*=2){
      Relay_set(ind, ind, SW);
      Delay_ms(5);
      get_swr();
      SWR = SWR/10;
      if(SWR<=SWR_mem){
         ind_mem = ind;
         SWR_mem = SWR;
      }
      else
         break;
   }
   ind = ind_mem;
   cap = ind_mem;
   Relay_set(ind, cap, SW);
   Delay_ms(5);
   return;
}

//
static void coarse_cap(void){
   int SWR_mem;
   char cap_mem;
   cap_mem = 0;
   get_swr();
   SWR_mem = SWR / 10;
   for(cap=1; cap<64; cap*=2){
      Relay_set(ind, cap, SW);
      Delay_ms(5);
      get_swr();
      SWR = SWR/10;
      if(SWR<=SWR_mem){
         cap_mem = cap;
         SWR_mem = SWR;
      }
      else
         break;
   }
   cap = cap_mem;
   Relay_set(ind, cap, SW);
   Delay_ms(5);
   return;
}

//
static void coarse_ind(void){
   int SWR_mem;
   char ind_mem;
   ind_mem = 0;
   get_swr();
   SWR_mem = SWR / 10;
   for(ind=1; ind<64; ind*=2){
      Relay_set(ind, cap, SW);
      Delay_ms(5);
      get_swr();
      SWR = SWR/10;
      if(SWR<=SWR_mem){
         ind_mem = ind;
         SWR_mem = SWR;
      }
      else
         break;
   }
   ind = ind_mem;
   Relay_set(ind, cap, SW);
   Delay_ms(5);
   return;
}

//
static void sharp_tune(void){
   if(cap>=ind){
      sharp_cap();
      sharp_ind();
   }
   else{
      sharp_ind();
      sharp_cap();
   }
   return;
}

//
static void sharp_cap(void){
   int SWR_mem;
   char step, cap_mem;
   cap_mem = cap;
   step = cap / 10;
   if(step==0) step = 1;
   get_SWR();
   SWR_mem = SWR;
   cap += step;
   Relay_set(ind, cap, SW);
   Delay_ms(5);
   get_SWR();
   if(SWR<=SWR_mem){
      SWR_mem = SWR;
      cap_mem = cap;
      for(cap+=step; cap<=(127-step); cap+=step){
         Relay_set(ind, cap, SW);
         Delay_ms(5);
         get_SWR();
         if(SWR<=SWR_mem){
            cap_mem = cap;
            SWR_mem = SWR;
            step = cap / 10;
            if(step==0) step = 1;
         }
         else
            break;
      }
   }
   else{
      SWR_mem = SWR;
      for(cap-=step; cap>=step; cap-=step){
         Relay_set(ind, cap, SW);
         Delay_ms(5);
         get_SWR();
         if(SWR<=SWR_mem){
            cap_mem = cap;
            SWR_mem = SWR;
            step = cap / 10;
            if(step==0) step = 1;
         }
         else
            break;
      }
   }
   cap = cap_mem;
   Relay_set(ind, cap, SW);
   Delay_ms(5);
   return;
}

//
static void sharp_ind(void){
   int SWR_mem;
   char step, ind_mem;
   ind_mem = ind;
   step = ind / 10;
   if(step==0) step = 1;
   get_SWR();
   SWR_mem = SWR;
   ind += step;
   Relay_set(ind, cap, SW);
   Delay_ms(5);
   get_SWR();
   if(SWR<=SWR_mem){
      SWR_mem = SWR;
      ind_mem = ind;
      for(ind+=step; ind<=(127-step); ind+=step){
         Relay_set(ind, cap, SW);
         Delay_ms(5);
         get_SWR();
         if(SWR<=SWR_mem){
            ind_mem = ind;
            SWR_mem = SWR;
            step = ind / 10;
            if(step==0) step = 1;
         }
         else
            break;
      }
   }
   else{
      SWR_mem = SWR;
      for(ind-=step; ind>=step; ind-=step){
         Relay_set(ind, cap, SW);
         Delay_ms(5);
         get_SWR();
         if(SWR<=SWR_mem){
            ind_mem = ind;
            SWR_mem = SWR;
            step = ind / 10;
            if(step==0) step = 1;
         }
         else
            break;
      }
   }
   ind = ind_mem;
   Relay_set(ind, cap, SW);
   Delay_ms(5);
   return;
}
//