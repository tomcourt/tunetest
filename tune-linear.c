// ATU-10 tuning algorithm, prototype Tom Court, AC0NY
// Linear search, one axis at a time with MRU 0.2
// Implements something like this https://en.wikipedia.org/wiki/Coordinate_descent

#include "stdio.h"
#include "stdbool.h"
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


// Most Recently Used (MRU) SWRs, use stored SWR reading for most recent tune attempts during hill a hill climb
static char MRU_l[8];
static char MRU_c[8];
static int MRU_swr[8];
static char n_MRUs = 0;


// Add to start of the MRU list, will push older MRUs down by 1 stopping at length
static int add_to_MRU(char l, char c, int swr, char length)
{
   for (int i=length; i>0; i--) {
      MRU_l[i] = MRU_l[i-1];
      MRU_c[i] = MRU_c[i-1];
      MRU_swr[i] = MRU_swr[i-1];
   }
   MRU_l[0] = ind = l;
   MRU_c[0] = cap = c;
   MRU_swr[0] = SWR = swr;
   return swr;
}


// Get the SWR for a L,C point. Avoid re-tuning the same points by maintaining a most recently used list
static int SWRgetMRU(char l, char c)
{
   for (int i=0; i<n_MRUs; i++)
      if (MRU_l[i] == l && MRU_c[i] == c) {
         // found a matching entry, moving entry to front, adding will overwrite the older copy    
         return add_to_MRU(MRU_l[i], MRU_c[i], MRU_swr[i], i);
      }
   if (++n_MRUs > sizeof(MRU_l)-1)
      n_MRUs = sizeof(MRU_l)-1;
   return add_to_MRU(l, c, testSwLC(l, c, SW), n_MRUs);
}


static bool hillClimbAxis(char l, char c, char dist, bool axis)
{
   int swr[3] = {1000, 1000, 1000};
   char vary = (axis ? l : c);
   // less dist or half the distance to the edge
   int minus = (vary>=dist) ? vary-dist : vary/2;   
   int plus = vary + dist;

   // try tuning both directons, but don't run off edge
   swr[1] = SWR;
   if (vary != minus)
      swr[0] = (axis ? SWRgetMRU(minus, c) : SWRgetMRU(l, minus));
   if (plus < 128 && vary != plus)
      swr[2] = (axis ? SWRgetMRU(plus, c) : SWRgetMRU(l, plus));
  
   // find lowest of original point, a smaller and a larger point along axis of interest
   int lowest = swr[1];
   int isBest = true;
   if (axis)
      ind = vary;
   else
      cap = vary;

   // prefer moving towards lower values when equal
   if (swr[0] <= lowest) {
      lowest = swr[0];
      if (axis)
         ind = minus;
      else
         cap = minus;
      isBest = false;
   }
   if (swr[2] < lowest) {
      lowest = swr[2];
      if (axis)
         ind = plus;
      else
         cap = plus;
      isBest = false;
   }
   SWR = lowest;
   return isBest;
}


static void hillClimb() 
{
   // Found a hill, try hill climbing to the top
   // first do a course search (0) and then a fine search (1), this reduces hill climbing switching
   for (int i=0; i<2; i++) {
      char startL;
      char startC;
      do {
         startL = ind;
         startC = cap;
         // Optimize each axis
         for (int j=0; j<2; j++) {
            int dist = 32;
            do {
               if (hillClimbAxis(ind, cap, dist, (j==0))) {
                  // start with course moves only 
                   if (i == 0)
                     break;
                  dist /= 2;
               }
            } while (dist);
         }
         // keep going until moving no longer causes improvement
      } while (ind != startL || cap != startC);
   }
}


void tune() 
{
   char avoidSw = 2;    // start by not avoiding either cap setting
   char saveInd;
   char saveCap;
   int saveSWR = 1001;
   for (int i=0; i<N_WAG; i++)
      if (wagSw[i] != avoidSw && testSwLC(wagL[i], wagC[i], wagSw[i]) < 999) {
         SW = wagSw[i];
         n_MRUs = 0;    // clear the MRU (because it only covers one cap switch side)
         hillClimb();    // hill climb from the starting point provided
         if (SWR < 120 || avoidSw != 2) 
            break;

         // tried this cap switch side, limit search to other side
         avoidSw = SW;
         saveInd = ind;
         saveCap = cap;
         saveSWR = SWR;
      }
   // need to tune back to best found from either cap side
   if (saveSWR < SWR)
      testSwLC(saveInd, saveCap, avoidSw);
   else
      testSwLC(ind, cap, SW);
}