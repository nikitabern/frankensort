#ifndef TGRIFFIN_H
#define TGRIFFIN_H

/** \addtogroup Detectors
 *  @{
 */

#include <utility>
#include <vector>
#include <cstdio>
#include <functional>
//#include <tuple>

#include "TBits.h"
#include "TVector3.h"

#include "Globals.h"
#include "TGriffinHit.h"
#include "TGRSIDetector.h"
#include "TGRSIRunInfo.h"
#include "TTransientBits.h"
#include "TSpline.h"

class TGriffin : public TGRSIDetector {
public:
   enum EGriffinBits {
      kIsLowGainAddbackSet    = 1<<0,
      kIsHighGainAddbackSet   = 1<<1,
      kIsLowGainCrossTalkSet  = 1<<2,
      kIsHighGainCrossTalkSet = 1<<3,
      kBit4                   = 1<<4,
      kBit5                   = 1<<5,
      kBit6                   = 1<<6,
      kBit7                   = 1<<7
   };
   enum EGainBits { kLowGain, kHighGain };

   TGriffin();
   TGriffin(const TGriffin&);
   ~TGriffin() override;

public:
   TGriffinHit* GetGriffinLowGainHit(const int& i);                                              //!<!
   TGriffinHit* GetGriffinHighGainHit(const int& i);                                             //!<!
   TGriffinHit* GetGriffinHit(const Int_t& i) { return GetGriffinHit(i, GetDefaultGainType()); } //!<!
   TGRSIDetectorHit* GetHit(const Int_t& idx = 0) override;
   Short_t GetLowGainMultiplicity() const { return fGriffinLowGainHits.size(); }
   Short_t GetHighGainMultiplicity() const { return fGriffinHighGainHits.size(); }
   Short_t   GetMultiplicity() const override { return GetMultiplicity(GetDefaultGainType()); }
   Short_t Size() const { return GetMultiplicity(); }

   static TVector3 GetPosition(int DetNbr, int CryNbr = 5, double dist = 110.0); //!<!
   static const char* GetColorFromNumber(Int_t number);
#ifndef __CINT__
   void AddFragment(const std::shared_ptr<const TFragment>&, TChannel*) override; //!<!
#endif
   void ClearTransients() override
   {
      fGriffinBits = 0;
      for(const auto& hit : fGriffinLowGainHits) {
         hit.ClearTransients();
      }
      for(const auto& hit : fGriffinHighGainHits) {
         hit.ClearTransients();
      }
   }
   void ResetFlags() const;

   TGriffin& operator=(const TGriffin&); //!<!

#if !defined(__CINT__) && !defined(__CLING__)
   void SetAddbackCriterion(std::function<bool(TGriffinHit&, TGriffinHit&)> criterion)
   {
      fAddbackCriterion = std::move(criterion);
   }
   std::function<bool(TGriffinHit&, TGriffinHit&)> GetAddbackCriterion() const { return fAddbackCriterion; }
#endif

   Int_t        GetAddbackLowGainMultiplicity();
   Int_t        GetAddbackHighGainMultiplicity();
   Int_t        GetAddbackMultiplicity() { return GetAddbackMultiplicity(GetDefaultGainType()); }
   TGriffinHit* GetAddbackLowGainHit(const int& i);
   TGriffinHit* GetAddbackHighGainHit(const int& i);
   TGriffinHit* GetAddbackHit(const int& i) { return GetAddbackHit(i, GetDefaultGainType()); }
   bool IsAddbackSet(const Int_t& gain_type) const;
   void     ResetLowGainAddback();                                 //!<!
   void     ResetHighGainAddback();                                //!<!
   void     ResetAddback() { ResetAddback(GetDefaultGainType()); } //!<!
   UShort_t GetNHighGainAddbackFrags(const size_t& idx);
   UShort_t GetNLowGainAddbackFrags(const size_t& idx);
   UShort_t GetNAddbackFrags(const size_t& idx) { return GetNAddbackFrags(idx, GetDefaultGainType()); }

private:
#if !defined(__CINT__) && !defined(__CLING__)
   static std::function<bool(TGriffinHit&, TGriffinHit&)> fAddbackCriterion;
#endif
   std::vector<TGriffinHit> fGriffinLowGainHits;  //  The set of crystal hits
   std::vector<TGriffinHit> fGriffinHighGainHits; //  The set of crystal hits

   // static bool fSetBGOHits;                //!<!  Flag that determines if BGOHits are being measured

   static bool fSetCoreWave; //!<!  Flag for Waveforms ON/OFF
   // static bool fSetBGOWave;                //!<!  Flag for BGO Waveforms ON/OFF

   long                            fCycleStart; //!<!  The start of the cycle
   mutable TTransientBits<UChar_t> fGriffinBits;  // Transient member flags

   mutable std::vector<TGriffinHit> fAddbackLowGainHits;  //!<! Used to create addback hits on the fly
   mutable std::vector<TGriffinHit> fAddbackHighGainHits; //!<! Used to create addback hits on the fly
   mutable std::vector<UShort_t> fAddbackLowGainFrags;  //!<! Number of crystals involved in creating in the addback hit
   mutable std::vector<UShort_t> fAddbackHighGainFrags; //!<! Number of crystals involved in creating in the addback hit

   static Int_t fDefaultGainType;

public:
   
   /////******  *********///////////
   /////******  *********///////////
   /////******  Added for the Cd Analysis! *********///////////
   void SortHits() { std::sort(fGriffinLowGainHits.begin(),fGriffinLowGainHits.end()); 
                     std::sort(fGriffinHighGainHits.begin(),fGriffinHighGainHits.end()); }
   void CleanHits(Short_t k1=649, Short_t k2=0, Short_t k3=0) {  
     std::vector<TGriffinHit>::iterator it;
     for(it=fGriffinLowGainHits.begin();it!=fGriffinLowGainHits.end(); ) {
       if(it->GetKValue()==k1 || it->GetKValue()==k2 || it->GetKValue()==k3) {
         it++;
       } else {
         fGriffinLowGainHits.erase(it);
       }
     }
     SortHits();

   }
   /////******  *********///////////
   /////******  *********///////////
   /////******  *********///////////
                    


   static bool SetCoreWave() { return fSetCoreWave; } //!<!
   // static bool SetBGOHits()       { return fSetBGOHits;   }  //!<!
   // static bool SetBGOWave()      { return fSetBGOWave;   } //!<!
   static void SetDefaultGainType(const Int_t& gain_type);
   static Int_t GetDefaultGainType() { return fDefaultGainType; }

private:
   static TVector3 gCloverPosition[17];                      //!<! Position of each HPGe Clover
   void            ClearStatus() const { fGriffinBits = 0; } //!<!
   void SetBitNumber(enum EGriffinBits bit, Bool_t set) const;
   Bool_t TestBitNumber(enum EGriffinBits bit) const { return fGriffinBits.TestBit(bit); }

   static std::map<int, TSpline*> fEnergyResiduals; //!<!

   // const std::tuple<std::vector<TGriffinHit> *, std::vector<TGriffinHit> *, std::vector<UShort_t>*> fLowGainTuple =
   // std::make_tuple(&fGriffinLowGainHits,&fAddbackLowGainHits,&fAddbackLowGainFrags);

   // Cross-Talk stuff
public:
   //		static const Double_t gStrongCT[2];   //!<!
   //		static const Double_t gWeakCT[2]; //!<!
   //		static const Double_t gCrossTalkPar[2][4][4]; //!<!
   static Double_t CTCorrectedEnergy(const TGriffinHit* const hit_to_correct, const TGriffinHit* const other_hit,
                                     Bool_t time_constraint = true);
   Bool_t IsCrossTalkSet(const Int_t& gain_type) const;
   void FixLowGainCrossTalk();
   void FixHighGainCrossTalk();

   static void LoadEnergyResidual(int chan, TSpline* residual);
   static Double_t GetEnergyNonlinearity(int chan, double energy);

   // This is where the general untouchable functions live.
   std::vector<TGriffinHit>* GetHitVector(const Int_t& gain_type);      //!<!
   std::vector<TGriffinHit>* GetAddbackVector(const Int_t& gain_type);  //!<!
   std::vector<UShort_t>* GetAddbackFragVector(const Int_t& gain_type); //!<!
   TGriffinHit* GetGriffinHit(const Int_t& i, const Int_t& gain_type);  //!<!
   Int_t GetMultiplicity(const Int_t& gain_type) const;
   TGriffinHit* GetAddbackHit(const int& i, const Int_t& gain_type);
   Int_t GetAddbackMultiplicity(const Int_t& gain_type);
   void SetAddback(const Int_t& gain_type, bool flag = true) const;
   void ResetAddback(const Int_t& gain_type); //!<!
   UShort_t GetNAddbackFrags(const size_t& idx, const Int_t& gain_type);
   void FixCrossTalk(const Int_t& gain_type);
   void SetCrossTalk(const Int_t& gain_type, bool flag = true) const;

public:
   void Copy(TObject&) const override;            //!<!
   void Clear(Option_t* opt = "all") override;    //!<!
   void Print(Option_t* opt = "") const override; //!<!

   /// \cond CLASSIMP
   ClassDefOverride(TGriffin, 5) // Griffin Physics structure
   /// \endcond
};
/*! @} */
#endif
