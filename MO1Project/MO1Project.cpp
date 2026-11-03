
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <string>
#include <set>
#include <vector>
#include <ctime>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <queue>
#include <iomanip>
#include <algorithm>
#include <sstream>
#include <cstdint>
#include <unordered_map>


using namespace std;


// ====================================
// Configuration (Kyan)
// ====================================
struct Config
{
    int numCPU = 0;
    string scheduler = "";

    unsigned long long quantumCycle = 0;
    unsigned long long processFreq = 0;
    unsigned long long minIns = 0;
    unsigned long long maxIns = 0;
    unsigned long long delays = 0;

};

unsigned long long parseConfigNum(const string& value)
{
    if (value.empty())
    {
        throw invalid_argument("Empty number value");
    }

    for (char digit : value)
    {
        if (digit < '0' || digit > '9')
        {
            throw invalid_argument("Invalid number format");
        }
    }

    unsigned long long number = stoull(value);
    if (number > 4294967295ULL)
    {
        throw out_of_range("Number exceed uint32 range");
    }
    return number;
}


// ====================================
// Instructions and data (Kyan)
// ====================================

enum class instructionsType
{
    PRINT, DECLARE, ADD, SUBTRACT, SLEEP, FOR
};

struct instructions
{
    instructionsType type = instructionsType::PRINT;
    vector<string> args;
    vector<instructions> body;
    unsigned int repeats = 0;
};

struct Process
{
    int pID = 0;
    string pName;
    string pState = "READY";

    unsigned long long totalInstructions = 0;
    unsigned long long currInstruction = 0;
    unsigned long long quantumSlice = 0;
    vector<instructions> instructions;
    unordered_map<string, uint16_t> variables;
    vector <string> logs; 
    int assignedCore = -1;
    string startTime;
    string finishTime;
};

// ====================================
// INSTRUCTION ENGINE 
// ====================================
// TODO:
// 1. Generate instructions for each process.
// 2. Implement PRINT, DECLARE, ADD, SUBTRACT.
// 3. Implement FOR loops (maximum nesting: 3).
// 4. Store variables in process.variables.
// 5. Store PRINT output in process.logs.
// 6. Update instruction execution progress.
// 7. Implement SLEEP and its CPU scheduling behavior.
// 8. Integrate the instruction engine into cpuSetup().
// 9. Update instruction progress and process completion.
// 10. Ensure FCFS and Round Robin still work correctly.
// VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV
vector<instructions> genInstructions(int count);
void doInstructions(Process& process);
// ====================================
// Process creation (Kyan)
// ====================================
Process createProcess(int pid, const string& name, unsigned long long noOfInstructions)
{
    Process newProcess;
    newProcess.pID = pid;
    newProcess.pName = name;
    newProcess.pState = "READY";
    newProcess.totalInstructions = noOfInstructions;
    newProcess.currInstruction = 0;
    return newProcess;
}

// ====================================
// PROCESS SCREENS 
// ====================================
// TODO:
// 1. Implement screen -s to create and attach to a process.
// 2. Implement screen -r to resume an existing process screen.
// 3. Implement process-smi to display process information.
// 4. Display instruction progress and PRINT logs.
// 5. Implement exit to return to the main console.
// 6. Prevent reattaching to finished processes.
// 7. Coordinate screen commands with the main CLI owner.
// 8. Connect screen commands to the main CLI.
// 9. Ensure screens work while processes execute in the background.
// VVVVVVVVVVVVVVVVVVVVVVVV
void showSMI(const Process& process);
void processScreen(int pid, vector<Process>&processes, mutex& processMutex); 
// ====================================
// REPORTING AND GENERATION 
// ====================================
// TODO:
// 1. Implement report-util to generate csopesy-log.txt.
// 2. Reuse the existing screenLS reporting information.
// 3. Display CPU utilization and process states in reports.
// 4. Generate processes with unique names (p01, p02, etc.).
// 5. Randomize instruction counts using min-ins and max-ins.
// 6. Use the existing createProcess() function.
// 7. Coordinate automatic generation with scheduler owner.
// 8. Implement scheduler-start and scheduler-stop.
// 9. Implement CPU ticks and delays-per-exec.
// 10. Integrate reporting and generation into the main CLI.
// VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV
void utilReport(ostream & output, const vector <Process>&process, const vector<int>&coreSlot);
Process genAutoProcess(int pid, const Config & config);


// ====================================
// Timestamps (Kyan)
// ====================================

string getTimestamp() {
    time_t now = time(nullptr);
    tm timeInfo{};
    localtime_s(&timeInfo, &now);
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%m/%d/%Y %I:%M:%S%p", &timeInfo);
    return string(buffer);
}

// ====================================
// Scheduler (Kyan)
// ====================================
void readyProcesses(vector<Process>& processes, queue<int>& readyQueue, vector<int>& coreSlot)
{
    for (int c = 0; c < static_cast<int>(coreSlot.size()); c++)
    {
        if (coreSlot[c] == -1 && !readyQueue.empty())
        {
            int pid = readyQueue.front();
            readyQueue.pop();

            for (Process& process : processes)
            {
                if (process.pID == pid)
                {
                    if (process.pState != "READY")
                    {
                        break;
                    }

                    process.assignedCore = c;
                    process.pState = "RUNNING";
                    process.quantumSlice = 0;
                    if (process.startTime.empty())
                    {
                        process.startTime = getTimestamp();
                    }
                    coreSlot[c] = pid;
                    //cout << "Process " << process.pName << " assigned to Core: " << c << "\n";
                    break;
                }
            }
        }
    }
}

void cpuSetup(int cID, vector<Process>& processes, vector<int>& coreSlot)
{
    int pid = coreSlot[cID];
    if (pid == -1)
    {
        return;
    }
    for (Process& process : processes)
    {
        if (process.pID != pid)
        {
            continue;
        }

        if (process.pState != "RUNNING")
        {
            return;
        }
        if (process.currInstruction < process.totalInstructions)
        {
            process.currInstruction++;
            //cout << "[Core: " << cID << "] " << process.pName << " executed " << process.currInstruction << "/" << process.totalInstructions << "\n";
        }

        if (process.currInstruction >= process.totalInstructions)
        {
            process.pState = "FINISHED";
            process.finishTime = getTimestamp();
            process.assignedCore = -1;

            coreSlot[cID] = -1;
            //cout << "Process " << process.pName << " has FINISHED!\n";
            //cout << "Core " << cID << " is now AVAILABLE\n";
        }
        return;
    }

}
// ====================================
// Process Listing (Kyan)
// ====================================
void screenLS(const vector<Process>& processes, const vector<int>& coreSlot)
{
    int coresUtilized = 0;
    for (int pid : coreSlot)
    {
        if (pid != -1)
        {
            coresUtilized++;
        }
    }
    int totalCores = static_cast<int>(coreSlot.size());
    int coresReady = totalCores - coresUtilized;

    double cpuUtil = 0;

    if (totalCores > 0)
    {
        cpuUtil = (static_cast<double>(coresUtilized) / totalCores) * 100;

    }
    cout << "\n CPU Utilization: " << fixed << setprecision(2) << cpuUtil << "%\n";
    cout << "Cores Used: " << coresUtilized << "\n";
    cout << "Cores Available: " << coresReady << "\n";

    cout << "\nRunning / Ready Processes: \n";

    for (const Process& p : processes)
    {
        if (p.pState != "FINISHED")
        {
            cout << p.pName << " | PID: " << p.pID << " | State: " << p.pState << " | " << p.currInstruction << "/" << p.totalInstructions << "\n";


        }
    }

    cout << "\nFinished processes:\n";

    for (const Process& p : processes)
    {
        if (p.pState == "FINISHED")
        {
            cout << p.pName << " | PID: " << p.pID << " | FINISHED" << " | " << p.currInstruction << "/" << p.totalInstructions << "\n";
        }
    }
    cout << "\n";
}

// ====================================
// CLI and Integration (Kyan)
// ====================================
int main() {
    std::string command;
    bool initialized = false;
    Config config;
    vector<Process> processes;
    int nextpID = 1;
    queue<int> readyQueue;
    vector<int> coreSlot;
    mutex processMutex;
    atomic<bool> runningScheduler{ false };
    thread schedulerThread;

    std::cout << "====================================\n";
    std::cout << "       CSOPESY OS EMULATOR\n";
    std::cout << "====================================\n";
    std::cout << "System startup successful!\n\n";


    while (true)
    {
        cout << "root:\\> ";
        getline(cin, command);

        if (command == "exit")
        {
            cout << "Exiting emulator...\n";
            break;
        }
        else if (command == "initialize")
        {
            if (initialized)
            {
                cout << "Already Initialized!\n";
                continue;
            }

            ifstream configFile("config.txt");

            if (!configFile.is_open())
            {
                std::cout << "ERROR: config.txt not found\n";
                continue;
            }
            std::string parameter;
            std::string value;
            set<string> foundParameters;
            bool duplicateParameter = false;
            bool unknownParameter = false;

            cout << "\nLoading Configuration...\n";
            try
            {
                while (configFile >> parameter >> value)
                {
                    const set<string> availableParameters = { "num-cpu", "scheduler", "quantum-cycles", "batch-process-freq", "min-ins", "max-ins", "delays-per-exec" };
                    if (availableParameters.count(parameter) == 0)
                    {
                        cout << "Error: Unknown parameter: " << parameter << "\n";

                        unknownParameter = true;
                        break;
                    }
                    if (foundParameters.count(parameter) > 0)
                    {
                        cout << "Error: Duplicate parameter detected: " << parameter << "\n";
                        duplicateParameter = true;
                        break;
                    }
                    foundParameters.insert(parameter);
                    if (parameter == "num-cpu")
                    {
                        unsigned long long cpuVal = parseConfigNum(value);

                        if (cpuVal > 128)
                        {
                            throw out_of_range("CPU count is over 128");
                        }
                        config.numCPU = static_cast<int>(cpuVal);
                    }
                    else if (parameter == "scheduler")
                    {
                        config.scheduler = value;
                    }
                    else if (parameter == "quantum-cycles")
                    {
                        config.quantumCycle = parseConfigNum(value);
                    }
                    else if (parameter == "batch-process-freq")
                    {
                        config.processFreq = parseConfigNum(value);
                    }
                    else if (parameter == "min-ins")
                    {
                        config.minIns = parseConfigNum(value);
                    }
                    else if (parameter == "max-ins")
                    {
                        config.maxIns = parseConfigNum(value);
                    }
                    else if (parameter == "delays-per-exec")
                    {
                        config.delays = parseConfigNum(value);
                    }
                    cout << parameter << " = " << value << "\n";
                }
            }
            catch (const std::exception& e)
            {
                cout << "Error: Invalid number value in config file\n";
                configFile.close();
                continue;
            }
            
            if (unknownParameter)
            {
                configFile.close();
                continue;
            }
            if (duplicateParameter)
            {
                configFile.close();
                continue;
            }
            configFile.close();


            const set<string> parameterChecking = { "num-cpu", "scheduler", "quantum-cycles", "batch-process-freq", "min-ins", "max-ins", "delays-per-exec" };
             

            bool missingParameters = false;
            for (const string& required : parameterChecking)
            {
                if (foundParameters.count(required) == 0)
                {
                    cout << "Error: Missing parameter: " << required << "\n";
                    missingParameters = true;
                }
            }
            if (missingParameters)
            {
                continue;
            }

            if (config.numCPU < 1 || config.numCPU > 128)
            {
                cout << "Error: CPU count must be within 1 - 128 \n";
                continue;
            }

            if (config.scheduler != "fcfs" && config.scheduler != "rr")
            {
                cout << "Error: Invalid scheduler, please use fcfs or rr\n";
                continue;
            }
            
            if (config.minIns == 0 || config.maxIns == 0)
            {
                cout << "Error: Instruction count must be greater than 0\n";
                continue;
            }
            if (config.minIns > config.maxIns)
            {
                cout << "Error: min-ins cannot be greater than max-ins\n";
                continue;
            }
            if (config.quantumCycle == 0)
            {
                cout << "Error: quantum-cycles must be greater than 0\n";
                continue;
            }
            if (config.processFreq == 0)
            {
                cout << "Error: batch-process-freq must be greater than 0\n";
                continue;
            }


            cout << "\nStored CPU count: " << config.numCPU << "\n";
            cout << "Stored Scheduler: " << config.scheduler << "\n";
            cout << "Stored Quantum: " << config.quantumCycle << "\n";
            coreSlot.assign(config.numCPU, -1);
            cout << "\nCPU cores have been initialized: " << coreSlot.size() << "\n";
            initialized = true;
            runningScheduler = true;
            schedulerThread = thread([&]()
                {
                    while (runningScheduler)
                    {
                        {

                        
                        lock_guard<mutex> lock(processMutex);
                        readyProcesses(processes, readyQueue, coreSlot);
                        for (int c = 0; c < static_cast<int>(coreSlot.size());
                            c++)
                        {
                            int pid = coreSlot[c];
                            if (pid == -1)
                            {
                                continue;
                            }
                            cpuSetup(c, processes, coreSlot);

                            if (coreSlot[c] != pid)
                            {
                                continue;
                            }

                            if (config.scheduler == "rr")
                            {
                                for (Process& process : processes)
                                {
                                    if (process.pID != pid)
                                    {
                                        continue;
                                    }
                                    process.quantumSlice++;
                                    if (process.quantumSlice >= config.quantumCycle)
                                    {
                                        process.pState = "READY";

                                        ofstream rrlog("rr-debug.txt", ios::app);
                                        rrlog << process.pName << " | Instructions compeleted: " << process.currInstruction << " | Quantum used: " << process.quantumSlice << "\n";

                                        rrlog.close();
                                        process.assignedCore = -1;
                                        process.quantumSlice = 0;
                                        coreSlot[c] = -1;
                                        readyQueue.push(pid);
                                        //cout << "RR: " << process.pName << " returned to the ready queue \n ";


                                    }
                                    break;
                                }
                            }
                        }
                    }
                    this_thread::sleep_for(chrono::milliseconds(200));
                        }
                });
            cout << "\nInitialization successful!\n\n";
        }

        else if (!initialized)
        {
            cout << "System is not initialized!\n";
        }
        else if (command.rfind("screen -s ", 0) == 0)
        {
            lock_guard<mutex> lock(processMutex);
            string pName = command.substr(10);
            if (pName.empty())
            {
                cout << "Error: Process name is empty!\n";
                continue;
            }

            bool nameAlreadyExists = false;
            for (const Process& process : processes)
            {
                if (process.pName == pName)
                {
                    nameAlreadyExists = true;
                    break;
                }
            }
            if (nameAlreadyExists)
            {
                cout << "Error: Name already exists! \n";
                continue;
            }
            Process newProcess = createProcess(nextpID++, pName, config.minIns);
            processes.push_back(newProcess);
            readyQueue.push(newProcess.pID);

            cout << "\nProcess was created successfully!\n";
            cout << "Process Name: " << newProcess.pName << "\n";
            cout << "Process ID: " << newProcess.pID << "\n";
            cout << "State: " << newProcess.pState << "\n";
            cout << "Instructions: " << newProcess.currInstruction << " / " << newProcess.totalInstructions << "\n\n";
        }
        else if (command == "screen -ls")
        {
            lock_guard<mutex> lock(processMutex);
            screenLS(processes, coreSlot);
        }

        else
        {
            cout << "Unkown command.\n";
        }
    }
    runningScheduler = false;
    if (schedulerThread.joinable())
    {
        schedulerThread.join();
    }
    return 0;
}
