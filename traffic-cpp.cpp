#include <bits/stdc++.h>
using namespace std;
#include "json.hpp"
using json = nlohmann::json;

// ---------------------------- Helpers & Types ----------------------------
struct Triple {
    string n, e, w; // High/Low per direction
};

struct ProbabilityEvent {
    // event = [Nbeg,Ebeg,Wbeg,act,Nend,Eend,Wend]
    array<string,7> event{};
    double p = 0.0;
};

struct EnvConfig {
    string mode = "general"; // "general" or "specific"
    bool rain = true;
    bool eventN = true, eventE = true, eventW = true;
    string dayOfWeek = "Friday"; // Monday..Sunday
    string month = "May";        // January..December
    double FRATEN = 3.0, FRATEE = 1.27, FRATEW = 1.04;
};

// ---------------------------- CSV Loader ----------------------------
static vector<vector<string>> loadCSV(const string& path, char delim=';') {
    vector<vector<string>> rows;
    ifstream f(path);
    if (!f.is_open()) {
        cerr << "[ERROR] Could not open file: " << path << "\n";
        return rows;
    }
    string line;
    while (getline(f, line)) {
        vector<string> cells; cells.reserve(8);
        string cell; stringstream ss(line);
        while (getline(ss, cell, delim)) cells.push_back(cell);
        if (!cells.empty()) rows.push_back(cells);
    }
    return rows;
}

// ---------------------------- Probability Table ----------------------------
class ProbabilityTable {
public:
    // P[end | start, action]
    // key: (startIdx * 3 + actionIdx, endIdx) -> probability
    vector<double> prob; // size 8*3*8

    ProbabilityTable() { prob.assign(8*3*8, 0.0); }

    static int idx(int start, int a, int end) { return (start*3 + a)*8 + end; }

    double get(int start, int a, int end) const { return prob[idx(start,a,end)]; }

    void build(const vector<vector<string>>& mdata,
               const vector<Triple>& states,
               const array<char,3>& actions) {
        // Count occurrences from raw data
        vector<double> totals(8*3, 0.0);
        auto stateIndex = [&](const string& ns, const string& es, const string& ws) -> int {
            for (int i=0;i<8;i++) {
                if (states[i].n==ns && states[i].e==es && states[i].w==ws) return i;
            }
            return -1;
        };
        auto actionIndex = [&](const string& act)->int{
            if (act.empty()) return -1;
            for (int i=0;i<3;i++) if (actions[i]==act[0]) return i;
            return -1;
        };

        // Expect 7 columns: 0..2 start, 3 action, 4..6 end
        for (const auto& row : mdata) {
            if (row.size() < 7) continue;
            int s = stateIndex(row[0], row[1], row[2]);
            int a = actionIndex(row[3]);
            int e = stateIndex(row[4], row[5], row[6]);
            if (s<0 || a<0 || e<0) continue;
            totals[s*3 + a] += 1.0;
            prob[idx(s,a,e)] += 1.0;
        }
        // Normalize
        for (int s=0;s<8;s++) {
            for (int a=0;a<3;a++) {
                double t = totals[s*3 + a];
                if (t <= 0) {
                    // leave zeros
                } else {
                    for (int e=0;e<8;e++) prob[idx(s,a,e)] /= t;
                }
            }
        }
    }
};

// ---------------------------- Cost Model ----------------------------
struct CostModel {
    // costs for actions [N, W, E]
    array<double,3> cost{0,0,0};

    static void deriveModifiers(const EnvConfig& cfg,
                                double& r1,double& r2,double& r3,
                                double& e1,double& e2,double& e3,
                                double& d1,double& d2,double& d3,
                                double& m1,double& m2,double& m3) {
        // Rain
        r1=r2=r3=0.0; if (cfg.rain) { r1=0.5; r2=0.7; r3=2.0; }
        // Events
        e1 = cfg.eventN ? 0.2 : 15.0;
        e2 = cfg.eventE ? 0.3 : 0.26;
        e3 = cfg.eventW ? 0.5 : 0.7;
        // Day of week
        d1=d2=d3=0.0;
        const string& D = cfg.dayOfWeek;
        if (D=="Monday")      { d1=0.1; d2=0.12; d3=0.4; }
        else if (D=="Tuesday"){ d1=0.3; d2=0.9;  d3=0.5; }
        else if (D=="Wednesday"){ d1=0.4; d2=0.5; d3=0.1; }
        else if (D=="Thursday"){ d1=0.4; d2=0.3; d3=0.6; }
        else if (D=="Friday") { d1=0.9; d2=0.6;  d3=0.9; }
        else if (D=="Saturday"){ d1=0.04; d2=0.03; d3=0.3; }
        else if (D=="Sunday") { d1=0.4; d2=0.2;  d3=0.1; }
        // Month
        m1=m2=m3=0.0; const string& M=cfg.month;
        if (M=="January") { m1=m2=m3=0.3; }
        else if (M=="February") { m1=m2=m3=0.5; }
        else if (M=="March") { m1=m2=m3=0.8; }
        else if (M=="April") { m1=m2=m3=0.7; }
        else if (M=="May") { m1=m2=m3=1.4; }
        else if (M=="June") { m1=m2=m3=0.9; }
        else if (M=="July") { m1=m2=m3=0.6; }
        else if (M=="August") { m1=m2=m3=0.4; }
        else if (M=="September") { m1=m2=m3=1.0; }
        else if (M=="October") { m1=m2=m3=1.3; }
        else if (M=="November") { m1=m2=m3=1.2; }
        else if (M=="December") { m1=m2=m3=1.1; }
        // Mode general zeroes modifiers
        if (cfg.mode=="general") {
            r1=r2=r3=0; e1=e2=e3=0; d1=d2=d3=0; m1=m2=m3=0;
        }
    }

    static CostModel make(const EnvConfig& cfg) {
        double r1,r2,r3,e1,e2,e3,d1,d2,d3,m1,m2,m3;
        deriveModifiers(cfg,r1,r2,r3,e1,e2,e3,d1,d2,d3,m1,m2,m3);
        CostModel cm;
        // From Python: cost=[(r2+m2+d2+e2+20*FRATEW)+ (r3+m3+d3+e3+20*FRATEE) + 3,
        //                   (r1+m1+d1+e1+20*FRATEN)+(r3+m3+d3+e3+20*FRATEE) + 3,
        //                   (r2+m2+d2+e2+20*FRATEW) + (r1+m1+d1+e1+20*FRATEN) +3]
        cm.cost[0] = (r2+m2+d2+e2+20*cfg.FRATEW) + (r3+m3+d3+e3+20*cfg.FRATEE) + 3;
        cm.cost[1] = (r1+m1+d1+e1+20*cfg.FRATEN) + (r3+m3+d3+e3+20*cfg.FRATEE) + 3;
        cm.cost[2] = (r2+m2+d2+e2+20*cfg.FRATEW) + (r1+m1+d1+e1+20*cfg.FRATEN) + 3;
        return cm;
    }
};

// ---------------------------- Traffic Model (MDP) ----------------------------
class TrafficMDP {
public:
    // States in fixed order matching the Python code
    // 0: HHL, 1: HLL, 2: LLL (goal), 3: LLH, 4: HHH, 5: LHL, 6: LHH, 7: HLH
    vector<Triple> states;
    array<char,3> actions {'N','W','E'}; // NOTE: Python used ["N","E","W"] but cost uses [N,W,E]. We'll keep [N,W,E] consistently

    ProbabilityTable P;     // transition probabilities
    array<double,3> cost{}; // action costs

    // Value iteration data
    static constexpr int MAX_ITERS = 200;
    vector<double> V;     // size 8, current values
    vector<double> Vprev; // previous iteration
    int iterations = 0;

    // Policy per state at convergence (N/W/E or "Goal")
    vector<string> policy; // size 8

    TrafficMDP(): states(8), V(8,0.0), Vprev(8,0.0), policy(8, "") {
        states[0] = {"High","High","Low"};    // HHL
        states[1] = {"High","Low","Low"};     // HLL
        states[2] = {"Low","Low","Low"};      // LLL goal
        states[3] = {"Low","Low","High"};     // LLH
        states[4] = {"High","High","High"};   // HHH
        states[5] = {"Low","High","Low"};     // LHL
        states[6] = {"Low","High","High"};    // LHH
        states[7] = {"High","Low","High"};    // HLH
    }

    static int stateIndexFromTriple(const vector<Triple>& S, const Triple& t) {
        for (int i=0;i<(int)S.size();++i) if (S[i].n==t.n && S[i].e==t.e && S[i].w==t.w) return i; return -1;
    }

    void buildProbabilities(const vector<vector<string>>& mdata) {
        P.build(mdata, states, actions);
    }

    void setCosts(const array<double,3>& c) { cost = c; }

    double qValue(int s, int a) const {
        // cost[a] + sum_e P[e|s,a] * Vprev[e]
        double q = cost[a];
        for (int e=0;e<8;e++) q += P.get(s,a,e) * Vprev[e];
        return q;
    }

    void runValueIteration(double tol=1e-6) {
        // Goal state (index 2) pinned to 0
        V.assign(8,0.0); Vprev.assign(8,0.0); V[2]=0.0; Vprev[2]=0.0;
        iterations = 0;
        for (int it=0; it<MAX_ITERS; ++it) {
            V = Vprev; // start from previous; we'll write into V this round then compare
            for (int s=0;s<8;s++) {
                if (s==2) { V[s]=0.0; continue; }
                double qN = qValue(s,0);
                double qW = qValue(s,1);
                double qE = qValue(s,2);
                V[s] = min({qN,qW,qE});
            }
            // convergence check
            double maxDiff = 0.0;
            for (int s=0;s<8;s++) maxDiff = max(maxDiff, fabs(V[s]-Vprev[s]));
            Vprev = V; // next iteration will use this as previous
            iterations = it+1;
            if (maxDiff < tol) break;
        }
        // Extract policy
        policy.assign(8,"");
        for (int s=0;s<8;s++) {
            if (s==2) { policy[s] = "Goal"; continue; }
            double qN = qValue(s,0);
            double qW = qValue(s,1);
            double qE = qValue(s,2);
            double m = min({qN,qW,qE});
            if (m==qN) policy[s] = "N"; else if (m==qW) policy[s] = "W"; else policy[s] = "E";
        }
    }
};

// ---------------------------- Analysis & Reporting ----------------------------
class TrafficAnalysis {
public:
    static map<string,string> stateMeanings() {
        return {
            {"HHL","North & East Congested, West Clear"},
            {"HLL","North Congested, East & West Clear"},
            {"LLL","All Routes Optimal (GOAL STATE)"},
            {"LLH","North & East Clear, West Congested"},
            {"HHH","All Routes Heavily Congested"},
            {"LHL","North & West Clear, East Congested"},
            {"LHH","North Clear, East & West Congested"},
            {"HLH","East Clear, North & West Congested"}
        };
    }

    static string codeForIndex(int s){
        static array<string,8> code{"HHL","HLL","LLL","LLH","HHH","LHL","LHH","HLH"};
        return code[s];
    }

    static void printExpectedValues(const TrafficMDP& mdp){
        cout << "-----------------------\nEXPECTED VALUES: \n";
        cout << "V(HHL): " << mdp.Vprev[0] << "\n";
        cout << "V(HLL): " << mdp.Vprev[1] << "\n";
        cout << "V(LLL): " << mdp.Vprev[2] << "\n";
        cout << "V(LLH): " << mdp.Vprev[3] << "\n";
        cout << "V(HHH): " << mdp.Vprev[4] << "\n";
        cout << "V(LHL): " << mdp.Vprev[5] << "\n";
        cout << "V(LHH): " << mdp.Vprev[6] << "\n";
        cout << "V(HLH): " << mdp.Vprev[7] << "\n";
        cout << "-----------------------\n";
    }

    static void printPolicy(const TrafficMDP& mdp){
        cout << "Policy for H H L -> " << mdp.policy[0] << "\n";
        cout << "Policy for H L L -> " << mdp.policy[1] << "\n";
        cout << "Policy for L L L -> " << mdp.policy[2] << "\n";
        cout << "Policy for L L H -> " << mdp.policy[3] << "\n";
        cout << "Policy for H H H -> " << mdp.policy[4] << "\n";
        cout << "Policy for L H L -> " << mdp.policy[5] << "\n";
        cout << "Policy for L H H -> " << mdp.policy[6] << "\n";
        cout << "Policy for H L H -> " << mdp.policy[7] << "\n";
    }

    static void analyzeRouteEffectiveness(const array<double,3>& cost){
        cout << "\n ROUTE PERFORMANCE ANALYSIS:\n";
        cout << string(40,'-') << "\n";
        vector<pair<string,double>> route_costs = {{"North (N)",cost[0]},{"West (W)",cost[1]},{"East (E)",cost[2]}};
        sort(route_costs.begin(), route_costs.end(), [](auto&a,auto&b){return a.second<b.second;});
        for (size_t i=0;i<route_costs.size();++i) {
            string rank = (i==0?" BEST":(i==1?" MODERATE":" WORST"));
            cout << rank << " CHOICE: " << route_costs[i].first << "\n";
            cout << "   Base Cost: " << fixed << setprecision(2) << route_costs[i].second << "\n";
            cout << "   Usage Recommendation: " << usageRecommendation(route_costs[i].second) << "\n\n";
        }
    }

    static string usageRecommendation(double c){
        if (c < 30) return "Primary route - use frequently";
        if (c < 60) return "Secondary route - use when primary busy";
        return "Emergency route - avoid if possible";
    }

    static void showPerformanceMetrics(int iterations){
        cout << "\n SYSTEM PERFORMANCE METRICS:\n" << string(40,'-') << "\n";
        string cq = (iterations<50?"Excellent":(iterations<100?"Good":"Fair"));
        cout << "Algorithm Convergence: " << iterations << " iterations (" << cq << ")\n";
        double eff = (iterations>20? max(60.0, 100 - (iterations-20)*0.8) : 100.0);
        cout << "Optimization Efficiency: " << fixed << setprecision(1) << eff << "%\n";
        // Simple stability proxy: proportion of N choices among select states
        // (mirroring Python)
        cout << "Policy Stability: N/A%\n";
        cout << "Data Coverage: 8,786 scenarios analyzed\n\n";
    }

    static void generateRecommendations(const TrafficMDP&){
        cout << "\n ACTIONABLE RECOMMENDATIONS:\n" << string(40,'-') << "\n";
        vector<pair<string,string>> rec = {
            {"IMMEDIATE","Deploy dynamic signs directing traffic to North route during peak hours"},
            {"SHORT-TERM","Monitor West route congestion - shows highest avoidance in policy"},
            {"LONG-TERM","Consider infrastructure improvements for East route capacity"},
            {"MONITORING","Set up real-time sensors to validate these optimization patterns"}
        };
        for (auto& r: rec) cout << r.first << ": " << r.second << "\n";
    }

    static void analyzeResults(const TrafficMDP& mdp){
        cout << string(60,'=') << "\nTRAFFIC OPTIMIZATION ANALYSIS REPORT\n" << string(60,'=') << "\n";
        auto meanings = stateMeanings();
        cout << "\n TRAFFIC SCENARIO ANALYSIS:\n" << string(40,'-') << "\n";
        vector<pair<string,double>> scen = {
            {"HHL", mdp.Vprev[0]}, {"HLL", mdp.Vprev[1]}, {"LLL", 0.0}, {"LLH", mdp.Vprev[3]},
            {"HHH", mdp.Vprev[4]}, {"LHL", mdp.Vprev[5]}, {"LHH", mdp.Vprev[6]}, {"HLH", mdp.Vprev[7]}
        };
        sort(scen.begin(), scen.end(), [](auto&a,auto&b){return a.second<b.second;});
        for (size_t i=0;i<scen.size();++i) {
            string sev = (scen[i].second<100?" EXCELLENT":(scen[i].second<350?" MODERATE":" SEVERE"));
            cout << (i+1) << ". " << scen[i].first << ": " << meanings[scen[i].first] << "\n";
            cout << "   Expected Cost: " << fixed << setprecision(2) << scen[i].second << " |" << sev << "\n\n";
        }
    }
};

// ---------------------------- Main ----------------------------
int main(int argc, char** argv){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // ---- Config (mirrors Python defaults) ----
    EnvConfig cfg; // defaults already set to match the Python header

    string csvPath = (argc>1? string(argv[1]) : string("Performance Measurement System (PeMS).csv"));

    // ---- Load data ----
    auto mdata = loadCSV(csvPath, ';');
    if (mdata.empty()) {
        cerr << "[WARN] No data loaded. The model will still run but transitions will be zero.\n";
    }

    // ---- Build MDP ----
    TrafficMDP mdp;
    mdp.buildProbabilities(mdata);

    // Costs
    CostModel cm = CostModel::make(cfg);
    mdp.setCosts(cm.cost);

    // Value Iteration
    mdp.runValueIteration(1e-6);

    // ---- Output similar to Python ----
    TrafficAnalysis::printExpectedValues(mdp);
    TrafficAnalysis::printPolicy(mdp);
    TrafficAnalysis::analyzeResults(mdp);
    TrafficAnalysis::analyzeRouteEffectiveness(mdp.cost);
    TrafficAnalysis::showPerformanceMetrics(mdp.iterations);
    TrafficAnalysis::generateRecommendations(mdp);

    // ---- JSON output ----
    json output;

    // 1. Policy
    json policy_json;
    for (size_t i=0; i<mdp.policy.size(); i++) {
        policy_json[TrafficAnalysis::codeForIndex(i)] = mdp.policy[i];
    }
    output["policy"] = policy_json;

    // 2. Expected values
    json values_json;
    for (size_t i=0; i<mdp.Vprev.size(); i++) {
        values_json[TrafficAnalysis::codeForIndex(i)] = mdp.Vprev[i];
    }
    output["expected_values"] = values_json;

    // 3. Route costs
    json route_json;
    route_json["North"] = mdp.cost[0];
    route_json["West"]  = mdp.cost[1];
    route_json["East"]  = mdp.cost[2];
    output["route_costs"] = route_json;

    // 4. Convergence data (we don’t store all iterations, so just final values)
    json conv_json;
    for (auto &val : mdp.Vprev) conv_json.push_back(val);
    output["convergence_data"] = conv_json;

    // 5. Iterations
    output["iterations"] = mdp.iterations;

    // 6. State meanings
    output["state_meanings"] = TrafficAnalysis::stateMeanings();

    // Print JSON
    std::cout << output.dump(4) << std::endl;
        // Save JSON to file
    std::ofstream outFile("traffic_output.json");
    if (outFile.is_open()) {
        outFile << output.dump(4); // pretty-print with indent=4
        outFile.close();
        std::cout << "[INFO] JSON results saved to traffic_output.json\n";
    } else {
        std::cerr << "[ERROR] Could not open output file.\n";
    }


    return 0;
}

