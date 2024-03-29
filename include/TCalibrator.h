#ifndef _TCALIBRATORS_H_
#define _TCALIBRATORS_H_

#include <map>

#include <TNamed.h>
#include <TGraphErrors.h>

class TH1;

class TChannel;
class TNucleus;



class TCalibrator : public TNamed { 
public:
  TCalibrator();
  ~TCalibrator();

  virtual void Copy(TObject& obj) const;
  virtual void Print(Option_t *opt = "") const;
  virtual void Clear(Option_t *opt = "");
  virtual void Draw(Option_t *option="");
        UInt_t Size() const { return fPeaks.size(); }

  bool Check() const;

  int GetFitOrder() const { return fit_order; }
  void SetFitOrder(int order) { fit_order = order; }

  TGraph& MakeCalibrationGraph(bool zerozero=false); //  double min_figure_of_merit = 0.001);
  TGraphErrors &MakeEffGraph(double secondsi=3600.,double bq=100000.,Option_t *opt="draw"); 
  TGraphErrors &MakeResidualGraph();
  bool          SaveEffGraph(std::string datafile="",std::string fitfile=""); 
  std::vector<double> Calibrate(double min_figure_of_merit = 0.001);

  int AddData(TH1* source_data, std::string source,
               double sigma=2.0,double threshold=0.05,bool rm_bg=false,double error=0.001);

  int AddData(TH1* source_data, std::string source, std::string gammalist,
               double sigma=2.0,double threshold=0.05,bool rm_bg=false,double error=0.001);


  int AddData(TH1* source_data, TNucleus* source,
               double sigma=2.0,double threshold=0.05,bool rm_bg=false,double error=0.001);

  void UpdateTChannel(TChannel* channel);
  void LogResults(TChannel* channel,const char *logfile="callog.txt");

  void Fit(int order=1,bool zerozero=false); 

  int    GetNPar() const;
  double GetParameter(int i=0) const;
  double GetEffParameter(int i=0) const;

  struct Peak {
    double centroid;
    double energy;
    double area;
    double intensity;
    double low;
    double high;
    double chisq;
    std::string nucleus;
  };

  void AddPeak(double cent,double eng,std::string nuc,double a=0.0,double inten=0.0,double low=-1,double high=-1, double chisq=-1);
  Peak GetPeak(UInt_t i) const { return fPeaks.at(i); }
 
  TH1 *ApplyCalibration(TH1 *source,int bins=8000,double range=4000,Option_t *opt="") const; 
  
  TGraph *FitGraph() { return &fit_graph; }
  TGraph *EffGraph() { return &eff_graph; }
  TGraph *ResGraph() { return &res_graph; }
  TF1 *LinFit() { return linfit; }
  TF1 *EffFit() { return efffit; }

  double GetChi2(Option_t *opt="") const;

  std::string PrintEfficency(const char *filenamei="");

#ifndef __CINT__
  //struct SingleFit {
  //  double max_error;
  //  std::string nucleus;
  //  std::map<double,double> data2source;
  //  TGraph graph;
  //};
#endif

  static TGraphErrors *SumEfficiencies(TGraphErrors *g1,TGraphErrors *g2); 
  static TGraphErrors *CombineEfficiencies(TGraphErrors *orignal, TGraphErrors *other, double scale=1.0);
  static TF1 *FitAndPrintEfficiencies(TGraphErrors *graph);
  static void SaveEffGraph(TGraphErrors *graph,std::string datafile="",std::string fitfile=""); 

private:
#ifndef __CINT__
  //std::map<std::string,SingleFit> all_fits;
  std::map<double,double> Match(std::vector<double>,std::vector<double>); 
#endif
  std::vector<Peak> fPeaks;

  TGraphErrors fit_graph;
  TGraphErrors eff_graph;
  TGraphErrors res_graph;
  TF1    *linfit;
  TF1    *efffit;

  int fit_order;
  int total_points;

  double eff_par[4];

  void ResetMap(std::map<double,double> &inmap);
  void PrintMap(std::map<double,double> &inmap);
  bool CheckMap(std::map<double,double> inmap);

  ClassDef(TCalibrator,1)
};

#endif
