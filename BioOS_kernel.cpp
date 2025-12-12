#include <iostream>
#include <vector>
#include <map>
#include <memory>
#include <thread>
#include <mutex>
#include <queue>
#include <algorithm>
#include <cmath>
#include <chrono>

// ============================================================================
// CORE DATA STRUCTURES
// ============================================================================

enum class ProcessState { READY, RUNNING, BLOCKED, TERMINATED };
enum class EventType { 
    CELL_DIVISION, GENE_EXPRESSION, PROTEIN_SYNTHESIS, 
    SIGNAL_RECEPTION, APOPTOSIS, MUTATION 
};

// Gene structure
struct Gene {
    std::string name;
    std::string sequence;
    float expression_level = 0.0f;
    
    Gene(const std::string& n, const std::string& seq) 
        : name(n), sequence(seq) {}
    
    float express(float factor = 1.0f) {
        expression_level = std::min(1.0f, expression_level + (0.1f * factor));
        return expression_level;
    }
};

// Protein structure
struct Protein {
    std::string name;
    std::string origin_gene;
    float concentration = 0.0f;
    float half_life = 10.0f;
    
    Protein(const std::string& n, const std::string& g) 
        : name(n), origin_gene(g) {}
    
    void degrade() {
        concentration *= (1.0f - (1.0f / half_life));
    }
};

// Biological event
struct BiologicalEvent {
    double timestamp;
    EventType event_type;
    int source_pid;
    
    BiologicalEvent(double t, EventType et, int pid)
        : timestamp(t), event_type(et), source_pid(pid) {}
    
    bool operator<(const BiologicalEvent& other) const {
        return timestamp > other.timestamp; // Min-heap
    }
};

// Biological process (analogous to OS process)
class BioProcess {
public:
    int pid;
    std::string name;
    ProcessState state = ProcessState::READY;
    std::map<std::string, Gene> genome;
    std::map<std::string, Protein> proteins;
    float energy = 100.0f;
    float age = 0.0f;
    int priority = 5;
    
    BioProcess(int id, const std::string& n) 
        : pid(id), name(n) {}
    
    void update(float delta_time = 1.0f) {
        age += delta_time;
        energy = std::max(0.0f, energy - delta_time * 0.5f);
        
        for (auto& [key, protein] : proteins) {
            protein.degrade();
        }
    }
    
    void add_gene(const Gene& gene) {
        genome[gene.name] = gene;
    }
    
    void express_gene(const std::string& gene_name, float factor = 1.0f) {
        if (genome.find(gene_name) != genome.end()) {
            genome[gene_name].express(factor);
        }
    }
};

// ============================================================================
// MEMORY MANAGEMENT SUBSYSTEM
// ============================================================================

class BiologicalMemory {
private:
    double total_capacity;
    double free_space;
    std::map<int, double> allocated;
    std::mutex mem_lock;
    
public:
    BiologicalMemory(double capacity = 10000.0) 
        : total_capacity(capacity), free_space(capacity) {}
    
    bool allocate(int entity_id, double size) {
        std::lock_guard<std::mutex> lock(mem_lock);
        if (free_space >= size) {
            allocated[entity_id] = size;
            free_space -= size;
            return true;
        }
        return false;
    }
    
    bool deallocate(int entity_id) {
        std::lock_guard<std::mutex> lock(mem_lock);
        if (allocated.find(entity_id) != allocated.end()) {
            free_space += allocated[entity_id];
            allocated.erase(entity_id);
            return true;
        }
        return false;
    }
    
    double get_usage_percent() const {
        return (1.0 - free_space / total_capacity) * 100.0;
    }
    
    double get_free_space() const {
        return free_space;
    }
};

// ============================================================================
// PROCESS SCHEDULER
// ============================================================================

class ProcessScheduler {
private:
    std::map<int, std::shared_ptr<BioProcess>> processes;
    std::vector<std::shared_ptr<BioProcess>> ready_queue;
    int pid_counter = 0;
    std::mutex scheduler_lock;
    
public:
    int create_process(const std::string& name) {
        std::lock_guard<std::mutex> lock(scheduler_lock);
        int pid = pid_counter++;
        auto process = std::make_shared<BioProcess>(pid, name);
        processes[pid] = process;
        ready_queue.push_back(process);
        return pid;
    }
    
    std::shared_ptr<BioProcess> schedule() {
        std::lock_guard<std::mutex> lock(scheduler_lock);
        if (ready_queue.empty()) return nullptr;
        
        std::sort(ready_queue.begin(), ready_queue.end(),
            [](const auto& a, const auto& b) { return a->priority < b->priority; });
        
        auto process = ready_queue.front();
        ready_queue.erase(ready_queue.begin());
        return process;
    }
    
    bool terminate_process(int pid) {
        std::lock_guard<std::mutex> lock(scheduler_lock);
        if (processes.find(pid) != processes.end()) {
            processes[pid]->state = ProcessState::TERMINATED;
            ready_queue.erase(
                std::remove_if(ready_queue.begin(), ready_queue.end(),
                    [pid](const auto& p) { return p->pid == pid; }),
                ready_queue.end());
            return true;
        }
        return false;
    }
    
    std::shared_ptr<BioProcess> get_process(int pid) {
        std::lock_guard<std::mutex> lock(scheduler_lock);
        if (processes.find(pid) != processes.end()) {
            return processes[pid];
        }
        return nullptr;
    }
    
    size_t get_process_count() const {
        return processes.size();
    }
    
    std::vector<std::shared_ptr<BioProcess>> get_all_processes() {
        std::lock_guard<std::mutex> lock(scheduler_lock);
        std::vector<std::shared_ptr<BioProcess>> result;
        for (const auto& [pid, process] : processes) {
            if (process->state != ProcessState::TERMINATED) {
                result.push_back(process);
            }
        }
        return result;
    }
};

// ============================================================================
// EVENT MANAGEMENT SYSTEM
// ============================================================================

class EventManager {
private:
    std::priority_queue<BiologicalEvent> event_queue;
    std::map<EventType, std::vector<std::function<void(const BiologicalEvent&)>>> handlers;
    std::mutex event_lock;
    
public:
    void subscribe(EventType event_type, 
                   std::function<void(const BiologicalEvent&)> handler) {
        std::lock_guard<std::mutex> lock(event_lock);
        handlers[event_type].push_back(handler);
    }
    
    void emit(const BiologicalEvent& event) {
        std::lock_guard<std::mutex> lock(event_lock);
        event_queue.push(event);
    }
    
    void process_events(double current_time) {
        std::lock_guard<std::mutex> lock(event_lock);
        while (!event_queue.empty() && event_queue.top().timestamp <= current_time) {
            BiologicalEvent event = event_queue.top();
            event_queue.pop();
            
            if (handlers.find(event.event_type) != handlers.end()) {
                for (auto& handler : handlers[event.event_type]) {
                    handler(event);
                }
            }
        }
    }
};

// ============================================================================
// BIOLOGICAL OPERATING SYSTEM KERNEL
// ============================================================================

class BioOS {
private:
    ProcessScheduler scheduler;
    BiologicalMemory memory;
    EventManager event_manager;
    double time_step;
    double current_time = 0.0;
    bool running = false;
    std::mutex kernel_lock;
    
public:
    BioOS(double ts = 0.1) : time_step(ts), memory(10000.0) {}
    
    void boot() {
        std::cout << "=== BioOS Booting ===" << std::endl;
        std::cout << "Kernel Version: 1.0.0 (C++ High-Performance)" << std::endl;
        std::cout << "Memory Capacity: 10000 units" << std::endl;
        std::cout << "Time Step: " << time_step << "ms" << std::endl;
        std::cout << "System Ready\n" << std::endl;
    }
    
    int create_organism(const std::string& name, const std::vector<Gene>& genome_genes) {
        int pid = scheduler.create_process(name);
        auto process = scheduler.get_process(pid);
        
        for (const auto& gene : genome_genes) {
            process->add_gene(gene);
        }
        
        memory.allocate(pid, 100.0);
        return pid;
    }
    
    void run_tick() {
        current_time += time_step;
        event_manager.process_events(current_time);
        
        auto processes = scheduler.get_all_processes();
        for (auto& process : processes) {
            if (process->state != ProcessState::TERMINATED) {
                process->state = ProcessState::RUNNING;
                process->update(time_step);
                
                if (process->energy <= 0) {
                    scheduler.terminate_process(process->pid);
                    memory.deallocate(process->pid);
                }
                
                process->state = ProcessState::READY;
            }
        }
    }
    
    void simulate(double duration) {
        boot();
        running = true;
        int ticks = static_cast<int>(duration / time_step);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < ticks && running; ++i) {
            run_tick();
            
            if (i % 100 == 0) {
                std::cout << "Tick " << i << " | Time: " << current_time 
                         << "s | Memory: " << memory.get_usage_percent() << "%" << std::endl;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();
        
        shutdown(elapsed);
    }
    
    void shutdown(long elapsed_ms = 0) {
        std::cout << "\n=== BioOS Shutdown ===" << std::endl;
        std::cout << "Simulation Time: " << current_time << "s" << std::endl;
        std::cout << "Real Time: " << elapsed_ms << "ms" << std::endl;
        std::cout << "Final Memory Usage: " << memory.get_usage_percent() << "%" << std::endl;
        std::cout << "Processes Created: " << scheduler.get_process_count() << std::endl;
        running = false;
    }
    
    double get_current_time() const { return current_time; }
};

// ============================================================================
// EXAMPLE USAGE
// ============================================================================

int main() {
    BioOS os(0.1);
    
    // Create organism genomes
    std::vector<Gene> genome1 = {
        Gene("GROWTH_GENE", "ATCGATCGATCG"),
        Gene("ENERGY_GENE", "GCTAGCTAGCTA")
    };
    
    std::vector<Gene> genome2 = {
        Gene("SURVIVAL_GENE", "TACGTACGTACG"),
        Gene("REPRODUCTION_GENE", "AAAAAATTTTTT")
    };
    
    // Create organisms
    int pid1 = os.create_organism("Organism_1", genome1);
    int pid2 = os.create_organism("Organism_2", genome2);
    
    std::cout << "Created organisms with PIDs: " << pid1 << ", " << pid2 << "\n" << std::endl;
    
    // Run simulation for 10 seconds
    os.simulate(10.0);
    
    return 0;
}
