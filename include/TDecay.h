// Author: Ryan Dunlop    09/15
#ifndef TDECAY_H
#define TDECAY_H

#include "TNamed.h"
#include "TMath.h"
#include "TFitResult.h"
#include "TFitResultPtr.h"
#include "TF1.h"
#include "TH1.h"
#include "TVirtualFitter.h"

#include <string>


class TVirtualDecay;
class TSingleDecay;
class TDecay;
class TDecayChain;

class TDecayFit : public TF1 {
  public:
   TDecayFit() : TF1() {}; 
   //TGRSIFit(const char *name,Double_t (*fcn)(Double_t *, Double_t *), Double_t xmin, Double_t xmax, Int_t npar) : TF1(name, fcn, xmin, xmax, npar){};
   TDecayFit(const char* name, const char* formula, Double_t xmin = 0, Double_t xmax = 1) : TF1(name,formula,xmin,xmax){ } 
   TDecayFit(const char* name, Double_t xmin, Double_t xmax, Int_t npar) : TF1(name,xmin,xmax,npar) { }
   //TDecayFit(const char* name, void* fcn, Double_t xmin, Double_t xmax, Int_t npar) : TF1(name, fcn,xmin,xmax,npar){}
   TDecayFit(const char* name, ROOT::Math::ParamFunctor f, Double_t xmin = 0, Double_t xmax = 1, Int_t npar = 0) : TF1(name,f,xmin,xmax,npar){}
   //TDecayFit(const char* name, void* ptr, Double_t xmin, Double_t xmax, Int_t npar, const char* className) : TF1(name,ptr, xmin, xmax, npar, className){ }
#ifndef __CINT__
   TDecayFit(const char *name, Double_t (*fcn)(Double_t *, Double_t *), Double_t xmin=0, Double_t xmax=1, Int_t npar=0) : TF1(name,fcn,xmin,xmax,npar){}
   TDecayFit(const char *name, Double_t (*fcn)(const Double_t *, const Double_t *), Double_t xmin=0, Double_t xmax=1, Int_t npar=0) : TF1(name,fcn,xmin,xmax,npar) {}
#endif
   //TDecayFit(const char *name, void *ptr, void *ptr2,Double_t xmin, Double_t xmax, Int_t npar, const char *className, const char *methodName = 0) : TF1(name,ptr,ptr2,xmin,xmax,npar,className,methodName){}

   template <class PtrObj, typename MemFn>
   TDecayFit(const char *name, const  PtrObj& p, MemFn memFn, Double_t xmin, Double_t xmax, Int_t npar, const char * className = 0, const char *methodName = 0) : TF1(name,p,memFn,xmin,xmax,npar,className,methodName) {}

   template <typename Func>
   TDecayFit(const char *name, Func f, Double_t xmin, Double_t xmax, Int_t npar, const char *className = 0  ) : TF1(name,f,xmin,xmax,npar,className){}
   virtual ~TDecayFit() {}

   void SetDecay(TVirtualDecay* decay);
   TVirtualDecay* GetDecay() const;
 //  void DrawComponents() const; // *MENU* 
   void DrawComponents() const; 

   virtual void Print(Option_t *opt = "") const;

  private:
   TVirtualDecay* fDecay;//!

   ClassDef(TDecayFit,1);  // Extends TF1 for nuclear decays
};

class TVirtualDecay : public TNamed {
  public: 
   TVirtualDecay() {}
   ~TVirtualDecay() {}

   virtual void DrawComponents(Option_t * opt = "",Bool_t color_flag = true);
   void Print(Option_t *opt ="") const = 0;

   ClassDef(TVirtualDecay,1) //Abstract Class for TDecayFit
};

class TSingleDecay : public TVirtualDecay {
   friend class TDecayChain;
   friend class TDecayFit;
   friend class TDecay;
 //  friend class TDecay;
  public:
   //TDecay(Double_t tlow, Double_t thigh);
   TSingleDecay(UInt_t generation, TSingleDecay* parent, Double_t tlow = 0,Double_t thigh = 10);
   TSingleDecay(TSingleDecay* parent = 0, Double_t tlow = 0, Double_t thigh = 10);
   virtual ~TSingleDecay();

  public:
   ///// TF1 Helpers ////
   UInt_t GetGeneration() const  { return fGeneration; }
   Double_t GetHalfLife() const  { return (fDecayFunc->GetParameter(1) > 0.0) ? std::log(2.0)/fDecayFunc->GetParameter(1) : 0.0; }
   Double_t GetDecayRate() const { return fDecayFunc->GetParameter(1); }
   Double_t GetIntensity() const { return fDecayFunc->GetParameter(0); }
   Double_t GetEfficiency() const { return fDetectionEfficiency; }
   Double_t GetHalfLifeError() const  { return GetDecayRate() ? GetHalfLife()*GetDecayRateError()/GetDecayRate() : 0.0;}
   Double_t GetDecayRateError() const { return fDecayFunc->GetParError(1); }
   Double_t GetIntensityError() const { return fDecayFunc->GetParError(0); }
   void SetHalfLife(const Double_t &halflife)  { fDecayFunc->SetParameter(1,std::log(2.0)/halflife); UpdateDecays(); }
   void SetDecayRate(const Double_t &decayrate){ fDecayFunc->SetParameter(1,decayrate); UpdateDecays(); }
   void SetIntensity(const Double_t &intens)   { fDecayFunc->SetParameter(0,intens); UpdateDecays(); }
   void SetEfficiency(const Double_t &eff)     { fDetectionEfficiency = eff; }
   void FixHalfLife(const Double_t &halflife)  { fDecayFunc->FixParameter(1,std::log(2)/halflife); UpdateDecays(); }
   void FixHalfLife()                          { fDecayFunc->FixParameter(1,GetHalfLife()); UpdateDecays();}
   void FixDecayRate(const Double_t &decayrate){ fDecayFunc->FixParameter(1,decayrate); UpdateDecays(); }
   void FixDecayRate()                         { fDecayFunc->FixParameter(0,GetDecayRate()); UpdateDecays();}
   void FixIntensity(const Double_t &intensity){ fDecayFunc->FixParameter(0,intensity); }
   void FixIntensity()                         { fDecayFunc->FixParameter(0,GetIntensity());}
   void SetHalfLifeLimits(const Double_t &low, const Double_t &high);
   void SetIntensityLimits(const Double_t &low, const Double_t &high);
   void SetDecayRateLimits(const Double_t &low, const Double_t &high);
   void GetHalfLifeLimits(Double_t &low, Double_t &high) const;
   void GetIntensityLimits(Double_t &low, Double_t &high) const;
   void GetDecayRateLimits(Double_t &low, Double_t &high) const;
   void ReleaseHalfLife()                      { fDecayFunc->ReleaseParameter(1);}
   void ReleaseDecayRate()                     { fDecayFunc->ReleaseParameter(1);}
   void ReleaseIntensity()                     { fDecayFunc->ReleaseParameter(0);}
   void Draw(Option_t *option = "");
   Double_t Eval(Double_t t);
   Double_t EvalPar(const Double_t* x, const Double_t* par=0);
   TFitResultPtr Fit(TH1* fithist,Option_t *opt ="");
   void Fix();
   void Release();
   void SetRange(Double_t tlow, Double_t thigh);
   void SetName(const char* name);
   void SetLineColor(Color_t color) { fTotalDecayFunc->SetLineColor(color); }
   Color_t GetLineColor() const { return fTotalDecayFunc->GetLineColor(); }
   void SetMinimum(Double_t min) {fTotalDecayFunc->SetMinimum(min); fDecayFunc->SetMinimum(min);}
   void SetMaximum(Double_t max) {fTotalDecayFunc->SetMaximum(max); fDecayFunc->SetMaximum(max);}

  private:
   void SetDecayRateError(Double_t err) { fDecayFunc->SetParError(1,err); }
   void SetIntensityError(Double_t err) { fDecayFunc->SetParError(0,err); }

   void UpdateDecays();

   void SetChainId(Int_t id) { fChainId = id; }

  public:
   void SetDaughterDecay(TSingleDecay *daughter) { fDaughter = daughter; }
   void SetParentDecay(TSingleDecay *parent) { fParent = parent; }
   void SetTotalDecayParameters();
   void SetDecayId(Int_t Id) {fUnId = Id; }
   Int_t GetDecayId() const { return fUnId; }
   Int_t GetChainId() const { return fChainId; }

   const TDecayFit* GetDecayFunc() const { return fDecayFunc; }
   const TDecayFit* GetTotalDecayFunc() { SetTotalDecayParameters(); return fTotalDecayFunc; }

   TSingleDecay* GetParentDecay();
   TSingleDecay* GetDaughterDecay();
   
   Double_t ActivityFunc(Double_t *dim, Double_t *par);

   void Print(Option_t *option = "") const;

  private:
   UInt_t fGeneration;     //Generation from the primary
   Double_t fDetectionEfficiency; //The probability that this decay can be detected
   TDecayFit *fDecayFunc;        //!Function describing decay
   TDecayFit *fTotalDecayFunc;   //!Function used to access other fits
   TSingleDecay *fParent;        //!Parent Decay
   TSingleDecay *fDaughter;      //!Daughter Decay
   TSingleDecay *fFirstParent;   //!FirstParent in the decay
   Int_t fUnId;
   static UInt_t fCounter;
   Int_t fChainId;

 //  static Double_t ExpDecay(Double_t *dim, Double_t par);

   ClassDef(TSingleDecay,1) //Class containing Single Decay information
};

class TDecayChain : public TVirtualDecay {
  public:
   TDecayChain();
   TDecayChain(UInt_t generations);
   virtual ~TDecayChain();

   TSingleDecay* GetDecay(UInt_t generation);
   Double_t Eval(Double_t t) const;
   void Draw(Option_t *option = "");
   Int_t Size() const { return fDecayChain.size(); } 

   void Print(Option_t *option = "") const;

   void SetChainParameters();
   void SetRange(Double_t xlow, Double_t xhigh);
   const TDecayFit* GetChainFunc() { SetChainParameters(); return fChainFunc; }
   void DrawComponents(Option_t *opt = "",Bool_t color_flag = true);
   TFitResultPtr Fit(TH1* fithist, Option_t *opt = "");
   Double_t EvalPar(const Double_t* x, const Double_t* par=0);

   Int_t GetChainId() const {return fChainId; }

  private:
   void AddToChain(TSingleDecay* decay);
   Double_t ChainActivityFunc(Double_t *dim, Double_t *par);
   static UInt_t fChainCounter;

  private:
   std::vector<TSingleDecay*> fDecayChain; //The Decays in the Decay Chain
   TDecayFit* fChainFunc;  //! Function describing the total chain activity
   Int_t fChainId;

   ClassDef(TDecayChain,1) //Class representing a decay chain
};

class TDecay : public TVirtualDecay {
  public:
   TDecay() {}
   TDecay(std::vector<TDecayChain*> chain);
   virtual ~TDecay();

   void AddChain(TDecayChain* chain){ fChainList.push_back(chain);}
   Double_t DecayFit(Double_t *dim, Double_t *par);
   TDecayChain* GetChain(UInt_t idx);

   void SetHalfLife(Int_t Id, Double_t halflife);
   void SetHalfLifeLimits(Int_t Id, Double_t low, Double_t high);
   void SetDecayRateLimits(Int_t Id, Double_t low, Double_t high);
   void FixHalfLife(Int_t Id,Double_t halflife) {SetHalfLife(Id,halflife); SetHalfLifeLimits(Id,halflife,halflife);}
   TFitResultPtr Fit(TH1* fithist, Option_t *opt = "");

   void Print(Option_t* opt = "") const;
   void PrintMap() const;
   const TF1* GetFitFunc() { return fFitFunc; }
   void SetBackground(Double_t background) { fFitFunc->SetParameter(0,background);}
   Double_t GetBackground() const {return fFitFunc->GetParameter(0); }
   Double_t GetBackgroundError() const {return fFitFunc->GetParError(0); }
   void SetRange(Double_t xlow, Double_t xhigh);
   void DrawComponents(Option_t *opt = "",Bool_t color_flag = true);
   void Draw(Option_t *opt = "");
   void DrawBackground(Option_t *opt = "");
   void FixBackground(const Double_t &background)  { fFitFunc->FixParameter(0,background); }
   void FixBackground()                         { fFitFunc->FixParameter(0,GetBackground());}
   void SetBackgroundLimits(const Double_t &low, const Double_t &high) { fFitFunc->SetParLimits(0,low,high); }
   void ReleaseBackground()                    { fFitFunc->ReleaseParameter(0);}

  private:
   void RemakeMap();
   void SetParameters();
   Double_t ComponentFunc(Double_t *dim, Double_t *par);

  private:
   std::vector<TDecayChain*> fChainList;
   TDecayFit* fFitFunc;//!
   std::map<Int_t, std::vector<TSingleDecay*>> fDecayMap;//!

   ClassDef(TDecay,1) //Contains all decay chains in a fit
};

#endif
