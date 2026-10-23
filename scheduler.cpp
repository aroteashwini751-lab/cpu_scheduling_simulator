
#include <algorithm>
#include <climits>
#include <iomanip>
#include <iostream>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace std;

using ll = long long;

struct Process {
    string pid;
    ll arrival, burst, priority;
    ll remaining = 0;
    ll completion = 0;
    ll firstStart = -1;
    int order = 0;
};

struct Segment {
    string pid;
    ll start, end;
};

vector<Process> p;
vector<Segment> gantt;

void addSegment(string pid, ll start, ll end) {
    if (start == end) return;

    if (!gantt.empty() && gantt.back().pid == pid &&
        gantt.back().end == start) {
        gantt.back().end = end;
    } else {
        gantt.push_back({pid, start, end});
    }
}

void runFCFS() {
    vector<int> ids(p.size());
    for (int i = 0; i < (int)p.size(); ++i) ids[i] = i;

    stable_sort(ids.begin(), ids.end(), [](int a, int b) {
        return p[a].arrival < p[b].arrival;
    });

    ll time = 0;

    for (int i : ids) {
        if (time < p[i].arrival) {
            addSegment("IDLE", time, p[i].arrival);
            time = p[i].arrival;
        }

        p[i].firstStart = time;
        addSegment(p[i].pid, time, time + p[i].burst);
        time += p[i].burst;
        p[i].completion = time;
    }
}

void runNonPreemptive(bool priorityMode) {
    int n = (int)p.size();
    vector<bool> done(n, false);
    ll time = 0;
    int completed = 0;

    while (completed < n) {
        int best = -1;

        for (int i = 0; i < n; ++i) {
            if (done[i] || p[i].arrival > time) continue;

            if (best == -1) {
                best = i;
                continue;
            }

            ll a = priorityMode ? p[i].priority : p[i].burst;
            ll b = priorityMode ? p[best].priority : p[best].burst;

            if (a < b || (a == b && p[i].arrival < p[best].arrival)) {
                best = i;
            }
        }

        if (best == -1) {
            ll nextArrival = LLONG_MAX;
            for (int i = 0; i < n; ++i) {
                if (!done[i])
                    nextArrival = min(nextArrival, p[i].arrival);
            }

            addSegment("IDLE", time, nextArrival);
            time = nextArrival;
            continue;
        }

        p[best].firstStart = time;
        addSegment(p[best].pid, time, time + p[best].burst);
        time += p[best].burst;
        p[best].completion = time;
        done[best] = true;
        ++completed;
    }
}

void runSRTF() {
    int n = (int)p.size();
    ll time = 0;
    int completed = 0;

    for (auto &x : p) x.remaining = x.burst;

    while (completed < n) {
        int best = -1;

        for (int i = 0; i < n; ++i) {
            if (p[i].arrival > time || p[i].remaining == 0) continue;

            if (best == -1 ||
                p[i].remaining < p[best].remaining ||
                (p[i].remaining == p[best].remaining &&
                 p[i].arrival < p[best].arrival)) {
                best = i;
            }
        }

        if (best == -1) {
            ll nextArrival = LLONG_MAX;
            for (const auto &x : p)
                if (x.remaining > 0)
                    nextArrival = min(nextArrival, x.arrival);

            addSegment("IDLE", time, nextArrival);
            time = nextArrival;
            continue;
        }

        if (p[best].firstStart == -1)
            p[best].firstStart = time;

        addSegment(p[best].pid, time, time + 1);
        --p[best].remaining;
        ++time;

        if (p[best].remaining == 0) {
            p[best].completion = time;
            ++completed;
        }
    }
}

void runRR(ll quantum) {
    int n = (int)p.size();
    vector<int> ids(n);

    for (int i = 0; i < n; ++i) {
        ids[i] = i;
        p[i].remaining = p[i].burst;
    }

    stable_sort(ids.begin(), ids.end(), [](int a, int b) {
        return p[a].arrival < p[b].arrival;
    });

    queue<int> ready;
    int next = 0, completed = 0;
    ll time = 0;

    while (completed < n) {
        if (ready.empty()) {
            if (next < n && time < p[ids[next]].arrival) {
                addSegment("IDLE", time, p[ids[next]].arrival);
                time = p[ids[next]].arrival;
            }

            while (next < n && p[ids[next]].arrival <= time)
                ready.push(ids[next++]);

            if (ready.empty()) continue;
        }

        int i = ready.front();
        ready.pop();

        if (p[i].firstStart == -1)
            p[i].firstStart = time;

        ll run = min(quantum, p[i].remaining);
        addSegment(p[i].pid, time, time + run);
        time += run;
        p[i].remaining -= run;

        while (next < n && p[ids[next]].arrival <= time)
            ready.push(ids[next++]);

        if (p[i].remaining > 0) {
            ready.push(i);
        } else {
            p[i].completion = time;
            ++completed;
        }
    }
}

string escapeJSON(const string &s) {
    string out;

    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else {
            out += c;
        }
    }

    return out;
}

void outputJSON(const string &algorithm,
                const string &recommendation,
                const string &reason) {
    double totalWait = 0, totalTurnaround = 0;
    ll totalBurst = 0, finish = 0;

    for (const auto &x : p) {
        ll turnaround = x.completion - x.arrival;
        ll waiting = turnaround - x.burst;

        totalWait += waiting;
        totalTurnaround += turnaround;
        totalBurst += x.burst;
        finish = max(finish, x.completion);
    }

    double avgWait = totalWait / p.size();
    double avgTurn = totalTurnaround / p.size();
    double utilization = finish == 0 ? 0 :
        100.0 * totalBurst / finish;

    cout << fixed << setprecision(2);
    cout << "{";
    cout << "\"algorithm\":\"" << algorithm << "\",";
    cout << "\"recommendation\":\"" << recommendation << "\",";
    cout << "\"reason\":\"" << escapeJSON(reason) << "\",";
    cout << "\"avgWaiting\":" << avgWait << ",";
    cout << "\"avgTurnaround\":" << avgTurn << ",";
    cout << "\"utilization\":" << utilization << ",";

    cout << "\"processes\":[";
    for (int i = 0; i < (int)p.size(); ++i) {
        if (i) cout << ",";
        const auto &x = p[i];

        ll turnaround = x.completion - x.arrival;
        ll waiting = turnaround - x.burst;
        ll response = x.firstStart - x.arrival;

        cout << "{\"pid\":\"" << escapeJSON(x.pid) << "\",";
        cout << "\"arrival\":" << x.arrival << ",";
        cout << "\"burst\":" << x.burst << ",";
        cout << "\"priority\":" << x.priority << ",";
        cout << "\"completion\":" << x.completion << ",";
        cout << "\"turnaround\":" << turnaround << ",";
        cout << "\"waiting\":" << waiting << ",";
        cout << "\"response\":" << response << "}";
    }

    cout << "],\"gantt\":[";
    for (int i = 0; i < (int)gantt.size(); ++i) {
        if (i) cout << ",";
        cout << "{\"pid\":\"" << escapeJSON(gantt[i].pid) << "\",";
        cout << "\"start\":" << gantt[i].start << ",";
        cout << "\"end\":" << gantt[i].end << "}";
    }

    cout << "]}" << endl;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    try {
        string requested;
        ll quantum;
        int n;

        if (!(cin >> requested >> quantum >> n))
            throw runtime_error("Invalid input.");

        if (n < 1 || n > 100)
            throw runtime_error("Use between 1 and 100 processes.");

        for (int i = 0; i < n; ++i) {
            Process x;
            if (!(cin >> x.pid >> x.arrival >> x.burst >> x.priority))
                throw runtime_error("Invalid process data.");

            if (x.arrival < 0 || x.burst < 1 || x.priority < 0 ||
                x.arrival > 1000000 || x.burst > 1000000 ||
                x.priority > 1000000)
                throw runtime_error("Process values are out of range.");

            x.order = i;
            p.push_back(x);
        }

        if (quantum < 1 || quantum > 1000000)
            throw runtime_error("Invalid time quantum.");

        string selected = requested;
        string reason;

        if (requested == "AUTO") {
            ll minBurst = LLONG_MAX, maxBurst = 0;
            ll minPriority = LLONG_MAX, maxPriority = 0;
            ll maxArrival = 0;

            for (const auto &x : p) {
                minBurst = min(minBurst, x.burst);
                maxBurst = max(maxBurst, x.burst);
                minPriority = min(minPriority, x.priority);
                maxPriority = max(maxPriority, x.priority);
                maxArrival = max(maxArrival, x.arrival);
            }

            if (maxPriority > minPriority) {
                selected = "PRIORITY";
                reason = "Priority values differ. Non-preemptive Priority "
                         "Scheduling favors the lowest numerical priority "
                         "among processes that have arrived.";
            } else if (p.size() >= 5) {
                selected = "RR";
                reason = "There are at least five processes and no priority "
                         "differences. Round Robin offers time-sharing; "
                         "its behavior depends on the chosen time quantum.";
            } else if (maxBurst > minBurst && maxArrival > 0) {
                selected = "SRTF";
                reason = "Burst lengths vary and processes arrive at "
                         "different times. SRTF favors the shortest remaining "
                         "execution time, though it may cause starvation.";
            } else {
                selected = "SJF";
                reason = "A small workload with no distinct priority needs "
                         "is suited to SJF as a baseline for low average "
                         "waiting time when burst lengths are known.";
            }
        } else if (requested != "FCFS" && requested != "SJF" &&
                   requested != "SRTF" && requested != "PRIORITY" &&
                   requested != "RR") {
            throw runtime_error("Unknown scheduling algorithm.");
        }

        if (selected == "FCFS") {
            reason = "FCFS runs ready processes in arrival order. It is "
                     "simple and fair by arrival order, but long jobs can "
                     "delay short jobs.";
            runFCFS();
        } else if (selected == "SJF") {
            if (requested != "AUTO")
                reason = "Non-preemptive SJF chooses the shortest burst "
                         "among ready processes. It can lower average "
                         "waiting time when burst estimates are accurate.";
            runNonPreemptive(false);
        } else if (selected == "SRTF") {
            if (requested != "AUTO")
                reason = "SRTF always chooses the ready process with the "
                         "shortest remaining time. It can improve response "
                         "for short jobs but adds preemption and may starve "
                         "long jobs.";
            runSRTF();
        } else if (selected == "PRIORITY") {
            if (requested != "AUTO")
                reason = "Non-preemptive Priority Scheduling selects the "
                         "highest-priority ready process; lower numbers "
                         "represent higher priority. Aging is not implemented.";
            runNonPreemptive(true);
        } else {
            if (requested != "AUTO")
                reason = "Round Robin gives each ready process up to one "
                         "time quantum per turn. A small quantum increases "
                         "switching overhead in real systems; overhead is "
                         "not included in this simulator.";
            runRR(quantum);
        }

        string displayName = selected;
        if (selected == "FCFS") displayName = "FCFS";
        if (selected == "SJF") displayName = "SJF (Non-preemptive)";
        if (selected == "SRTF") displayName = "SRTF (Preemptive SJF)";
        if (selected == "PRIORITY")
            displayName = "Priority (Non-preemptive)";
        if (selected == "RR") displayName = "Round Robin";

        outputJSON(displayName, displayName, reason);
    } catch (const exception &e) {
        cout << "{\"error\":\"" << escapeJSON(e.what()) << "\"}" << endl;
        return 1;
    }

    return 0;
}