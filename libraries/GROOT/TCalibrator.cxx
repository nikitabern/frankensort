
#include <TCalibrator.h>

#include <cmath>
#include <cstdio>
#include <vector>
#include <map>
#include <algorithm>
#include <fstream>
#include <iostream>

#include <TH1.h>
#include <TSpectrum.h>
#include <TLinearFitter.h>
#include <TGraphErrors.h>
#include <TRandom.h>

#include <TChannel.h>
#include <TNucleus.h>
#include <TTransition.h>

#include <GRootFunctions.h>
#include <GRootCommands.h>
#include <GCanvas.h>
#include <GPeak.h>
#include <GGaus.h>
#include <Globals.h>

#include "combinations.h"

ClassImp(TCalibrator)

TCalibrator::TCalibrator() {
  linfit=0;
  efffit=0;
  Clear();
}

TCalibrator::~TCalibrator() {
  if(linfit) delete linfit;
  if(efffit) delete efffit;
}

void TCalibrator::Copy(TObject &obj) const { }

bool TCalibrator::Check() const {

  if(Size()<2)
    return false;

  for(auto it:fPeaks) {
    double caleng = it.centroid*GetParameter(1)+GetParameter(0);
    double pdiff  = std::abs(caleng-it.energy)/it.energy;
    if(pdiff>0.05) {
      return false;
    }
  }
  return true;
}

void TCalibrator::Print(Option_t *opt) const {
  int counter=0;
  TString sopt(opt);
  printf("\t%2senergy%10scent%10scalc%10sarea%7snuc%8sintensity\n","","","","","","");
  for(auto it:fPeaks) {
    double caleng = it.centroid*GetParameter(1)+GetParameter(0);
    //double pdiff  = (std::abs(caleng-it.energy)/it.energy)*100.;
    double pdiff  = std::abs(caleng-it.energy);
    //printf("%i:\t%7.02f%16.02f%8.2f%3s[%%%3.2f]%16.02f%8s%16.04f\n",
    //        counter++,it.energy,it.centroid,caleng,"",pdiff,
    //        it.area,it.nucleus.c_str(),it.intensity);
    //
    if(sopt.Contains("v")) {
      printf("%i:\t%7.02f%16.02f%8.2f%3s[Res: %3.2f-keV]%16.02f%8s%16.04f %8s%16.04f %8s%16.04f %8s%16.04f\n",
              counter++,it.energy,it.centroid,caleng,"",pdiff,
              it.area,it.nucleus.c_str(),it.intensity,"",it.low,"",it.high,"",it.chisq );

    } else {
      printf("%i:\t%7.02f%16.02f%8.2f%3s[Res: %3.2f-keV]%16.02f%8s%16.04f\n",
              counter++,it.energy,it.centroid,caleng,"",pdiff,
              it.area,it.nucleus.c_str(),it.intensity);
    }
  }
  if(sopt.Contains("fit") && linfit) 
    linfit->Print();
  printf("-------------------------------\n");

  //for(auto it:all_fits) {
    //printf("Calibration for %s\tmax error: %.04f\n",it.first.c_str(),it.second.max_error);
    //int counter = 0;
    //printf("\t\t energy\tchannel\n");
    //for(auto it2:it.second.data2source) {
    //  printf("\t%i\t%.1f\t%.1f\n",counter++,it2.second,it2.first);
    //}
    //printf("------------------\n");
  //}
  //if(linfit) linfit->Print();
}


std::string TCalibrator::PrintEfficency(const char *filename) {
  std::string toprint;
  std::string file = filename;
  int counter=1;
  toprint.append("line\teng\tcounts\tt1/2\tactivity\n");
  toprint.append("--------------------------------------\n");
  for(auto it:fPeaks) {
    toprint.append(Form("%i\t%.02f\t%.02f\t%i\t%.02f\n",
                         counter++,it.energy,it.area,100, (it.intensity/100)*1e5 ));
  }
  toprint.append("--------------------------------------\n");

  if(file.length()) {
    std::ofstream ofile;
    ofile.open(file.c_str());
    ofile << toprint ;
    ofile.close();
  }
  printf("%s\n",toprint.c_str());
  return toprint;
}

TGraphErrors &TCalibrator::MakeEffGraph(double seconds,double bq,Option_t *opt) {
  //this currently assumes it only has 152Eu data.
  //one day it may be smarter, but today is not that day.
  TString option(opt);
  TString fitopt;
  if(option.Contains("Q")) {
    fitopt.Append("Q");
    option.ReplaceAll("Q","");
  }
  std::vector<double> energy;
  std::vector<double> error_e;
  std::vector<double> observed;
  std::vector<double> error_o;
  for(auto it:fPeaks) {
    //TNucleus n(it.nucleus.c_str());
    energy.push_back(it.energy);
    error_e.push_back(0.0);
    //observed.push_back((it.area/seconds) / ((it.intensity/100)*bq) );
    observed.push_back(it.area / ((it.intensity/100)*bq*seconds)  );
    error_o.push_back( observed.back() * (sqrt(it.area)/it.area) + observed.back()*0.02);
  }

  eff_graph.Clear();
  eff_graph = TGraphErrors(fPeaks.size(),energy.data(),observed.data(),error_e.data(),error_o.data());

  if(efffit)
    efffit->Delete();
  static int counter=0;
  efffit = new TF1(Form("eff_fit_%i",counter++),GRootFunctions::GammaEff,0,4000,4);
  eff_graph.Fit(efffit,fitopt.Data());

  if(option.Contains("draw",TString::kIgnoreCase)) {
    TVirtualPad *current = gPad;
    new GCanvas;
    eff_graph.Draw("AP");
    if(current)
      current->cd();
  }
  for(unsigned int  i=0;i<energy.size();i++) {
    printf("[%.1f] Observed  = %.04f  | Calculated = %.04f  |  per diff = %.2f\n",
            energy.at(i),observed.at(i),efffit->Eval(energy.at(i)),
            (std::abs(observed.at(i)-efffit->Eval(energy.at(i)))/observed.at(i))*100.   );
  }
  return eff_graph;
}


bool TCalibrator::SaveEffGraph(std::string datafile,std::string fitfile) {
  if(!efffit)
    return false;

  if(datafile.length()==0)
    datafile = Form("%s_data.dat",this->GetName());
  if(fitfile.length()==0)
    fitfile = Form("%s_fit.dat",this->GetName());

  std::ofstream outfile;
  outfile.open(datafile.c_str());
  for(int i=0;i<eff_graph.GetN();i++) {
     double x,y;
     eff_graph.GetPoint(i,x,y);
     outfile << x << "\t" << y << "\t" << eff_graph.GetErrorY(i) << std::endl;
  }
  outfile << std::endl;
  outfile.close();

  outfile.open(fitfile.c_str());
  double xmin,xmax;
  efffit->GetRange(xmin,xmax);
  double range = xmax-xmin;
  while(xmin<xmax) {
    outfile << xmin << "\t" << efffit->Eval(xmin) << std::endl;
    xmin += range/10000.;
  }
  outfile << std::endl;
  outfile.close();
  return true;

}

void TCalibrator::SaveEffGraph(TGraphErrors *graph,std::string datafile,std::string fitfile) { 
  if(!graph->GetListOfFunctions()->GetSize())
    return;
  TF1 *fit = (TF1*)graph->GetListOfFunctions()->At(0);

  if(datafile.length()==0)
    datafile = Form("%s_data.dat",graph->GetName());
  if(fitfile.length()==0)
    fitfile = Form("%s_fit.dat",graph->GetName());

  std::ofstream outfile;
  outfile.open(datafile.c_str());
  for(int i=0;i<graph->GetN();i++) {
     double x,y;
     graph->GetPoint(i,x,y);
     outfile << x << "\t" << y << "\t" << graph->GetErrorY(i) << std::endl;
  }
  outfile << std::endl;
  outfile.close();

  outfile.open(fitfile.c_str());
  double xmin,xmax;
  fit->GetRange(xmin,xmax);
  double range = xmax-xmin;
  while(xmin<xmax) {
    outfile << xmin << "\t" << fit->Eval(xmin) << std::endl;
    xmin += range/10000.;
  }
  outfile << std::endl;
  outfile.close();
  return ;


}

TGraphErrors *TCalibrator::CombineEfficiencies(TGraphErrors *orignal, TGraphErrors *other, double scale) {  

  std::vector<double> xvals;
  std::vector<double> yvals;
  std::vector<double> xerr;
  std::vector<double> yerr;

  TGraphErrors *g1 = orignal; //(TGraphErrors*)cal1->EffGraph();
  TGraphErrors *g2 = other;   //(TGraphErrors*)cal2->EffGraph();
  for(int x1=0;x1<g1->GetN();x1++) {
    xvals.push_back((*(g1->GetX()+x1)));
    yvals.push_back((*(g1->GetY()+x1)));
    xerr.push_back((*(g1->GetEX()+x1)));
    yerr.push_back((*(g1->GetEY()+x1)));
  }  
  for(int x2=0;x2<g2->GetN();x2++) {
    xvals.push_back((*(g2->GetX()+x2)));
    yvals.push_back((*(g2->GetY()+x2))*scale);
    xerr.push_back((*(g2->GetEX()+x2))*scale);
    yerr.push_back((*(g2->GetEY()+x2))*scale);
  
  }
  
  TGraphErrors *combined = new TGraphErrors(xvals.size(),xvals.data(),yvals.data(),xerr.data(),yerr.data()); 
  combined->SetNameTitle("combined",Form("%s + %s",g1->GetTitle(),g2->GetTitle()));
  return combined;

}

TF1 *TCalibrator::FitAndPrintEfficiencies(TGraphErrors *graph) { 

  static int counter=0;
  TF1 *fit = new TF1(Form("eff_fit_%i",counter++),GRootFunctions::GammaEff,0,4000,4);

  new GCanvas;
  graph->Fit(fit,""); //fitopt.Data());
  graph->Draw("AP");
  
  printf("results for %s\n",graph->GetName());
  for(int  i=0;i<graph->GetN();i++) {
    printf("%i)  [%.1f] Observed  = %.04f  | Calculated = %.04f  |  per diff = %.2f\n",
            i,*(graph->GetX()+i),*(graph->GetY()+i),fit->Eval(*(graph->GetX()+i)), 
            std::abs(*(graph->GetY()+i)-fit->Eval(*(graph->GetX()+i)))*100.); 
  }
  return fit;
}




void TCalibrator::Clear(Option_t *opt) {
  fit_graph.Clear(opt);
  eff_graph.Clear(opt);
  res_graph.Clear(opt);
  //all_fits.clear();

  for(int i=0;i<4;i++) eff_par[i]=0.;

  fPeaks.clear();

  total_points=0;
}

void TCalibrator::Draw(Option_t *opt) {
  //if((graph_of_everything.GetN()<1) &&
  //    (all_fits.size()>0))
  //MakeCalibrationGraph();
  //Fit();
  TString option(opt);
  if(option.Contains("new",TString::kIgnoreCase))
    new GCanvas;
  fit_graph.Draw("AP");
}

void TCalibrator::Fit(int order,bool zerozero) {

  //if((graph_of_everything.GetN()<1) &&
  //    (all_fits.size()>0))
  MakeCalibrationGraph(zerozero);
  if(fit_graph.GetN()<1)
    return;
  if(order==1) {
    linfit = new TF1("linfit",GRootFunctions::LinFit,0,1,2);
    linfit->SetParameter(0,0.0);
    linfit->SetParameter(1,1.0);
    linfit->SetParName(0,"intercept");
    linfit->SetParName(1,"slope");
  } else if(order==2) {
    linfit = new TF1("linfit",GRootFunctions::QuadFit,0,1,3);
    linfit->SetParameter(0,0.0);
    linfit->SetParameter(1,1.0);
    linfit->SetParameter(2,0.0);
    linfit->SetParName(0,"A");
    linfit->SetParName(1,"B");
    linfit->SetParName(2,"C");
  } else if(order==3) {
    linfit = new TF1("linfit",GRootFunctions::TriFit,0,1,4);
    linfit->SetParameter(0,0.0);
    linfit->SetParameter(1,1.0);
    linfit->SetParameter(2,0.0);
    linfit->SetParameter(3,0.0);
    linfit->SetParName(0,"A");
    linfit->SetParName(1,"B");
    linfit->SetParName(2,"C");
    linfit->SetParName(3,"D");
  } else if(order==4) {
    linfit = new TF1("linfit",GRootFunctions::NonLinearFit,0,1,5);
    linfit->SetParameter(0,0.0);
    linfit->SetParameter(1,1.0);
    linfit->SetParameter(2,0.0);
    linfit->SetParameter(3,0.0);
    linfit->SetParameter(4,0.0);
    linfit->SetParName(0,"A");
    linfit->SetParName(1,"B");
    linfit->SetParName(2,"C");
    linfit->SetParName(3,"D");
    linfit->SetParName(4,"X");


  }
  
  fit_graph.Fit(linfit);
  fit_graph.Print();

  Print();

}

TGraphErrors &TCalibrator::MakeResidualGraph() {
  if(!linfit) {
    printf("Please fit your data first.\n");
    return res_graph;
  }
  std::vector<double> res_vec;
  std::vector<double> res_val;
  std::vector<double> res_err;
  std::vector<double> x_err;
  for(int i=0;i<fit_graph.GetN();i++) {
    double true_val = *(fit_graph.GetY()+i);
    double cal_val  = linfit->Eval(*(fit_graph.GetX()+i)); 
    res_vec.push_back(true_val);
    res_val.push_back(true_val-cal_val);
    res_err.push_back(fabs(res_val.back())*0.10);
    x_err.push_back(0);
  }
  res_graph = TGraphErrors(res_vec.size(),res_vec.data(),res_val.data(),x_err.data(),res_err.data());
  
  return res_graph;
}


double TCalibrator::GetChi2(Option_t *opt) const {
  if(!linfit)
    return sqrt(-1);
  return linfit->GetChisquare()/linfit->GetNDF();

}

double TCalibrator::GetParameter(int i) const {
  if(linfit)
    return linfit->GetParameter(i);
  return sqrt(-1);
}

int TCalibrator::GetNPar() const {
  if(linfit) 
    return linfit->GetNpar();
  return -1;
}

double TCalibrator::GetEffParameter(int i) const {
  if(efffit)
    return efffit->GetParameter(i);
  return sqrt(-1);
}


TGraph &TCalibrator::MakeCalibrationGraph(bool zerozero) { //double min_fom) {
  std::vector<double> xvalues;
  std::vector<double> yvalues;
  std::vector<double> xerrors;
  std::vector<double> yerrors;
  if(zerozero) {
    xvalues.push_back(0.0);
    yvalues.push_back(0.0);
    xerrors.push_back(1.0);
    yerrors.push_back(0.0);
  }

  for(auto it:fPeaks) {
    xvalues.push_back(it.centroid);
    yvalues.push_back(it.energy);
    xerrors.push_back( sqrt(TMath::Power(sqrt(it.area)/it.area,2) + TMath::Power(0.01,2)) * xvalues.back());
    yerrors.push_back(0.0);
  }
  fit_graph.Clear();
  fit_graph = TGraphErrors(xvalues.size(),xvalues.data(),yvalues.data(),xerrors.data(),yerrors.data());

  return fit_graph;
}

std::vector<double> TCalibrator::Calibrate(double min_fom) { std::vector<double> vec; return vec; }


int TCalibrator::AddData(TH1 *data,std::string source, double sigma,double threshold,bool rm_bg,double error) {
  if(!data || !source.length()) {
    printf("data not added. data = %p \t source = %s\n",(void*)data,source.c_str());
    return 0;
  }
  TNucleus n(source.c_str());
  return AddData(data,&n,sigma,threshold,rm_bg,error);
}

int TCalibrator::AddData(TH1 *data,std::string source, std::string sou_file, double sigma,double threshold,bool rm_bg,double error) {
  if(!data || !source.length()) {
    printf("data not added. data = %p \t source = %s\n",(void*)data,source.c_str());
    return 0;
  }
  TNucleus n(source.c_str(),sou_file.c_str());
  return AddData(data,&n,sigma,threshold,rm_bg,error);
}


int TCalibrator::AddData(TH1 *data,TNucleus *source, double sigma,double threshold,bool rm_bg,double error) {
  if(!data || !source) {
    printf("data not added. data = %p \t source = %p\n",(void*)data,(void*)source);
    return 0;
  }
  int actual_x_max = std::floor(data->GetXaxis()->GetXmax());
  int actual_x_min = std::floor(data->GetXaxis()->GetXmax());
  int displayed_x_max = std::floor(data->GetXaxis()->GetBinUpEdge(data->GetXaxis()->GetLast()));
  int displayed_x_min = std::floor(data->GetXaxis()->GetBinLowEdge(data->GetXaxis()->GetFirst()));

  std::string name;
  if((actual_x_max==displayed_x_max) && (actual_x_min==displayed_x_min))
    name = source->GetName();
  else
    name = Form("%s_%i_%i",source->GetName(),displayed_x_min,displayed_x_max);

  TNamed::SetName(Form("%s%s",GetName(),name.c_str()));


  TIter iter(source->GetTransitionList());
  std::vector<double> source_energy;
  std::map<double,double> src_eng_int;
  while(TTransition *transition = (TTransition*)iter.Next()) {
    source_energy.push_back(transition->GetEnergy());
    src_eng_int[transition->GetEnergy()] = transition->GetIntensity();
  }
  std::sort(source_energy.begin(),source_energy.end());

  TSpectrum spectrum;
  if(rm_bg) {
    //data    = (TH1*)data->Clone(Form("%s_clone",data->GetName()));
     TH1 *bg = spectrum.Background(data,4,"Compton");
     std::cout << "subtracting bg!" << std::endl ;
     std::cout<< "data: " << data->Integral() << std::endl;
     std::cout<< "bg: "   << bg->Integral() << std::endl;
     
     data->Add(bg,-1);
     std::cout<< "data: " << data->Integral() << std::endl;
  }

  spectrum.Search(data,sigma,"",threshold);
  while(spectrum.GetNPeaks()>10) {
    spectrum.Clear();
    threshold+=.01;
    spectrum.Search(data,sigma,"",threshold);
  }
  //std::vector<double> data_channels;
  std::vector<double> peak_positions;
  //std::map<double,double> peak_area;;
  //std::vector<double> data;
  //printf("found %i peaks\n",spectrum.GetNPeaks()); fflush(stdout);
  for(int x=0;x<spectrum.GetNPeaks();x++) {
    //double range = 8*data->GetXaxis()->GetBinWidth(1);
    //printf(DGREEN "\tlow %.02f \t high %.02f" RESET_COLOR "\n",spectrum.GetPositionX()[x]-range,spectrum.GetPositionX()[x]+range);

    //GPeak *fit = PhotoPeakFit(data,spectrum.GetPositionX()[x]-range,spectrum.GetPositionX()[x]+range,"no-print");
    //data_channels.push_back(fit->GetCentroid());
    //data->GetListOfFunctions()->Remove(fit);
    //peak_area[fit->GetCentroid()] = fit->GetSum();
    //printf("%i peak at %.02f\n",x,spectrum.GetPositionX()[x]); fflush(stdout); 
    peak_positions.push_back(spectrum.GetPositionX()[x]);


  }
  //printf("i am here!\n"); fflush(stdout);

  //std::map<double,double> datatosource = Match(data_channels,source_energy);;
  std::map<double,double> datatosource = Match(peak_positions,source_energy);;

  //PrintMap(datatosource);
  double range = 8;//8*data->GetXaxis()->GetBinWidth(1);
  for(auto it : datatosource) {

    double peak = it.first;
    double eng  = it.second;

    range = 0.01 * peak;
    if(range < 5.)
      range = 5.0;

    data->GetXaxis()->SetRangeUser(peak-(2*range),peak+range);
    //data->GetListOfFunctions()->Clear();
    GPeak *fit = PhotoPeakFit(data,peak-range,peak+range/2.,"no-print");
    //GGaus *fit = GausFit(data,peak-range,peak+range/2.,"no-print");
    double sum=0.0;
    //sum = fit->GetSum();
    sum = fit->GetArea();
    fit->Print();
    
    AddPeak(fit->GetCentroid(),eng,source->GetName(),sum,src_eng_int[eng],peak-range,peak+range/2.,fit->GetChisquare());
    //AddPeak(peak,eng,source->GetName(),fit->GetSum(),src_eng_int[eng]);
  }

  //Print();
  int counter =0;
  for(auto it : datatosource) {
    if(!std::isnan(it.second)) counter++;
  }
  return counter; //CheckMap(datatosource);

}

void TCalibrator::ResetMap(std::map<double,double> &inmap) {
  for(auto &it:inmap) {
    it.second = sqrt(-1);
  }
}

void TCalibrator::PrintMap(std::map<double,double> &inmap) {
  printf("\tfirst\tsecond\n");
  int counter=0;
  for(auto &it:inmap) {
    printf("%i\t%.01f\t%.01f\n",counter++,it.first,it.second);
  }


}

std::map<double,double> TCalibrator::Match(std::vector<double> peaks,std::vector<double> source) {
  std::map<double,double> map;
  std::sort(peaks.begin(),peaks.end());
  std::sort(source.begin(),source.end());

  // Peaks are the fitted points.
  // source are the known values

  std::vector<bool> filled(source.size());
  std::fill(filled.begin(), filled.begin() + peaks.size(), true);

  //std::cout << "Num peaks: " << peaks.size() << std::endl;
  //std::cout << "Num source: " << source.size() << std::endl;

  TLinearFitter fitter(1, "1 ++ x");

  for(size_t num_data_points = peaks.size(); num_data_points > 0; num_data_points--) {
    double best_chi2 = DBL_MAX;
    for(auto peak_values : combinations(peaks, num_data_points)) {
      // Add a (0,0) point to the calibration.
      peak_values.push_back(0);
      for(auto source_values : combinations(source, num_data_points)) {
	source_values.push_back(0);

        if(peaks.size()>3) {
          double max_err = 0.02;
          double pratio = peak_values.front()/peak_values.at(peak_values.size()-2);
          double sratio = source_values.front()/source_values.at(source_values.size()-2);
          //std::cout << "ratio: " << pratio << " - " << sratio << " = " << std::abs(pratio-sratio) << std::endl;
          if(std::abs(pratio-sratio)>max_err) {
            //std::cout << "skipping" << std::endl;
            continue;
          }
        }

        fitter.ClearPoints();
	fitter.AssignData(source_values.size(), 1, peak_values.data(), source_values.data());
	fitter.Eval();

	if(fitter.GetChisquare() < best_chi2) {
	  map.clear();
	  for(size_t i = 0; i<num_data_points; i++) {
	    map[peak_values[i]] = source_values[i];
	  }
	  best_chi2 = fitter.GetChisquare();
	}

      }
    }

    // Remove one peak value from the best fit, make sure that we reproduce (0,0) intercept.
    if(map.size() > 2) {
      std::vector<double> peak_values;
      std::vector<double> source_values;
      for(auto& item : map) {
	peak_values.push_back(item.first);
	source_values.push_back(item.second);
      }

      for(size_t skipped_point = 0; skipped_point<source_values.size(); skipped_point++) {
	std::swap(peak_values[skipped_point], peak_values.back());
	std::swap(source_values[skipped_point], source_values.back());

	fitter.ClearPoints();
	fitter.AssignData(source_values.size()-1, 1, peak_values.data(), source_values.data());
	fitter.Eval();

	if(std::abs(fitter.GetParameter(0)) > 10) {
	  map.clear();
	  break;
	}

	std::swap(peak_values[skipped_point], peak_values.back());
	std::swap(source_values[skipped_point], source_values.back());
      }
    }

    if(map.size()) {
      return map;
    }
  }

  map.clear();
  return map;














  // //for(auto s : source)
  // //  printf(" s = %.02f\n",s);


  // //unsigned int ssize = source.size();
  // double max_err = 0.01;
  // std::vector<double> pused;
  // for(unsigned int i=0;i<peaks.size();i++)  { pused.push_back(100.); }
  // std::vector<double> sused;
  // for(unsigned int i=0;i<source.size();i++)  { sused.push_back(100.); }

  // struct point {
  //   double x1;
  //   double x2;
  //   double y1;
  //   double y2;
  //   double slope() const {return (y2-y1)/(x2-x1);}
  //   void   print() const { printf("(%.2f - %.2f)/(%.2f - %.2f) = %.4f\n",y2,y1,x2,x1,slope()); }
  //   bool operator<(const point& other) const { return slope()<other.slope(); }
  // };

  // std::vector<point> points;

  // printf("-------------------------------\n");
  // for(unsigned int s1=0;s1<source.size();s1++) {
  //   for(unsigned int s2=s1+1;s2<source.size();s2++) {
  //     double sratio = source.at(s1)/source.at(s2);
  //     for(unsigned int p1=0;p1<peaks.size();p1++) {
  //       for(unsigned int p2=p1+1;p2<peaks.size();p2++) {

  //         double pratio = peaks.at(p1)/peaks.at(p2);
  //         double diff = fabs(pratio *1/(fabs(peaks.at(p1)-peaks.at(p2)))
  //                           -sratio*1/(fabs(source.at(s1)-source.at(s2)))) ;
  //           if(diff<sused.at(s1) && diff<pused.at(p1) ) {
  //             printf("1]  %.02f/%.02f - %.02f/%.02f  = %f  | old = %f \n" ,
  //                    peaks.at(p1),peaks.at(p2),source.at(s1),source.at(s2),
  //                    diff,pused.at(p1));
  //             map[peaks.at(p1)] = source.at(s1);
  //             pused[p1] = diff;
  //             sused[s1] = diff;
  //           }
  //           if(diff<sused.at(s2) && diff<pused.at(p2) ) {
  //             printf("2]  %.02f/%.02f - %.02f/%.02f  = %f  | old = %f \n" ,
  //                    peaks.at(p1),peaks.at(p2),source.at(s1),source.at(s2),
  //                    diff,pused.at(p2));
  //             map[peaks.at(p2)] = source.at(s2);
  //             pused[p2] = diff;
  //             sused[s2] = diff;
  //           }
  //       }
  //     }
  //   }
  // }
  // printf("-------------------------------\n");

  // PrintMap(map);


  // max_err = 0.05;

  // for(std::map<double,double>::iterator mit1=map.begin();mit1!=map.end();mit1++) {
  //   for(std::map<double,double>::iterator mit2=mit1;mit2!=map.end();mit2++) {
  //     if(mit1==mit2) continue;
  //     point pt;
  //     if(mit2->second>mit1->second &&
  //        mit2->first>mit1->first) {
  //       pt.x1 = mit1->first;
  //       pt.y1 = mit1->second;
  //       pt.x2 = mit2->first;
  //       pt.y2 = mit2->second;
  //       points.push_back(pt);
  //     }
  //   }
  // }

  // //printf("points.size() = %i\n",points.size());
  // std::sort(points.begin(),points.end());

  // auto is_nearby = [&](const point& a, const point& b) {
  //   return std::abs(a.slope() - b.slope())/a.slope() < max_err;
  // };

  // auto count_nearby = [&](const point& p) {
  //   return std::count_if(points.begin(), points.end(),
  //       [&](const point& p_other) { return is_nearby(p,p_other); }
  //       );
  // };

  // point best_slope = *std::max_element(points.begin(), points.end(),
  //     [&](const point& a, const point& b) {
  //       return count_nearby(a) < count_nearby(b);
  //     }
  //   );

  // //for(auto p : points) {
  // //  p.print();
  // //  std::cout << count_nearby(p) << std::endl;
  // //}

  // //std::cout << "-----------------" << std::endl;
  // //best_slope.print();
  // //double bs = best_slope.slope();
  // //std::cout << count_nearby(best_slope) << std::endl;
  // //std::cout << "-----------------" << std::endl;

  // for(auto it = points.begin(); it!=points.end();) {
  //   if(is_nearby(*it, best_slope)) {
  //     it++;
  //   } else {
  //     it = points.erase(it);
  //   }
  // }
  // map.clear();
  // for(auto it = points.begin(); it!=points.end();it++) {
  //   map[it->x1] = it->y1;
  //   map[it->x2] = it->y2;
  // }

  // //for(auto p : peaks) {
  // //  printf("%.02f -> %.02f\n",p,bs*p);
  // //}

  // //PrintMap(map);
  // return map;
}

bool TCalibrator::CheckMap(std::map<double,double> inmap) {
  for(auto it:inmap) {
    if(std::isnan(it.second))
      return false;
  }
  return true;
}



void TCalibrator::UpdateTChannel(TChannel *channel) { }


void TCalibrator::LogResults(TChannel *channel,const char *logfile) { 
  
  FILE *saved = stdout;
  stdout = fopen(logfile,"a");

  printf("====================================\n");
  printf("====================================\n");

  

  printf("%s",channel->PrintToString().c_str());
  Print("fit");
  printf("====================================\n");
  printf("====================================\n");
  
  fclose(stdout);
  stdout=saved;

  return;
}



void TCalibrator::AddPeak(double cent,double eng,std::string nuc,double a,double inten,double low, double high, double chisq) {
  Peak p;
  p.centroid = cent;
  p.energy   = eng;
  p.nucleus  = nuc;
  p.area     = a;
  p.intensity = inten;
  p.low  = low;
  p.high = high;
  p.chisq = chisq;

  fPeaks.push_back(p);
  return;
}




TH1 *TCalibrator::ApplyCalibration(TH1 *source,int bins,double range,Option_t *opt) const  { 
  if(fit_graph.GetN()<1)
    return 0;
  double offset = GetParameter(0);
  double gain   = GetParameter(1);
  std::string hname = Form("%s_cal",source->GetName());
  TH1D *hist = new TH1D(hname.c_str(),hname.c_str(),bins,0,range);
  for(int x=1;x<=source->GetNbinsX();x++) {
    double value  = source->GetXaxis()->GetBinLowEdge(x);
    double weight = source->GetBinContent(x);
    double j=0.0;
    while(j<=weight) {
      value = (value+gRandom->Uniform())*gain + offset;
      hist->Fill(value);
      //printf("bin[%i]:  %.0f\t\t%.02f\n",x,j,value);
      j++;
    }
  }
  return hist;
}



TGraphErrors *TCalibrator::SumEfficiencies(TGraphErrors *g1,TGraphErrors *g2) {

  std::vector<double> xvals;
  std::vector<double> yvals;
  std::vector<double> xerr;
  std::vector<double> yerr;

  //TGraphErrors *g1 = (TGraphErrors*)cal1->EffGraph();
  //TGraphErrors *g2 = (TGraphErrors*)cal2->EffGraph();
  for(int x1=0;x1<g1->GetN();x1++) {
    for(int x2=0;x2<g2->GetN();x2++) {
      if( (*(g1->GetX()+x1)) == (*(g2->GetX()+x2)) ) {
        xvals.push_back((*(g1->GetX()+x1)));
        yvals.push_back((*(g1->GetY()+x1))+(*(g2->GetY()+x2)));
        xerr.push_back((*(g1->GetEX()+x1))+(*(g2->GetEX()+x2)));
        yerr.push_back((*(g1->GetEY()+x1))+(*(g2->GetEY()+x2)));
        break;
      }
    }
  }
  
  TGraphErrors *sum = new TGraphErrors(xvals.size(),xvals.data(),yvals.data(),xerr.data(),yerr.data()); 

  return sum;
}












