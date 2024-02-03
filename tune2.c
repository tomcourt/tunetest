// ATU-10 tuning algorithm, prototype Tom Court, AC0NY

#include "stdio.h"
#include "tune.h"

// this allows us to have two 'tune' functions, an new one here and another old one in tune.c
#define tune() tune2()

 
// Wild a$$ guesses, in order of most likely to least likely to hit
#define N_WAG 71
static const char wagSw[N_WAG] = {1,0,1,0,1,1,1,0,1,0,0,1,1,0,1,1,1,1,0,1,0,0,0,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,0,1,1,0,1,1,1,0,0,1,1,0,1,1,1,1,1,1,1,0,0,1,1,0,0,1,1,1,1,1};
static const char wagL[N_WAG] = {44,47,9,61,5,37,11,25,54,17,3,9,56,12,3,99,15,12,24,25,10,26,27,22,7,124,8,64,2,15,26,43,64,81,16,5,14,17,78,10,30,8,6,32,3,13,19,125,9,11,10,16,3,4,6,28,41,72,95,118,48,72,12,55,1,26,3,11,16,32,46};
static const char wagC[N_WAG] = {35,44,3,7,2,2,120,3,6,13,1,1,1,120,3,11,7,1,8,13,2,4,19,1,1,1,1,26,5,1,3,7,14,2,5,1,2,15,4,1,1,10,1,8,3,1,1,9,31,15,7,1,2,1,2,3,22,13,10,8,2,4,2,5,5,8,1,1,4,1,2};


static int testSwLC(char l, char c, char swCap)
{
   ind = l;
   cap = c;
   SW = swCap;
   Relay_set(ind, cap, SW);
   delay_ms(5);
   get_SWR();
   return SWR;
}


static int hillClimb(char l, char c, char dist, char dir)
{
   int swr[5] = {1000, 1000, 1000, 1000, 1000};
   // less dist or half the distance to the edge
   int l_minus = (l>=dist) ? l-dist : l/2;   
   int c_minus = (c>=dist) ? c-dist : c/2;
   int l_plus = l+dist;
   int c_plus = c+dist;

   // try tuning to all 4 cardinal directions, but don't run off edge, also don't backtrack
   swr[0] = SWR;
   if (dir != 2 && l != l_minus)
      swr[1] = testSwLC(l_minus, c, SW); 
   if (dir != 1 && l_plus < 128)
      swr[2] = testSwLC(l_plus, c, SW);
   if (dir != 4 && c != c_minus)
      swr[3] = testSwLC(l, c_minus, SW);  
   if (dir != 3 && c_plus < 128)
      swr[4] = testSwLC(l, c_plus, SW);
   
   // find lowest in all 4 cardinal directions (and original)
   int lowest = swr[0];
   dir = 0;
   for (int i=1; i<5; i++)
      if (swr[i] < lowest) {
         lowest = swr[i];
         dir = i;
      }

   // setup  for lowest found
   SWR = swr[dir];
   switch (dir) {
   case 0:  ind = l;        cap = c;         break;
   case 1:  ind = l_minus;  cap = c;         break;
   case 2:  ind = l_plus;   cap = c;         break;
   case 3:  ind = l;        cap = c_minus;   break;
   case 4:  ind = l;        cap = c_plus;    break;
   }
   // TODO - remove this from real code
   if (showTuning) printf("\n");
   return dir;
}


void tune() {
   char avoidSw = 2;
   char saveInd;
   char saveCap;
   int saveSWR = 1001;
   for (int i=0; i<N_WAG; i++)
      if (wagSw[i] != avoidSw && testSwLC(wagL[i], wagC[i], wagSw[i]) < 999) {
         // Found a hill, try hill climbing to the top
         int dist = 32;
         int direction = 0;
         do {
            direction = hillClimb(ind, cap, dist, direction);
            if (direction == 0)
               dist /= 2;
         } while (dist);
            
         if (SWR < 120 || avoidSw != 2) 
            break;

         // tried this cap switch side, limit search to other side
         avoidSw = SW;
         saveInd = ind;
         saveCap = cap;
         saveSWR = SWR;
      }
   // need to tune back to best found
   if (saveSWR < SWR)
      testSwLC(saveInd, saveCap, avoidSw);
   else
      testSwLC(ind, cap, SW);
}