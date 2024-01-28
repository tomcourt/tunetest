// ATU-10 tuning algorithm, prototype Tom Court, AC0NY

// test stubs imiplemented in impedance.c
extern void delay_ms(int ms);
extern void get_SWR();
extern void Relay_set(char L, char C, char I);

// hack for case insentive linking in function names
#define get_swr get_SWR
#define Delay_ms delay_ms

// this allows us to have two 'tune' functions, an new one here and another old one in tune.c
#define tune(A) tune2(A)


void tune(void);


// The following code is pulled from main.c unmodified. Just the stuff that is needed to test tuning.
// static was added to all functions except 'tune' to allow linking two copies of the tuning algorithm.
// These were made extern for the second coopy

extern int SWR;
extern char ind, cap, SW;

// Wild a$$ guesses, in order of most likely to least likely to hit
#define N_WAG 71
char wagSw[N_WAG] = {1,0,1,0,1,1,1,0,1,0,0,1,1,0,1,1,1,1,0,1,0,0,0,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,0,1,1,0,1,1,1,0,0,1,1,0,1,1,1,1,1,1,1,0,0,1,1,0,0,1,1,1,1,1};
char wagL[N_WAG] = {44,47,9,61,5,37,11,25,54,17,3,9,56,12,3,99,15,12,24,25,10,26,27,22,7,124,8,64,2,15,26,43,64,81,16,5,14,17,78,10,30,8,6,32,3,13,19,125,9,11,10,16,3,4,6,28,41,72,95,118,48,72,12,55,1,26,3,11,16,32,46};
char wagC[N_WAG] = {35,44,3,7,2,2,120,3,6,13,1,1,1,120,3,11,7,1,8,13,2,4,19,1,1,1,1,26,5,1,3,7,14,2,5,1,2,15,4,1,1,10,1,8,3,1,1,9,31,15,7,1,2,1,2,3,22,13,10,8,2,4,2,5,5,8,1,1,4,1,2};


int testSwLC(char l, char c, char sw)
{
   ind = l;
   cap = c;
   SW = sw;
   Relay_set(ind, cap, SW);
   Delay_ms(5);
   get_SWR();
   return SWR;
}


void hillClimb(char l, char c, char dist)
{
   // try tuning to all 4 cardinal directions, but don't run off edge
   int swr[5] = {1000, 1000, 1000, 1000, 0};
   swr[4] = SWR;
   if (l-dist >= 0)
      swr[0] = testSwLC(l-dist, c, SW);
   if (l+dist < 128)
      swr[1] = testSwLC(l+dist, c, SW);
   if (c-dist >= 0)
      swr[2] = testSwLC(l, c-dist, SW);
   if (c+dist < 128)
      swr[3] = testSwLC(l, c+dist, SW);
   
   // find lowest in all 4 cardinal directions (and original)
   int lowest = 1001;
   int inx = 0;
   for (int i=0; i<5; i++)
      if (swr[i] < lowest) {
         lowest = swr[i];
         inx = i;
      }

   // setup  for lowest
   SWR = swr[inx];
   switch (inx) {
   case 0:  ind = l-dist;   cap = c;      break;
   case 1:  ind = l+dist;   cap = c;      break;
   case 2:  ind = l;        cap = c-dist; break;
   case 3:  ind = l;        cap = c+dist; break;
   case 4:  ind = l;        cap = c;      break;
   }
}


void tune(void) {
   char avoidSw = 2;
   char saveInd;
   char saveCap;
   int saveSWR = 1001;
   for (int i=0; i<N_WAG; i++)
      if (wagSw[i] != avoidSw && testSwLC(wagL[i], wagC[i], wagSw[i]) < 999) {
         // Found a hill, try hill climbing to the top
         for (int dist = 64; dist; dist /= 2)
            hillClimb(ind, cap, dist);
         if (SWR < 120 || avoidSw != 2) 
            break;
         // tried this cap switch side, limit search to other side
         if (avoidSw == 2) {
            avoidSw = SW;
            saveInd = ind;
            saveCap = cap;
            saveSWR = SWR;
         }
      }
   // need to tune back to best found
   if (saveSWR < SWR)
      testSwLC(saveInd, saveCap, avoidSw);
   else
      testSwLC(ind, cap, SW);
}