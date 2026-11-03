// Updated 3:00 PM, November 3, 2026 - Kyan, Jek

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
#include <random>
#include <cstdlib>


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
// Random helper (Jek)
// ====================================
mt19937_64 rng(random_device{}());

// gives a random number from low to high (both included)
unsigned long long randomRange(unsigned long long low, unsigned long long high)
{
    uniform_int_distribution<unsigned long long> dist(low, high);
    return dist(rng);
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
    unsigned long long sleepTicks = 0;   // ticks left for SLEEP
    unsigned long long delayLeft = 0;    // busy-wait ticks left (delays-per-exec)
};

// ====================================
// INSTRUCTION ENGINE (Jek)
// ====================================
// genInstructions - makes the instruction list of a process (PRINT, DECLARE, ADD, SUBTRACT, SLEEP, FOR)
// doInstructions  - runs ONE instruction of the process (the one at currInstruction)
vector<instructions> genInstructions(unsigned long long count);
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
    newProcess.instructions = genInstructions(noOfInstructions);
    return newProcess;
}

// ====================================
// PROCESS SCREENS (Jek)
// ====================================
// showSMI       - prints the info and logs of a process (process-smi)
// processScreen - the screen of a process, used by screen -s and screen -r
void showSMI(const Process& process);
void processScreen(int pid, vector<Process>&processes, mutex& processMutex);
// ====================================
// REPORTING AND GENERATION (Jek)
// ====================================
// utilReport     - prints the utilization report (used by screen -ls and report-util)
// genAutoProcess - makes a process named p01, p02, ... for scheduler-start
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
// INSTRUCTION ENGINE functions (Jek)
// ====================================
const string varNames[5] = { "x", "y", "z", "a", "b" };

// keeps a number inside the uint16 range (0 to 65535)
uint16_t clamp16(long long value)
{
    if (value < 0)
    {
        return 0;
    }
    if (value > 65535)
    {
        return 65535;
    }
    return static_cast<uint16_t>(value);
}

bool isNumber(const string& text)
{
    if (text.empty())
    {
        return false;
    }
    for (char c : text)
    {
        if (c < '0' || c > '9')
        {
            return false;
        }
    }
    return true;
}

// gets the value of a number or a variable
// a variable that was not declared yet is declared with a value of 0
uint16_t getValue(Process& process, const string& token)
{
    if (isNumber(token))
    {
        return clamp16(stoll(token));
    }
    if (process.variables.count(token) == 0)
    {
        process.variables[token] = 0;
    }
    return process.variables[token];
}

// a random variable name or a random number
string randomOperand()
{
    if (randomRange(0, 1) == 0)
    {
        return varNames[randomRange(0, 4)];
    }
    return to_string(randomRange(0, 100));
}

// makes a block of instructions that takes exactly "count" steps to run
// depth is how many FOR loops we are inside of (max is 3)
vector<instructions> genBlock(unsigned long long count, int depth)
{
    vector<instructions> block;
    unsigned long long remaining = count;

    while (remaining > 0)
    {
        instructions ins;
        int choice = static_cast<int>(randomRange(0, 5));

        if (choice == 5)
        {
            unsigned int repeats = static_cast<unsigned int>(randomRange(2, 4));

            if (depth >= 3 || remaining < repeats)
            {
                choice = 0;   // cannot make a FOR here, make a PRINT instead
            }
            else
            {
                unsigned long long maxBody = remaining / repeats;
                if (maxBody > 5)
                {
                    maxBody = 5;
                }
                unsigned long long bodySize = randomRange(1, maxBody);

                ins.type = instructionsType::FOR;
                ins.repeats = repeats;
                ins.body = genBlock(bodySize, depth + 1);
                block.push_back(ins);
                remaining -= bodySize * repeats;
                continue;
            }
        }

        if (choice == 0)
        {
            ins.type = instructionsType::PRINT;   // no args = "Hello world from <process_name>!"
        }
        else if (choice == 1)
        {
            ins.type = instructionsType::DECLARE;
            ins.args = { varNames[randomRange(0, 4)], to_string(randomRange(0, 100)) };
        }
        else if (choice == 2)
        {
            ins.type = instructionsType::ADD;
            ins.args = { varNames[randomRange(0, 4)], randomOperand(), randomOperand() };
        }
        else if (choice == 3)
        {
            ins.type = instructionsType::SUBTRACT;
            ins.args = { varNames[randomRange(0, 4)], randomOperand(), randomOperand() };
        }
        else
        {
            ins.type = instructionsType::SLEEP;
            ins.args = { to_string(randomRange(1, 10)) };
        }
        block.push_back(ins);
        remaining--;
    }
    return block;
}

vector<instructions> genInstructions(unsigned long long count)
{
    return genBlock(count, 0);
}

// how many steps a list of instructions takes to run (a FOR counts its body x repeats)
unsigned long long countSteps(const vector<instructions>& list)
{
    unsigned long long total = 0;
    for (const instructions& ins : list)
    {
        if (ins.type == instructionsType::FOR)
        {
            total += countSteps(ins.body) * ins.repeats;
        }
        else
        {
            total++;
        }
    }
    return total;
}

// finds the instruction for step number "step" (0 is the first step)
// FOR loops are walked through here, so the process only needs currInstruction to remember where it is
const instructions* findStep(const vector<instructions>& list, unsigned long long step)
{
    for (const instructions& ins : list)
    {
        if (ins.type == instructionsType::FOR)
        {
            unsigned long long bodySize = countSteps(ins.body);
            unsigned long long loopSize = bodySize * ins.repeats;
            if (step < loopSize)
            {
                return findStep(ins.body, step % bodySize);
            }
            step -= loopSize;
        }
        else
        {
            if (step == 0)
            {
                return &ins;
            }
            step--;
        }
    }
    return nullptr;
}

void doInstructions(Process& process)
{
    const instructions* ins = findStep(process.instructions, process.currInstruction);
    if (ins == nullptr)
    {
        return;
    }

    if (ins->type == instructionsType::PRINT)
    {
        string msg = "Hello world from " + process.pName + "!";
        if (!ins->args.empty())
        {
            msg = ins->args[0];
            if (ins->args.size() > 1)
            {
                msg += to_string(getValue(process, ins->args[1]));
            }
        }
        process.logs.push_back("(" + getTimestamp() + ") Core:" + to_string(process.assignedCore) + " \"" + msg + "\"");
    }
    else if (ins->type == instructionsType::DECLARE)
    {
        process.variables[ins->args[0]] = getValue(process, ins->args[1]);
    }
    else if (ins->type == instructionsType::ADD)
    {
        long long result = static_cast<long long>(getValue(process, ins->args[1])) + getValue(process, ins->args[2]);
        process.variables[ins->args[0]] = clamp16(result);
    }
    else if (ins->type == instructionsType::SUBTRACT)
    {
        long long result = static_cast<long long>(getValue(process, ins->args[1])) - getValue(process, ins->args[2]);
        process.variables[ins->args[0]] = clamp16(result);
    }
    else if (ins->type == instructionsType::SLEEP)
    {
        unsigned long long ticks = stoull(ins->args[0]);
        if (ticks > 255)
        {
            ticks = 255;   // SLEEP takes a uint8
        }
        if (ticks > 0)
        {
            process.sleepTicks = ticks;
            process.pState = "SLEEPING";   // cpuSetup will free the core
        }
    }
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

// counts down the sleeping processes, the ones that are done go back to the ready queue
void wakeSleepers(vector<Process>& processes, queue<int>& readyQueue)
{
    for (Process& process : processes)
    {
        if (process.pState != "SLEEPING")
        {
            continue;
        }
        if (process.sleepTicks > 0)
        {
            process.sleepTicks--;
        }
        if (process.sleepTicks == 0)
        {
            process.pState = "READY";
            readyQueue.push(process.pID);
        }
    }
}

void cpuSetup(int cID, vector<Process>& processes, vector<int>& coreSlot, unsigned long long delays)
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
        // delays-per-exec: busy waiting, the process stays in the core and does nothing
        if (process.delayLeft > 0)
        {
            process.delayLeft--;
            return;
        }

        if (process.currInstruction < process.totalInstructions)
        {
            doInstructions(process);   // run the instruction first, then count it
            process.currInstruction++;
            process.delayLeft = delays;
            //cout << "[Core: " << cID << "] " << process.pName << " executed " << process.currInstruction << "/" << process.totalInstructions << "\n";
        }

        if (process.currInstruction >= process.totalInstructions)
        {
            process.pState = "FINISHED";
            process.finishTime = getTimestamp();
            process.assignedCore = -1;
            process.instructions.clear();           // not needed anymore, frees memory
            process.instructions.shrink_to_fit();

            coreSlot[cID] = -1;
            //cout << "Process " << process.pName << " has FINISHED!\n";
            //cout << "Core " << cID << " is now AVAILABLE\n";
        }
        else if (process.pState == "SLEEPING")
        {
            // SLEEP was just executed, the process leaves the core
            process.assignedCore = -1;
            coreSlot[cID] = -1;
        }
        return;
    }

}
// ====================================
// Process Listing (Kyan)
// ====================================
// this is the old screenLS body, now writes to any stream (console or file)
void utilReport(ostream& output, const vector<Process>& processes, const vector<int>& coreSlot)
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
    output << "\nCPU Utilization: " << fixed << setprecision(2) << cpuUtil << "%\n";
    output << "Cores Used: " << coresUtilized << "\n";
    output << "Cores Available: " << coresReady << "\n";

    output << "\n--------------------------------------\n";
    output << "Running processes:\n";

    for (const Process& p : processes)
    {
        if (p.pState == "FINISHED")
        {
            continue;
        }
        output << p.pName << "  (" << (p.startTime.empty() ? "not started yet" : p.startTime) << ")  ";
        if (p.pState == "RUNNING")
        {
            output << "Core: " << p.assignedCore;
        }
        else
        {
            output << p.pState;   // READY or SLEEPING
        }
        output << "  " << p.currInstruction << " / " << p.totalInstructions << "\n";
    }

    output << "\nFinished processes:\n";

    for (const Process& p : processes)
    {
        if (p.pState == "FINISHED")
        {
            output << p.pName << "  (" << p.finishTime << ")  Finished  " << p.currInstruction << " / " << p.totalInstructions << "\n";
        }
    }
    output << "--------------------------------------\n\n";
}

void screenLS(const vector<Process>& processes, const vector<int>& coreSlot)
{
    utilReport(cout, processes, coreSlot);   // same output as report-util, but on the console
}

// ====================================
// Process generation (Jek)
// ====================================
// makes a process named p01, p02, ... with a random number of instructions (min-ins to max-ins)
Process genAutoProcess(int pid, const Config& config)
{
    string name = "p";
    if (pid < 10)
    {
        name += "0";
    }
    name += to_string(pid);
    return createProcess(pid, name, randomRange(config.minIns, config.maxIns));
}

// ====================================
// PROCESS SCREENS functions (Jek)
// ====================================
void clearScreen()
{
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void printHeader()
{
    cout << "====================================\n";
    cout << "       CSOPESY OS EMULATOR\n";
    cout << "====================================\n";
}

// looks for a process using its ID (the caller must lock processMutex first)
Process* findProcess(vector<Process>& processes, int pid)
{
    for (Process& process : processes)
    {
        if (process.pID == pid)
        {
            return &process;
        }
    }
    return nullptr;
}

void showSMI(const Process& process)
{
    cout << "\nProcess name: " << process.pName << "\n";
    cout << "ID: " << process.pID << "\n";
    cout << "Logs:\n";
    for (const string& log : process.logs)
    {
        cout << log << "\n";
    }
    cout << "\n";

    if (process.pState == "FINISHED")
    {
        cout << "Finished!\n";
    }
    else
    {
        cout << "Current instruction line: " << process.currInstruction << "\n";
        cout << "Lines of code: " << process.totalInstructions << "\n";
    }
    cout << "\n";
}

void printProcessInfo(int pid, vector<Process>& processes, mutex& processMutex)
{
    lock_guard<mutex> lock(processMutex);
    Process* process = findProcess(processes, pid);
    if (process != nullptr)
    {
        showSMI(*process);
    }
}

void processScreen(int pid, vector<Process>& processes, mutex& processMutex)
{
    clearScreen();
    printProcessInfo(pid, processes, processMutex);

    string command;
    while (true)
    {
        cout << "root:\\> ";
        if (!getline(cin, command))
        {
            return;
        }

        if (command == "exit")
        {
            return;
        }
        else if (command == "process-smi")
        {
            printProcessInfo(pid, processes, processMutex);
        }
        else
        {
            cout << "Unknown command. Use process-smi or exit.\n";
        }
    }
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
    atomic<bool> generating{ false };      // true while scheduler-start is active
    unsigned long long cpuCycles = 0;      // the CPU tick counter
    thread schedulerThread;

    printHeader();   // banner moved into printHeader()
    std::cout << "System startup successful!\n\n";


    while (true)
    {
        cout << "root:\\> ";
        if (!getline(cin, command))
        {
            break;   // stop when input ends
        }

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

                        // one loop = one CPU tick
                        cpuCycles++;
                        if (generating && cpuCycles % config.processFreq == 0)
                        {
                            Process autoProcess = genAutoProcess(nextpID++, config);
                            processes.push_back(autoProcess);
                            readyQueue.push(autoProcess.pID);
                        }
                        wakeSleepers(processes, readyQueue);

                        readyProcesses(processes, readyQueue, coreSlot);
                        for (int c = 0; c < static_cast<int>(coreSlot.size());
                            c++)
                        {
                            int pid = coreSlot[c];
                            if (pid == -1)
                            {
                                continue;
                            }
                            cpuSetup(c, processes, coreSlot, config.delays);   // passes delays-per-exec

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
        else if (command == "screen -s" || command == "screen -r")
        {
            cout << "Usage: screen -s <process name> | screen -r <process name> | screen -ls\n";
        }
        else if (command.rfind("screen -s ", 0) == 0)
        {
            string pName = command.substr(10);
            if (pName.empty())
            {
                cout << "Error: Process name is empty!\n";
                continue;
            }

            int newPid = -1;
            {
                lock_guard<mutex> lock(processMutex);

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
                Process newProcess = createProcess(nextpID++, pName, randomRange(config.minIns, config.maxIns));
                processes.push_back(newProcess);
                readyQueue.push(newProcess.pID);
                newPid = newProcess.pID;
            }   // lock is released here, so the screen and the scheduler can both use the processes

            processScreen(newPid, processes, processMutex);
            clearScreen();
            printHeader();
        }
        else if (command.rfind("screen -r ", 0) == 0)
        {
            string pName = command.substr(10);
            int foundPid = -1;
            {
                lock_guard<mutex> lock(processMutex);
                for (const Process& process : processes)
                {
                    if (process.pName == pName && process.pState != "FINISHED")
                    {
                        foundPid = process.pID;
                        break;
                    }
                }
            }

            if (foundPid == -1)
            {
                cout << "Process " << pName << " not found.\n";
            }
            else
            {
                processScreen(foundPid, processes, processMutex);
                clearScreen();
                printHeader();
            }
        }
        else if (command == "screen -ls")
        {
            lock_guard<mutex> lock(processMutex);
            screenLS(processes, coreSlot);
        }

        else if (command == "scheduler-start")
        {
            if (generating)
            {
                cout << "Scheduler is already generating processes!\n";
            }
            else
            {
                generating = true;
                cout << "Scheduler started: a new process every " << config.processFreq << " CPU cycles.\n";
            }
        }
        else if (command == "scheduler-stop")
        {
            if (!generating)
            {
                cout << "Scheduler is not generating processes!\n";
            }
            else
            {
                generating = false;
                cout << "Scheduler stopped: no more dummy processes will be generated.\n";
            }
        }
        else if (command == "report-util")
        {
            lock_guard<mutex> lock(processMutex);
            ofstream reportFile("csopesy-log.txt");
            if (!reportFile.is_open())
            {
                cout << "Error: could not create csopesy-log.txt\n";
                continue;
            }
            utilReport(reportFile, processes, coreSlot);
            reportFile.close();
            cout << "Report generated at csopesy-log.txt!\n";
        }
        else
        {
            cout << "Unknown command.\n";   // fixed typo ("Unkown")
        }
    }
    runningScheduler = false;
    if (schedulerThread.joinable())
    {
        schedulerThread.join();
    }
    return 0;
}