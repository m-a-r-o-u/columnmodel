#include "thermodynamic.h"
#include <vector>
#include "layer_quantities.h"
#include "level_quantities.h"

double super_saturation(double T, double p, double qv) {
    return qv / saturation_vapor(T, p) - 1;
}

double saturation_vapor(double T, double p) {
    auto es = saturation_pressure(T);
    return R_G / R_V * es / (p - es);
}

bool will_nucleate(double r_dry, double S, double T) {
    auto S_crit = critical_saturation(r_dry, T);
    return S > S_crit;
}

double kelvins_parameter(double T) { return 2 * GAMMA / R_V / RHO_H2O / T; }

double raoults_parameter(double r_dry) {
    return 2 * r_dry*r_dry*r_dry * RHO_S * M_MOL_H2O / RHO_H2O / M_MOL_S;
}

double critical_saturation(double r_dry, double T) {
    double kp = kelvins_parameter(T);
    return std::sqrt(4. * kp*kp*kp / 27. / raoults_parameter(r_dry));
}

Tendencies condensation(double qc, double N, const double r_dry, double S, double T,
                        double E, double dt) {
    Tendencies tendencies{0, 0};

    const double r_old = radius(qc, N, r_dry);
    const double es = saturation_pressure(T);
    const double r_new = condensation_solver(r_old, es, T, S, E, dt);

    if (r_new < r_dry){
        tendencies.dqc = -qc;
    }else{
        double dqc = cloud_water(N, r_new, r_old);
        tendencies.dqc = dqc;
    }
    double dT = H_LAT / C_P * tendencies.dqc;
    tendencies.dT = dT;
    return tendencies;
}

double _radius(double qc, double N, double rho) {
    return std::pow(3. / 4. / PI * qc * rho / RHO_H2O / N, 1. / 3.);
}

double radius(double qc, double N, double r_min, double rho) {
    return _radius(qc + cloud_water(N, r_min, 0., rho), N, rho);
}

double _cloud_water(double N, double r, double rho) {
    return 4. / 3. * PI * r*r*r * RHO_H2O / rho * N;
}

double cloud_water(double N, double r, double r_min, double rho) {
    return _cloud_water(N, r, rho) - _cloud_water(N, r_min, rho);
}

double saturation_pressure(const double T) {
    return ES0 * std::exp(17.62 * (T - T0) / (243.12 + (T - T0)));
}

double condensation_solver(const double r_old, const double es, const double T,
                           const double S, const double E, const double dt) {
    return r_old + dt * diffusional_growth(r_old, es, T, S, E, dt);
}

double diffusional_growth(const double r_old, const double es, const double T,
                          const double S, const double E, const double dt) {
    auto c1 =
        H_LAT * H_LAT / (R_V * K * T*T) + R_V * T / (D * es);
    auto c2 = H_LAT / (R_V * K * T*T);
    return (S / r_old + c2 * E) / (c1 * RHO_H2O);
}

double fall_speed(const double r) {
    //    '''approximation found in rogers: Short Course in Cloud Physics
    //    p.126'''
    //    assert np.all(r >= 0)
    //    assert np.all(r < 2.e-3)
    if (r > 2.e-3) {
        std::cout
            << "large drops present, adjust droplet fall_speed function, r="
            << r << std::endl;
    }

    double k1 = 1.19e8;
    double k2 = 8e3;
    double k3 = 2.01e2;

    double r1 = 40.e-6;
    double r2 = 0.6e-3;

    if (r < r1) {
        return k1 * r*r;
    }
    else if ( r >= r1 && r < r2){
        return k2 * r;
    }
    else {
        return k3 * std::sqrt(r);
    }
}
