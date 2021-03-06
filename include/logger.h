#pragma once
#include <yaml-cpp/yaml.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <iomanip>
#include <memory>
#include <fstream>
#include <string>
#include <sstream>
#include <numeric>
#include "netcdfwrapper.h"
#include <string>
#include "state.h"
#include "superparticle.h"
#include "grid.h"
#include "thermodynamic.h"
#include "analize_sp.h"
#include "analize_state.h"
#include "time_stamp.h"
#include "member_iterator.h"

class Logger {
   public:
    virtual void initialize(const State& state, const double& dt){} 
    virtual void setAttr(const std::string& key, bool val){}
    virtual void setAttr(const std::string& key, int val){}
    virtual void setAttr(const std::string& key, double val){}
    virtual void setAttr(const std::string& key, const std::string& val){}
    virtual void log(const State& state,
                     const std::vector<Superparticle>& superparticles
                    ) = 0;
    virtual ~Logger(){}
};

class StdoutLogger : public Logger {
   public:
    inline void log(const State& state,
                    const std::vector<Superparticle>& superparticles
                    )  override {
        std::vector<double> qc_sum = calculate_qc_profile(superparticles, state.grid);
        std::vector<double> r_mean = calculate_mean_radius_profile(superparticles, state.grid);
        std::vector<double> r_max = calculate_maximal_radius_profile(superparticles, state.grid);
        std::vector<int> sp_count_nuc = count_nucleated(superparticles, state.grid);
        std::vector<double> S = supersaturation_profile(state);

        std::cout << std::endl;
        std::cout << "State at " << state.t << "\n";
        std::cout << "     layer         z         E         p         T       "
                     " qv         S        qc    r_mean     r_max     N_nuc\n";
        for (unsigned int i = 0; i < state.layers.size(); ++i) {
            std::cout << std::setprecision(3) << std::setw(10) << i;
            std::cout << std::setprecision(3) << std::setw(10) << state.grid.getlay(i);
            std::cout << std::setprecision(3) << std::setw(10) << state.layers[i].E;
            std::cout << std::setprecision(3) << std::setw(10) << state.layers[i].p;
            std::cout << std::setprecision(3) << std::setw(10) << state.layers[i].T;
            std::cout << std::setprecision(3) << std::setw(10) << state.layers[i].qv;
            std::cout << std::setprecision(3) << std::setw(10) << S[i];
            std::cout << std::setprecision(3) << std::setw(10) << qc_sum[i];
            std::cout << std::setprecision(3) << std::setw(10) << r_mean[i];
            std::cout << std::setprecision(3) << std::setw(10) << r_max[i];
            std::cout << std::setprecision(3) << std::setw(10) << sp_count_nuc[i];
            std::cout << "\n";
        }
        std::cout << std::endl;
    }
};

class NetCDFLogger: public Logger {
    public:
    NetCDFLogger(std::string folder_name, std::string file_name="dummy.nc"): folder(folder_name), file(file_name) {
        mkdir(folder.c_str(), S_IRWXU);
        std::string fullname = folder+file;

        fh = std::unique_ptr<netCDF::NcFile>(nullptr);
        for (int fcounter = 0; !fh; ++fcounter){
            try{
                std::ostringstream ss;
                ss << std::setfill('0') << std::setw(3) << fcounter;
                fh = std::make_unique<netCDF::NcFile>(fullname + '_' + ss.str() + ".nc", netCDF::NcFile::newFile);
            }
            catch(const netCDF::exceptions::NcException& e){
                std::cout << "renaming file" << std::endl;
            }
        }

    }
    virtual void initialize(const State& state, const double& dt){
        n_lay = state.grid.n_lay;

        this->setAttr("dz", state.grid.length);
        this->setAttr("dt", dt);
        this->setAttr("w_init", state.w_init);

        netCDF::NcDim layer_dim = fh->addDim("layer", n_lay);
        netCDF::NcVar layer_var = fh->addVar("layer", netCDF::ncDouble, layer_dim);
        auto layers = state.grid.getlays();
        layer_var.putVar({0}, {n_lay}, layers.data());

        time_dim = fh->addDim("time");
        time_var = fh->addVar("time", netCDF::ncDouble, time_dim);

        qr_var = fh->addVar("qr_ground", netCDF::ncDouble, time_dim);
        qc_var = fh->addVar("qc", netCDF::ncDouble, {time_dim, layer_dim});
        qv_var = fh->addVar("qv", netCDF::ncDouble, {time_dim, layer_dim});
        S_var = fh->addVar("S", netCDF::ncDouble, {time_dim, layer_dim});
        r_max_var = fh->addVar("r_max", netCDF::ncDouble, {time_dim, layer_dim});
        r_mean_var = fh->addVar("r_mean", netCDF::ncDouble, {time_dim, layer_dim});
        ccn_count_var = fh->addVar("ccn_count", netCDF::ncDouble, {time_dim, layer_dim});
        ccn_count_falling_var = fh->addVar("ccn_count_falling", netCDF::ncDouble, {time_dim, layer_dim});
        r_std_var = fh->addVar("r_std", netCDF::ncDouble, {time_dim, layer_dim});
        T_var = fh->addVar("T", netCDF::ncDouble, {time_dim, layer_dim});
        p_var = fh->addVar("p", netCDF::ncDouble, {layer_dim});

        std::vector<double> p(member_iterator(const_cast<State&>(state).layers.begin(), &Layer::p), 
                               member_iterator(const_cast<State&>(state).layers.end(), &Layer::p));
        p_var.putVar({0}, {n_lay}, p.data());
        i = 0;
    } 

    virtual void setAttr(const std::string& key, bool val){
        fh->putAtt(key, netCDF::ncByte, val);
    }
    virtual void setAttr(const std::string& key, int val){
        fh->putAtt(key, netCDF::ncInt, val);
    }
    virtual void setAttr(const std::string& key, double val){
        fh->putAtt(key, netCDF::ncDouble, val);
    }
    virtual void setAttr(const std::string& key, const std::string& val){
        fh->putAtt(key, val);
    }

    inline void log(const State& state,
                    const std::vector<Superparticle>& superparticles
                    ) override {


        auto qc = calculate_qc_profile(superparticles, state.grid);
        auto S = supersaturation_profile(state);
        auto r_max = calculate_maximal_radius_profile(superparticles, state.grid);
        auto r_mean = calculate_mean_radius_profile(superparticles, state.grid);
        auto ccn_count = count_nucleated_ccn(superparticles, state.grid);
        auto ccn_count_falling = count_falling_ccn(superparticles, state.grid);
        auto r_std = calculate_stddev_radius_profile(superparticles, state.grid);
        std::vector<double> qv(member_iterator(const_cast<State&>(state).layers.begin(), &Layer::qv), 
                               member_iterator(const_cast<State&>(state).layers.end(), &Layer::qv));
        std::vector<double> T(member_iterator(const_cast<State&>(state).layers.begin(), &Layer::T), 
                               member_iterator(const_cast<State&>(state).layers.end(), &Layer::T));

        time_var.putVar({i}, {1}, &state.t);
        qr_var.putVar({i}, {1}, &state.qr_ground);
        qc_var.putVar({i,0}, {1, n_lay}, qc.data());
        qv_var.putVar({i,0}, {1, n_lay}, qv.data());
        S_var.putVar({i,0}, {1, n_lay}, S.data());
        r_max_var.putVar({i,0}, {1, n_lay}, r_max.data());
        r_mean_var.putVar({i,0}, {1, n_lay}, r_mean.data());
        ccn_count_var.putVar({i,0}, {1, n_lay}, ccn_count.data());
        ccn_count_falling_var.putVar({i,0}, {1, n_lay}, ccn_count_falling.data());
        r_std_var.putVar({i,0}, {1, n_lay}, r_std.data());
        //T_var.putVar({i,0}, {1, n_lay}, T.data());

        fh->sync();
        std::cout << "log at [min]: " << state.t/60. << std::endl;
        double sum=0;
        for (const auto& q: qc){
            sum += q;
        }
        std::cout << "qc sum: " << sum << std::endl;
        std::cout << "sp size: " << superparticles.size() << std::endl;
        ++i;
    }

    ~NetCDFLogger(){
        //if(fh){fh->close();}
    }

    private:
    netCDF::NcDim time_dim;
    netCDF::NcVar time_var;
    netCDF::NcVar qr_var;
    netCDF::NcVar qc_var;
    netCDF::NcVar qv_var;
    netCDF::NcVar S_var;
    netCDF::NcVar r_max_var;
    netCDF::NcVar r_mean_var;
    netCDF::NcVar ccn_count_var;
    netCDF::NcVar ccn_count_falling_var;
    netCDF::NcVar r_std_var;
    netCDF::NcVar T_var;
    netCDF::NcVar p_var;
    size_t i;
    size_t n_lay;
    std::string folder;
    std::string file;
    std::unique_ptr<netCDF::NcFile> fh;
};

inline std::unique_ptr<Logger> createLogger(std::string logger, std::string file_name) {
    if(logger == "netcdf") {
        return std::make_unique<NetCDFLogger>(file_name);
    }
    else {return std::make_unique<StdoutLogger>();}
}

inline std::unique_ptr<Logger> createLogger(const YAML::Node& config){
    std::string logger = config["type"].as<std::string>();
    std::string file_name = config["file_name"].as<std::string>();
    std::string dir_name = config["dir_name"].as<std::string>();
    std::cout << file_name << std::endl;
    if (file_name == "time_stamp") { file_name = time_stamp(); std::cout << file_name << std::endl;}

    if(logger == "netcdf") {
        return std::make_unique<NetCDFLogger>(dir_name, file_name);
    }

    else {return std::make_unique<StdoutLogger>();}
}
