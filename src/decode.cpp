//--------------------------------------------------------------
// nurbs decoding algorithms
// ref: [P&T] Piegl & Tiller, The NURBS Book, 1995
//
// Tom Peterka
// Argonne National Laboratory
// tpeterka@mcs.anl.gov
//--------------------------------------------------------------

#include <mfa/types.hpp>
#include <mfa/encode.hpp>
#include <mfa/decode.hpp>

// eigen objects
#include <Eigen/Dense>

#include <vector>

using namespace std;
using namespace Eigen;

// compute a point from a 1d NURBS curve at a given parameter value
// algorithm 4.1, Piegl & Tiller (P&T) p.124
// this version recomputes basis functions rather than taking them as an input
// this version also assumes weights = 1; no division by weight is done
void CurvePt1d(int            p,             // polynomial degree
               vector<Pt2d>&  ctrl_pts,      // control points
               vector<float>& knots,         // knots
               float          param,         // parameter value of desired point
               Pt2d&          out_pt)        // (output) point
{
    int n      = (int)ctrl_pts.size() - 1;   // number of control point spans
    int span   = FindSpan(p, n, knots, param);
    MatrixXf N = MatrixXf::Zero(1, n + 1);   // basis coefficients
    BasisFuns(p, knots, param, span, N, 0, n, 0);
    out_pt.x = 0.0;
    out_pt.y = 0.0;
    for (int j = 0; j <= p; j++)
    {
        out_pt.x += N(0, j + span - p) * ctrl_pts[span - p + j].x;
        out_pt.y += N(0, j + span - p) * ctrl_pts[span - p + j].y;
    }
    // debug
    // cerr << "n " << n << " param " << param << " span " << span << " out_pt " << out_pt << endl;
    // cerr << " N " << N << endl;
}

// max distance from a set of input points to a 1d NURBS curve
// P&T eq. 9.77, p. 424
// this version recomputes parameter values of input points and
// recomputes basis functions rather than taking them as an input
// this version also assumes weights = 1; no division by weight is done
// assumes all vectors have been correctly resized by the caller
void MaxErr1d(int            p,              // polynomial degree
              vector<Pt1d>&  domain,         // domain of input data points
              vector<float>& range,          // range of input data points, same length as domain
              vector<Pt2d>&  ctrl_pts,       // control points
              vector<float>& knots,          // knots
              vector<Pt2d>&  approx,         // points on approximated curve
                                             // (same number as input points, for rendering only)
              vector<float>& errs,           // (output) error at each input point
              float&         max_err)        // (output) maximum error
{
    // curve parameters for input points
    vector<float> params(domain.size());     // curve parameters for input data points
    Params1d(domain, range, params);

    // errors and max error
    max_err = 0;
    for (size_t i = 0; i < domain.size(); i++)
    {
        Pt2d cpt;                            // point on curve at parameter of input point
        CurvePt1d(p, ctrl_pts, knots, params[i], cpt);
        approx[i] = cpt;
        Pt2d ipt(domain[i].x, range[i]);     // input point
        errs[i] = Pt2d::dist(cpt, ipt);
        if (i == 0 || errs[i] > max_err)
            max_err = errs[i];

        // debug
        // cerr << "input point " << ipt << " param " << params[i] <<
        //     " curve point " << cpt << endl;
   }
}

// max norm distance from a set of input points to a 1d NURBS curve
// P&T eq. 9.78, p. 424
// usually a smaller and more accurate measure of max error than MaxError
// this version recomputes parameter values of input points and
// recomputes basis functions rather than taking them as an input
// this version also assumes weights = 1; no division by weight is done
// assumes all vectors have been correctly resized by the caller
void MaxNormErr1d(int            p,          // polynomial degree
                  vector<Pt1d>&  domain,     // domain of input data points
                  vector<float>& range,      // range of input data points, same length as domain
                  vector<Pt2d>&  ctrl_pts,   // control points
                  vector<float>& knots,      // knots
                  int            max_niter,  // max num iterations to search for nearest curve pt
                  float          err_bound,  // desired error bound (stop searching if less)
                  int            search_rad, // number of parameter steps to search path on either
                                             // side of parameter value of input point
                  vector<Pt2d>&  approx,     // points on approximated curve
                                             // (same number as input points, for rendering only)
                  vector<float>& errs,       // (output) error at each input point
                  float&         max_err)    // (output) maximum error from any input point to curve
{
    // curve parameters for input points
    vector<float> params(domain.size());     // curve parameters for input data points
    Params1d(domain, range, params);

    // fit approximated curve (for debugging and rendering only)
    for (size_t i = 0; i < domain.size(); i++)
    {
        Pt2d cpt;                            // point on curve at parameter of input point
        CurvePt1d(p, ctrl_pts, knots, params[i], cpt);
        approx[i] = cpt;
        // debug
        // cerr <<  "param " << params[i] << " curve point " << approx[i] << endl;
    }

    // errors and max error
    max_err = 0;
    for (size_t i = 0; i < domain.size(); i++)
    {
        Pt2d ipt(domain[i].x, range[i]);     // input point

        // find nearest curve point
        Pt2d cpt;                            // point on curve at parameter of input point
        float ul, uh, um;                    // low, high, middle  parameter values

        // range of parameter values to search
        if (i < search_rad)
        {
            ul = params[0];
            uh = params[i + search_rad];
        }
        else if (i > domain.size() - 1 - search_rad)
        {
            uh = params[domain.size() - 1];
            ul = params[i < search_rad];
        }
        else
        {
            ul = params[i - search_rad];
            uh = params[i + search_rad];
        }
        um = (ul + uh) / 2.0;

        // debug
        // cerr << "i " << i << " ul " << ul << " um " << um << " uh " << uh << endl;

        int j;
        float el, eh, em;                    // low, high, mid errs; dists to C(ul), C(uh), C(um)
        for (j = 0; j < max_niter; j++)
        {
            CurvePt1d(p, ctrl_pts, knots, ul, cpt);
            el = Pt2d::dist(cpt, ipt);      // distance to C(ul)
            // cerr << "low " << cpt << " el " << el;         // debug
            CurvePt1d(p, ctrl_pts, knots, um, cpt);
            em = Pt2d::dist(cpt, ipt);      // distance to C(um)
            // cerr << " mid " << cpt << " em " << em;        // debug
            CurvePt1d(p, ctrl_pts, knots, uh, cpt);
            eh = Pt2d::dist(cpt, ipt);     // distance to C(uh)
            // cerr << " hi " << cpt << " eh " << eh << endl; // debug

            if (el < eh && el < em)          // el is the best error
            {                                // shift the search interval to [uh, um]
                if (el <= err_bound)
                {
                    errs[i] = el;
                    // cerr << "el ";           // debug
                    break;
                }
                uh = um;
                um = (ul + uh) / 2.0;
            }

            else if (eh < el && eh < em)          // eh is the best error
            {                                // shift the search interval to [um, uh]
                if (eh <= err_bound)
                {
                    errs[i] = eh;
                    // cerr << "eh ";           // debug
                    break;
                }
                ul = um;
                um = (ul + uh) / 2.0;
            }

            else                             // em is the best error
            {                                // narrower search interval and centered on um
                if (em <= err_bound)
                {
                    errs[i] = em;
                    // cerr << "em ";           // debug
                    break;
                }
                ul = (ul + um) / 2.0;
                uh = (uh + um) / 2.0;
            }
        }
        if (j == max_niter)                   // max number of iterations was reached
        {                                     // pick minimum of el, eh, em
            if (el < eh && el < em)
                errs[i] = el;
            else if (eh < el && eh < em)
                errs[i] = eh;
            else
                errs[i] = em;
            // cerr << "max iter ";             // debug
        }

        // max error
        if (i == 0 || errs[i] > max_err)
            max_err = errs[i];

        // debug
        // cerr << "i " << i << "input point " << ipt << " param " << params[i] <<
        //     " err " << errs[i] << endl;
   }
}
